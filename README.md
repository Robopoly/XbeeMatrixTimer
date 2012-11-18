# XbeeMatrixTimer

![XbeeMatrixTimer](http://i.imgur.com/NP3hB.png)

## Timer for events
This code is to be used with the XbeeMatrix project and the arch during events. The arch sends the start trigger and the timer starts to count up to 1 minute and 30 seconds after which it stops. The timer stops when one of the bumbers is hit. It is reset when the arch is reset.

**NOTE: The chip is an ATmega164P on a PRisme2 board!**

## Connection
Bumpers trigger external interrupts on the PRisme connected to the chronometer on pins PD2 and PD3 (rising edge trigger).

The start condition is triggered by the arch, the arch sets its PC0 pin to 0 when nothing's going on and at 1 when the competition starts. So a rising edge signals the start of counting, either bumper triggers the stop signal and a falling edge of the arch (PC0) signals a reset signal to the chronometer on the external inperrupt pin PB2.

Additionnaly to know which side won a crown is shown on the winner side (left or right) of the conter digits.

Connect pins as follows:

    XMT   | Arch | Bumper 1 | Bumper 2
    ------+------+----------+---------
    PB2   | PC0  |          |
    PD2   |      | x        |
    PD3   |      |          | x

Bumpers trigger the stop signal on the *rising* edge, connect the power accordingly (signal at 0V when not triggered).