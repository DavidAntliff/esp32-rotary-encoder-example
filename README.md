# ESP32 Rotary Encoder Example

[![Platform: ESP-IDF](https://img.shields.io/badge/ESP--IDF-v3.0%2B-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/)
[![Build Status](https://travis-ci.org/DavidAntliff/esp32-rotary-encoder-example.svg?branch=master)](https://travis-ci.org/DavidAntliff/esp32-rotary-encoder-example)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3+-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

This is a demonstration of using the [esp32-rotary-encoder](https://github.com/DavidAntliff/esp32-rotary-encoder) driver to track the relative position of an [incremental](https://en.wikipedia.org/wiki/Rotary_encoder#Incremental) rotary encoder.

It is written and tested for v3.0-v3.2 of the [ESP-IDF](https://github.com/espressif/esp-idf) environment, using the xtensa-esp32-elf toolchain (gcc version 5.2.0). It may or may not work with older or newer versions.

Ensure that submodules are cloned:

    $ git clone --recursive https://github.com/DavidAntliff/esp32-rotary-encoder-example.git

Build the application with:

    $ cd esp32-rotary-encoder-example.git
    $ make menuconfig    # set your serial configuration and the Rotary Encoder GPIO - see Circuit below
    $ make flash monitor

Typically this kind of rotary encoder has at least four pins - +ve supply, ground, and two quadrature pins labelled CLK (or A) and DT (or B). The device used for testing is the one supplied with the KY-040 rotary encoder, as mentioned [here](http://henrysbench.capnfatz.com/henrys-bench/arduino-sensors-and-input/keyes-ky-040-arduino-rotary-encoder-user-manual/).

Debouncing is performed by the driver using a state machine that ensures correct tracking of direction, and emits a directional event only at the resting states.

## Dependencies

This application makes use of the following components (included as submodules):

 * components/[esp32-rotary-encoder](https://github.com/DavidAntliff/esp32-rotary-encoder)

## Circuit

1. Connect GND to the ESP32 ground reference.
1. Connect + to the ESP32 3.3V output.
1. Connect DT (pin B) to an ESP32 GPIO.
1. Connect CLK (pin A) to an ESP32 GPIO.
1. Use `make menuconfig` to configure the correct ESP32 GPIOs according to the previous connections.

1. Optional: connect a 100nF capacitor between DT and GND for electrical smoothing.
1. Optional: connect a 100nF capacitor between CLK and GND for electrical smoothing.

It can also be illustrative to connect LEDs between 3.3V and each of the pins DT and CLK, such that they will light when those outputs are low.

## Source Code

The source is available from [GitHub](https://www.github.com/DavidAntliff/esp32-rotary-encoder-example).

## License

The code in this project is licensed under the GNU GPL Version 3, or (at your option) any later version. - see [LICENSE](LICENSE) for details.

## Notes

The state machine is designed to operate with inverted values of the rotary encoder's A and B outputs. This is because pull-ups are used to read the outputs.

Power should be supplied to the rotary encoder on pin + otherwise the transitional levels are floating, which causes multiple interrupts to fire on the ESP32 input.

The KY-040 rotary encoder is quite noisy and although this code does a fairly good job, there are some occasional missed events. This is mitigated with some analogue filtering on the A and B outputs of the rotary encoder. For example, a pair of 100nF capacitors from ground to pin A and B works well enough for me.

This project also works with higher-resolution rotary encoders such as the [LPD3806](https://www.codrey.com/electronic-circuits/paupers-rotary-encoder/). Note that (at least for the device I have), you need to supply more than 5V to the rotary encoder because of an internal linear regulator - I have had success with 9V.
