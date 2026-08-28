/******************************************************************************
 * The MIT License
 *
 * Copyright (c) 2010 Perry Hung.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *****************************************************************************/

/**
 * @file wirish/include/wirish/io.h
 * @brief Wiring-style pin I/O interface.
 */

#ifndef _WIRISH_IO_H_
#define _WIRISH_IO_H_

//#include <libmaple/libmaple_types.h>
#include <boards.h>

/**
 * Specifies a GPIO pin behavior.
 * @see pinMode()
 */
typedef enum WiringPinMode {
    OUTPUT, /**< Basic digital output: when the pin is HIGH, the
               voltage is held at +3.3v (Vcc) and when it is LOW, it
               is pulled down to ground. */

    OUTPUT_OPEN_DRAIN, /**< In open drain mode, the pin indicates
                          "low" by accepting current flow to ground
                          and "high" by providing increased
                          impedance. An example use would be to
                          connect a pin to a bus line (which is pulled
                          up to a positive voltage by a separate
                          supply through a large resistor). When the
                          pin is high, not much current flows through
                          to ground and the line stays at positive
                          voltage; when the pin is low, the bus
                          "drains" to ground with a small amount of
                          current constantly flowing through the large
                          resistor from the external supply. In this
                          mode, no current is ever actually sourced
                          from the pin. */

    INPUT, /**< Basic digital input. The pin voltage is sampled; when
              it is closer to 3.3v (Vcc) the pin status is high, and
              when it is closer to 0v (ground) it is low. If no
              external circuit is pulling the pin voltage to high or
              low, it will tend to randomly oscillate and be very
              sensitive to noise (e.g., a breath of air across the pin
              might cause the state to flip). */

    INPUT_ANALOG, /**< This is a special mode for when the pin will be
                     used for analog (not digital) reads.  Enables ADC
                     conversion to be performed on the voltage at the
                     pin. */

    INPUT_PULLUP, /**< The state of the pin in this mode is reported
                     the same way as with INPUT, but the pin voltage
                     is gently "pulled up" towards +3.3v. This means
                     the state will be high unless an external device
                     is specifically pulling the pin down to ground,
                     in which case the "gentle" pull up will not
                     affect the state of the input. */

    INPUT_PULLDOWN, /**< The state of the pin in this mode is reported
                       the same way as with INPUT, but the pin voltage
                       is gently "pulled down" towards 0v. This means
                       the state will be low unless an external device
                       is specifically pulling the pin up to 3.3v, in
                       which case the "gentle" pull down will not
                       affect the state of the input. */

    INPUT_FLOATING, /**< Synonym for INPUT. */

    PWM, /**< This is a special mode for when the pin will be used for
            PWM output (a special case of digital output). */

    PWM_OPEN_DRAIN, /**< Like PWM, except that instead of alternating
                       cycles of LOW and HIGH, the voltage on the pin
                       consists of alternating cycles of LOW and
                       floating (disconnected). */
} WiringPinMode;

float ADC_measVDDA();

/**
 * Configure behavior of a GPIO pin.
 *
 * @param pin Number of pin to configure.
 * @param mode Mode corresponding to desired pin behavior.
 * @see WiringPinMode
 */
void pinMode(uint8 pin, WiringPinMode mode);

#define HIGH 0x1
#define LOW  0x0

/**
 * Writes a (analog) value to a pin.  The pin must have its
 * mode set to PWM or PWM_OPEN_DRAIN.
 * on DAC capable pin the voltage is set to sdfg
 * on pwm capable pins a pwm cycle will be applied
 *
 * @param pin Pin to write to.
 * @param duty_cycle8
 * @see pinMode()
 */
void analogWrite(uint8 pin, int duty_cycle8);

#define PVAL_12 A,6

/**
 * Writes a (digital) value to a pin.  The pin must have its
 * mode set to OUTPUT or OUTPUT_OPEN_DRAIN.
 *
 * @param pin Pin to write to.
 * @param value Either LOW (write a 0) or HIGH (write a 1).
 * @see pinMode()
 */
