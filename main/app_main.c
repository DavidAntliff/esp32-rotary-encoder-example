#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define ONBOARD_LED_GPIO (CONFIG_ONBOARD_LED_GPIO)
#define ROT_ENC_A_GPIO (CONFIG_ROT_ENC_A_GPIO)
#define ROT_ENC_B_GPIO (CONFIG_ROT_ENC_B_GPIO)
#define ROT_ENC_SW_GPIO (CONFIG_ROT_ENC_SW_GPIO)
#define TAG "app"

// if defined, show direct state of Rotary Encoder pins
//#define DIRECT 1

// Values returned by 'process'
// No complete step yet.
#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Anti-clockwise step.
#define DIR_CCW 0x20

typedef struct {
    unsigned char state;
    unsigned char pin_a;
    unsigned char pin_b;
} rotary_encoder_t;

// Based on https://github.com/buxtronix/arduino/tree/master/libraries/Rotary
// Original header:

/* Rotary encoder handler for arduino. v1.1
 *
 * Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3.
 * Contact: bb@cactii.net
 *
 * A typical mechanical rotary encoder emits a two bit gray code
 * on 3 output pins. Every step in the output (often accompanied
 * by a physical 'click') generates a specific sequence of output
 * codes on the pins.
 *
 * There are 3 pins used for the rotary encoding - one common and
 * two 'bit' pins.
 *
 * The following is the typical sequence of code on the output when
 * moving from one step to the next:
 *
 *   Position   Bit1   Bit2
 *   ----------------------
 *     Step1     0      0
 *      1/4      1      0
 *      1/2      1      1
 *      3/4      0      1
 *     Step2     0      0
 *
 * From this table, we can see that when moving from one 'click' to
 * the next, there are 4 changes in the output code.
 *
 * - From an initial 0 - 0, Bit1 goes high, Bit0 stays low.
 * - Then both bits are high, halfway through the step.
 * - Then Bit1 goes low, but Bit2 stays high.
 * - Finally at the end of the step, both bits return to 0.
 *
 * Detecting the direction is easy - the table simply goes in the other
 * direction (read up instead of down).
 *
 * To decode this, we use a simple state machine. Every time the output
 * code changes, it follows state, until finally a full steps worth of
 * code is received (in the correct order). At the final 0-0, it returns
 * a value indicating a step in one direction or the other.
 *
 * It's also possible to use 'half-step' mode. This just emits an event
 * at both the 0-0 and 1-1 positions. This might be useful for some
 * encoders where you want to detect all positions.
 *
 * If an invalid state happens (for example we go from '0-1' straight
 * to '1-0'), the state machine resets to the start until 0-0 and the
 * next valid codes occur.
 *
 * The biggest advantage of using a state machine over other algorithms
 * is that this has inherent debounce built in. Other algorithms emit spurious
 * output with switch bounce, but this one will simply flip between
 * sub-states until the bounce settles, then continue along the state
 * machine.
 * A side effect of debounce is that fast rotations can cause steps to
 * be skipped. By not requiring debounce, fast rotations can be accurately
 * measured.
 * Another advantage is the ability to properly handle bad state, such
 * as due to EMI, etc.
 * It is also a lot simpler than others - a static state table and less
 * than 10 lines of logic.
 */

/*
 * The below state table has, for each state (row), the new state
 * to set based on the next encoder output. From left to right in,
 * the table, the encoder outputs are 00, 01, 10, 11, and the value
 * in that position is the new state to set.
 */

#define R_START 0x0

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#  define R_CCW_BEGIN 0x1
#  define R_CW_BEGIN 0x2
#  define R_START_M 0x3
#  define R_CW_BEGIN_M 0x4
#  define R_CCW_BEGIN_M 0x5
const uint8_t ttable[6][4] = {
    // R_START (00)
    {R_START_M,            R_CW_BEGIN,     R_CCW_BEGIN,  R_START},
    // R_CCW_BEGIN
    {R_START_M | DIR_CCW,  R_START,        R_CCW_BEGIN,  R_START},
    // R_CW_BEGIN
    {R_START_M | DIR_CW,   R_CW_BEGIN,     R_START,      R_START},
    // R_START_M (11)
    {R_START_M,            R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},
    // R_CW_BEGIN_M
    {R_START_M,            R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},
    // R_CCW_BEGIN_M
    {R_START_M,            R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW},
};
#else
// Use the full-step state table (emits a code at 00 only)
#  define R_CW_FINAL 0x1
#  define R_CW_BEGIN 0x2
#  define R_CW_NEXT 0x3
#  define R_CCW_BEGIN 0x4
#  define R_CCW_FINAL 0x5
#  define R_CCW_NEXT 0x6

