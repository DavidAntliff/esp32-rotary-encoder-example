# ESP32 Rotary Encoder

This project is a simple demonstration of using a rotary encoder to track relative position, with a reset switch to reset the position.

Debouncing is performed by a state machine that ensures correct tracking of direction, and emits a directional event only at the resting states.

## Circuit

1. Connect GND to the ESP32 ground reference.
1. Connect + to the ESP32 3.3V output.
1. Connect SW to an ESP32 GPIO.
1. Connect DT (pin B) to an ESP32 GPIO.
1. Connect CLK (pin A) to an ESP32 GPIO.
1. Use `make menuconfig` to configure the correct ESP32 GPIOs according to the previous connections.

1. Optional: connect a 100nF capacitor between DT and GND.
1. Optional: connect a 100nF capacitor between CLK and GND.

It can also be illustrative to connect LEDs between 3.3V and each of the pins SW, DT and CLK, such that they will light when those outputs are low.

## Acknowledgements

This code is based on the Arduino state machine design and code by [Ben Buxton](https://github.com/buxtronix/arduino/tree/master/libraries/Rotary), modified to support the ESP32.

## License

Ben Buxton's original code is licensed under the GNU GPL Version 3, therefore this code is also licensed under the GNU GPL Version 3.

## Notes

The state machine is designed to operate with inverted values of the rotary encoder's A and B outputs. This is because pull-ups are used to read the outputs.

Power should be supplied to the rotary encoder on pin + otherwise the transitional levels are floating, which causes multiple interrupts to fire on the ESP32 input.

The KY-040 rotary encoder (as mentioned [here](http://henrysbench.capnfatz.com/henrys-bench/arduino-sensors-and-input/keyes-ky-040-arduino-rotary-encoder-user-manual/)) is quite noisy and although this code does a fairly good job, there are some occasional missed events. This is mitigated with some analogue filtering on the A and B outputs of the rotary encoder. For example, a pair of 100nF capacitors from ground to pin A and B works well enough for me.