//void digitalWrite(uint8 pin, uint8 value);
#define digitalWriteEx(port,bit,value) (GPIO_BSRR(GPIO_PORT_##port##_BASE) = (uint32_t)(1<<bit)<<(value?0:16))
#define digitalWrite(pin,value)	switch (pin) { \
		default: if (value) gpio_set(PIN_MAP[pin].gpio_pin); else gpio_clear(PIN_MAP[pin].gpio_pin); break; \
		case 0: digitalWriteEx(A,10,value); break; \
		case 1: digitalWriteEx(A,9,value); break; \
		case 2: digitalWriteEx(B,7,value); break; \
		case 3: digitalWriteEx(B,6,value); break; \
		case 5: digitalWriteEx(B,9,value); break; \
		case 6: digitalWriteEx(A,3,value); break; \
		case 10: digitalWriteEx(A,2,value); break; \
		case 11: digitalWriteEx(B,0,value); break; \
		case 12: digitalWriteEx(A,6,value); break; \
		case 13: digitalWriteEx(B,1,value); break; \
		case 14: digitalWriteEx(B,4,value); break; \
		case 15: digitalWriteEx(B,3,value); break; \
		case 16: digitalWriteEx(B,5,value); break; \
		case 17: digitalWriteEx(A,15,value); break; \
		case 18: digitalWriteEx(B,8,value); break; \
		case 19: digitalWriteEx(A,0,value); break; \
		case 20: digitalWriteEx(A,1,value); break; \
		case 21: digitalWriteEx(A,4,value); break; \
		case 22: digitalWriteEx(A,7,value); break; \
		case 24: digitalWriteEx(B,12,value); break; \
		case 25: digitalWriteEx(C,13,value); break; \
		case 26: digitalWriteEx(A,11,value); break; \
		case 27: digitalWriteEx(A,12,value); break; \
		case 28: digitalWriteEx(A,5,value); break; \
		case 29: digitalWriteEx(A,8,value); break; \
		case 30: digitalWriteEx(A,13,value); break; \
		case 31: digitalWriteEx(A,14,value); break; \
		case 32: digitalWriteEx(B,10,value); break; \
		case 33: digitalWriteEx(B,11,value); break; \
		case 34: digitalWriteEx(B,13,value); break; \
		case 35: digitalWriteEx(B,14,value); break; \
		case 36: digitalWriteEx(B,15,value); break; \
	}

/**
 * Toggles the digital value at the given pin.
 *
 * The pin must have its mode set to OUTPUT.
 *
 * @param pin the pin to toggle.  If the pin is HIGH, set it LOW.  If
 * it is LOW, set it HIGH.
 *
 * @see pinMode()
 */
//void togglePin(uint8 pin);
#define togglePinEx(port,bit) (GPIO_BSRR(GPIO_PORT_##port##_BASE) = (uint32_t)(1<<bit)<<((GPIO_ODR(GPIO_PORT_##port##_BASE)&(1<<bit))?16:0))
#define togglePin(pin) switch(pin) { \
		default: gpio_toggle(PIN_MAP[(pin)].gpio_pin); break; \
		case 0: togglePinEx(A,10); break; \
		case 1: togglePinEx(A,9); break; \
		case 2: togglePinEx(B,7); break; \
		case 3: togglePinEx(B,6); break; \
		case 5: togglePinEx(B,9); break; \
		case 6: togglePinEx(A,3); break; \
		case 10: togglePinEx(A,2); break; \
		case 11: togglePinEx(B,0); break; \
		case 12: togglePinEx(A,6); break; \
		case 13: togglePinEx(B,1); break; \
		case 14: togglePinEx(B,4); break; \
		case 15: togglePinEx(B,3); break; \
		case 16: togglePinEx(B,5); break; \
		case 17: togglePinEx(A,15); break; \
		case 18: togglePinEx(B,8); break; \
		case 19: togglePinEx(A,0); break; \
		case 20: togglePinEx(A,1); break; \
		case 21: togglePinEx(A,4); break; \
		case 22: togglePinEx(A,7); break; \
		case 24: togglePinEx(B,12); break; \
		case 25: togglePinEx(C,13); break; \
		case 26: togglePinEx(A,11); break; \
		case 27: togglePinEx(A,12); break; \
		case 28: togglePinEx(A,5); break; \
		case 29: togglePinEx(A,8); break; \
		case 30: togglePinEx(A,13); break; \
		case 31: togglePinEx(A,14); break; \
		case 32: togglePinEx(B,10); break; \
		case 33: togglePinEx(B,11); break; \
		case 34: togglePinEx(B,13); break; \
		case 35: togglePinEx(B,14); break; \
		case 36: togglePinEx(B,15); break; \
	}