const uint8_t ttable[7][4] = {
    // R_START
    {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
    // R_CW_FINAL
    {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
    // R_CW_BEGIN
    {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
    // R_CW_NEXT
    {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
    // R_CCW_BEGIN
    {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
    // R_CCW_FINAL
    {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
    // R_CCW_NEXT
    {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};
#endif

// GPIO direction and pull-ups should be set externally
static rotary_encoder_t rotary_init(gpio_num_t pin_a, gpio_num_t pin_b)
{
    rotary_encoder_t rotenc;
    rotenc.pin_a = pin_a;
    rotenc.pin_b = pin_b;
    rotenc.state = R_START;
    return rotenc;
}

uint8_t rotary_process(rotary_encoder_t * rotenc)
{
    uint8_t event = 0;
    if (rotenc != NULL)
    {
        // Get state of input pins.
        unsigned char pin_state = (gpio_get_level(rotenc->pin_b) << 1) | gpio_get_level(rotenc->pin_a);

        // Determine new state from the pins and state table.
        rotenc->state = ttable[rotenc->state & 0xf][pin_state];

        // Return emit bits, i.e. the generated event.
        event = rotenc->state & 0x30;
    }
    return event;
}

typedef struct
{
    rotary_encoder_t rotenc;
    int32_t position;
} knob_with_reset_t;

#ifdef DIRECT
static void isr_rotenc_sw(void * args)
{
    int level = gpio_get_level(ROT_ENC_SW_GPIO);
    ESP_EARLY_LOGI(TAG, "SW%d", level);
}

static void isr_rotenc_a(void * args)
{
    int level = gpio_get_level(ROT_ENC_A_GPIO);
    ESP_EARLY_LOGI(TAG, "A%d", level);
}

static void isr_rotenc_b(void * args)
{
    int level = gpio_get_level(ROT_ENC_B_GPIO);
    ESP_EARLY_LOGI(TAG, "B%d", level);
}
#else  // DIRECT
static void isr_rotenc_process(void * args)
{
    ESP_EARLY_LOGD(TAG, "intr");
    knob_with_reset_t * knob = (knob_with_reset_t *)args;
    uint8_t event = rotary_process(&knob->rotenc);
    switch (event)
    {
    case DIR_CW:
        ++knob->position;
        ESP_EARLY_LOGI(TAG, "%d", knob->position);
        break;
    case DIR_CCW:
        --knob->position;
        ESP_EARLY_LOGI(TAG, "%d", knob->position);
        break;
    default:
        break;
    }
}

static void isr_rotenc_sw(void * args)
{
    knob_with_reset_t * knob = (knob_with_reset_t *)args;
    if (gpio_get_level(ROT_ENC_SW_GPIO) == 0)  // inverted
    {
        ESP_EARLY_LOGI(TAG, "reset");
        knob->position = 0;
    }
}
#endif  // DIRECT

static void main_task(void * pvParameter)
{
	gpio_pad_select_gpio(ONBOARD_LED_GPIO);
	gpio_set_direction(ONBOARD_LED_GPIO, GPIO_MODE_OUTPUT);

	gpio_pad_select_gpio(ROT_ENC_SW_GPIO);
	gpio_set_pull_mode(ROT_ENC_SW_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_direction(ROT_ENC_SW_GPIO, GPIO_MODE_INPUT);
    gpio_set_intr_type(ROT_ENC_SW_GPIO, GPIO_INTR_ANYEDGE);

    gpio_pad_select_gpio(ROT_ENC_A_GPIO);
    gpio_set_pull_mode(ROT_ENC_A_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_direction(ROT_ENC_A_GPIO, GPIO_MODE_INPUT);
    gpio_set_intr_type(ROT_ENC_A_GPIO, GPIO_INTR_ANYEDGE);

    gpio_pad_select_gpio(ROT_ENC_B_GPIO);
    gpio_set_pull_mode(ROT_ENC_B_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_direction(ROT_ENC_B_GPIO, GPIO_MODE_INPUT);
    gpio_set_intr_type(ROT_ENC_B_GPIO, GPIO_INTR_ANYEDGE);

    gpio_install_isr_service(0);
#ifdef DIRECT
    gpio_isr_handler_add(ROT_ENC_SW_GPIO, isr_rotenc_sw, NULL);
    gpio_isr_handler_add(ROT_ENC_A_GPIO, isr_rotenc_a, NULL);
    gpio_isr_handler_add(ROT_ENC_B_GPIO, isr_rotenc_b, NULL);
#else  // DIRECT
    knob_with_reset_t knob = { rotary_init(ROT_ENC_A_GPIO, ROT_ENC_B_GPIO), 0 };
    gpio_isr_handler_add(ROT_ENC_A_GPIO, isr_rotenc_process, &knob);
    gpio_isr_handler_add(ROT_ENC_B_GPIO, isr_rotenc_process, &knob);
    gpio_isr_handler_add(ROT_ENC_SW_GPIO, isr_rotenc_sw, &knob);
#endif  // DIRECT

//    int32_t counter = 0;
	while(1)
	{
		gpio_set_level(ONBOARD_LED_GPIO, 0);
		vTaskDelay(1000 / portTICK_RATE_MS);
		gpio_set_level(ONBOARD_LED_GPIO, 1);
		vTaskDelay(1000 / portTICK_RATE_MS);

//	    // polling - not fast enough to catch all states at portTICK_RATE_MS = 10
//	    uint8_t event = rotary_process(&rotenc);
//	    switch (event)
//	    {
//	    case DIR_CW:
//            ++counter;
//            ESP_LOGI(TAG, "%d", counter);
//            break;
//	    case DIR_CCW:
//            --counter;
//            ESP_LOGI(TAG, "%d", counter);
//            break;
//	    default:
//	        break;
//	    }
//
//	    vTaskDelay(1);
	}
}

void app_main()
{
	xTaskCreate(&main_task, "main_task", 2048, NULL, 5, NULL);
}