/**
 * Read a digital value from a pin. Will deliver unreliable results if ADC
 * capable pin is set to INPUT_ANALOG.
 *
 * @param pin Pin to read from.
 * @return LOW or HIGH.
 * @see pinMode()
 */
uint32 digitalRead(uint8 pin);

/**
 * Read an analog value from pin.  This function blocks during ADC
 * conversion, and has 12 bits of resolution.  The pin must have its
 * mode set to INPUT_ANALOG.
 *
 * @param pin Pin to read from.
 * @return Converted voltage, in the range 0--4095, (i.e. a 12-bit ADC
 *         conversion).
 * @see pinMode()
 */
uint16 analogRead(uint8 pin);

/**
 * Toggle the LED.
 *
 * If the LED is on, turn it off.  If it is off, turn it on.
 *
 * The LED must its mode set to OUTPUT. This can be accomplished
 * portably over all LeafLabs boards by calling pinMode(BOARD_LED_PIN,
 * OUTPUT) before calling this function.
 *
 * @see pinMode()
 */
static inline void toggleLED() {
    togglePin(BOARD_LED_PIN);
}

/**
 * If the button is currently pressed, waits until the button is no
 * longer being pressed, and returns true.  Otherwise, returns false.
 *
 * The button pin must have its mode set to INPUT.  This can be
 * accomplished portably over all LeafLabs boards by calling
 * pinMode(BOARD_BUTTON_PIN, INPUT).
 *
 * @see pinMode()
 */
uint8 isButtonPressed(uint8 pin=BOARD_BUTTON_PIN, uint32 pressedLevel=BOARD_BUTTON_PRESSED_LEVEL);

/**
 * Wait until the button is pressed and released, timing out if no
 * press occurs.
 *
 * The button pin must have its mode set to INPUT.  This can be
 * accomplished portably over all LeafLabs boards by calling
 * pinMode(BOARD_BUTTON_PIN, INPUT).
 *
 * @param timeout_millis Number of milliseconds to wait until the
 * button is pressed.  If timeout_millis is left out (or 0), wait
 * forever.
 *
 * @return true, if the button was pressed; false, if the timeout was
 * reached.
 *
 * @see pinMode()
 */
uint8 waitForButtonPress(uint32 timeout_millis=0);

/**
 * Shift out a byte of data, one bit at a time.
 *
 * This function starts at either the most significant or least
 * significant bit in a byte value, and shifts out each byte in order
 * onto a data pin.  After each bit is written to the data pin, a
 * separate clock pin is pulsed to indicate that the new bit is
 * available.
 *
 * @param dataPin  Pin to shift data out on
 * @param clockPin Pin to pulse after each bit is shifted out
 * @param bitOrder Either MSBFIRST (big-endian) or LSBFIRST (little-endian).
 * @param value    Value to shift out
 */
void shiftOut(uint8 dataPin, uint8 clockPin, uint8 bitOrder, uint8 value);


unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout = 1000000L);

__attribute__((always_inline)) static inline void pinLow(const uint8_t pin, const WiringPinMode mode=OUTPUT)
{
  digitalWrite(pin,LOW);
  pinMode(pin,mode);
  digitalWrite(pin,LOW);
}
__attribute__((always_inline)) static inline void pinHigh(const uint8_t pin, const WiringPinMode mode=OUTPUT)
{
  digitalWrite(pin,HIGH);
  pinMode(pin,mode);
  digitalWrite(pin,HIGH);
}

#endif
