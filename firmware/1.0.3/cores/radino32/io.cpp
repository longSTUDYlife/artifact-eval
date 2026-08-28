#include "io.h"
#include "pwm.h"
#include <wirish_time.h>
#include <stm32/l1/gpio.h>
#include <stm32/l1/tim.h>
#include <stm32/l1/adc.h>
#include <stm32/l1/dac.h>

float ADC_measVDDA()
{
  //Enable ADC
  adc_enable();
  adc_enable_ts_vref();
  
  uint16_t adc_dr;
  float adc_f1, adc_f2, adc_scale;
  
  //Measure VCC
  // read internal reference voltage
  // single conversion
  adc_set_regular_channel(ADC_IN_VREFINT);
  adc_set_resolution(12);
  adc_set_sampling(ADC_IN_VREFINT, 384);
  //dbgSerial.println(ADC_SQR5, HEX);
  // need to wait after channel change
  adc_wait_for_regular_ready();
  adc_start_regular_conversion();
  // wait for end of conversion
  while (!(ADC_SR & ADC_SR_EOC));
  
  //dbgSerial.println(ADC_SR, HEX);
  //dbgSerial.println(ADC_CCR, HEX);
  //dbgSerial.println(ADC_SQR5, HEX);
  adc_dr = ADC_DR;
  //dbgSerial.println(dr, HEX);
  //dbgSerial.print("V DDA: ");
  adc_f1 = (float) adc_dr;
  adc_f2 = (float) ADC_H_VREFINT_CAL;
  adc_scale = adc_f2 / adc_f1;
  //dbgSerial.println(3 * adc_scale);
  //Disable ADC
  adc_disable_ts_vref();
  adc_disable();
  
  return 3 * adc_scale;
}

void pinMode(uint8 pin, WiringPinMode mode) {
    gpio_otype_t outputMode = GPIO_PUSHPULL;
    gpio_pupd_t pupdMode = GPIO_NOPUPD;
    bool output = false;

    switch(mode) {
		  case OUTPUT_OPEN_DRAIN:
		      outputMode = GPIO_OPENDRAIN;
		  case OUTPUT:
		      //gpio_config_output(outputMode, GPIO_2MHZ, GPIO_NOPUPD, PIN_MAP[pin].gpio_pin);
		      gpio_config_output(outputMode, GPIO_40MHZ, GPIO_NOPUPD, PIN_MAP[pin].gpio_pin);
		      gpio_clear(PIN_MAP[pin].gpio_pin);
		      return;

	    case PWM_OPEN_DRAIN:
		outputMode = GPIO_OPENDRAIN;
	    case PWM:
	        if (pin == BOARD_DAC_PIN) {
	            gpio_config_analog(PIN_MAP[pin].gpio_pin);
	            /* Enable DAC channel1. */
	            dac_enable(DAC_CH1);
	            /* Wait tWAKEUP. */
	            delayMicroseconds(DAC_T_WAKEUP);
	        } else if (PIN_MAP[pin].timer_device != TIMx) { //port kann PWM
	            gpio_config_altfn(PIN_MAP[pin].altfn, outputMode, GPIO_2MHZ, GPIO_NOPUPD, PIN_MAP[pin].gpio_pin);
	            //gpio_config_altfn(PIN_MAP[pin].altfn, outputMode, GPIO_40MHZ, GPIO_NOPUPD, PIN_MAP[pin].gpio_pin);
	        } else {
	            gpio_config_output(outputMode, GPIO_2MHZ, GPIO_NOPUPD, PIN_MAP[pin].gpio_pin);
	            //gpio_config_output(outputMode, GPIO_40MHZ, GPIO_NOPUPD, PIN_MAP[pin].gpio_pin);
	            gpio_clear(PIN_MAP[pin].gpio_pin);
	        }
		return;

	    case INPUT:
	    case INPUT_FLOATING:
	        pupdMode = GPIO_FLOAT;
	        break;
	    case INPUT_PULLUP:
	        pupdMode = GPIO_PULLUP;
	        break;
	    case INPUT_PULLDOWN:
	        pupdMode = GPIO_PULLDOWN;
	        break;

	case INPUT_ANALOG:
		if (PIN_MAP[pin].adc_channel != ADCx) //port kann ADC
			gpio_config_analog(PIN_MAP[pin].gpio_pin);
			else
			gpio_config_input(GPIO_FLOAT, PIN_MAP[pin].gpio_pin);
		return;

	    default:
	        return;
    }
    gpio_config_input(pupdMode, PIN_MAP[pin].gpio_pin);
}

/*
void togglePin(uint8 pin) {
    if (PIN_MAP[pin].timer_device != TIMx)
        analogWrite(pin, tim_get_capture_compare_value(PIN_MAP[pin].timer_channel) ? 0 : 255);
    else
        gpio_toggle(PIN_MAP[pin].gpio_pin);
}
*/

/*
void digitalWrite(uint8 pin, uint8 value)
{
	if (PIN_MAP[pin].timer_device != TIMx) //port kann PWM
		analogWrite(pin, value ? 255 : 0);
	else if (value)
		gpio_set(PIN_MAP[pin].gpio_pin);
	else
		gpio_clear(PIN_MAP[pin].gpio_pin);
}
*/

/*
uint32 digitalRead(uint8 pin)
{
  return gpio_get(PIN_MAP[pin].gpio_pin);
}
*/

#define digitalReadEx(port,bit) ((GPIO_IDR(GPIO_PORT_##port##_BASE) & (1<<bit))?1:0)
uint32 digitalRead(uint8 pin)
{
	switch (pin) {
		default: return gpio_get(PIN_MAP[pin].gpio_pin);
		case 0: return digitalReadEx(A,10);
		case 1: return digitalReadEx(A,9);
		case 2: return digitalReadEx(B,7);
		case 3: return digitalReadEx(B,6);
		case 5: return digitalReadEx(B,9);
		case 6: return digitalReadEx(A,3);
		case 10: return digitalReadEx(A,2);
		case 11: return digitalReadEx(B,0);
		case 12: return digitalReadEx(A,6);
		case 13: return digitalReadEx(B,1);
		case 14: return digitalReadEx(B,4);
		case 15: return digitalReadEx(B,3);
		case 16: return digitalReadEx(B,5);
		case 17: return digitalReadEx(A,15);
		case 18: return digitalReadEx(B,8);
		case 19: return digitalReadEx(A,0);
		case 20: return digitalReadEx(A,1);
		case 21: return digitalReadEx(A,4);
		case 22: return digitalReadEx(A,7);
		case 24: return digitalReadEx(B,12);
		case 25: return digitalReadEx(C,13);
		case 26: return digitalReadEx(A,11);
		case 27: return digitalReadEx(A,12);
		case 28: return digitalReadEx(A,5);
		case 29: return digitalReadEx(A,8);
		case 30: return digitalReadEx(A,13);
		case 31: return digitalReadEx(A,14);
		case 32: return digitalReadEx(B,10);
		case 33: return digitalReadEx(B,11);
		case 34: return digitalReadEx(B,13);
		case 35: return digitalReadEx(B,14);
		case 36: return digitalReadEx(B,15);
	}
}

void analogWrite(uint8 pin, int duty_cycle8)
{
    if (pin == BOARD_DAC_PIN)
        dac_set_data(DAC_8R, DAC_CH1, duty_cycle8);
    else if (PIN_MAP[pin].timer_device != TIMx)
        tim_set_capture_compare_value(PIN_MAP[pin].timer_channel, duty_cycle8);
    else
        digitalWrite(pin, duty_cycle8 > 127 ? 1 : 0);
}

uint16 analogRead(uint8 pin)
{
    uint16 ret;

    /* Set channel number */
    adc_set_regular_channel(PIN_MAP[pin].adc_channel);

    /* ADC on */
	adc_enable();

    /* 10bit resolution */
    adc_set_resolution(10);

    /* ADC power-up time (tSTAB) */
    delayMicroseconds(ADC_T_STAB);

    /* Start conversion. */
    adc_start_regular_conversion();

    /* Wait for the end of conversion. */
    while (!adc_get_interrupt_status(ADC_REGULAR_END))
        ;

    /* Get data and clear interrupt. */
    ret = adc_get_regular_data();

    /* ADC off */
	adc_disable();

    return ret;
}

unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout)
{
    uint32_t start_pulse = 0;
	uint32_t start_timeout = millis();
    
    switch(state)
    {
        case HIGH:
            while(digitalRead(pin)) {if((micros() - start_timeout ) >= timeout) {return 0;}}
            while(!digitalRead(pin)) {if((micros() - start_timeout ) >= timeout) {return 0;}}
            start_pulse = micros();
            while(digitalRead(pin)) {if((micros() - start_timeout ) >= timeout) {return 0;}}
            return (micros() - start_pulse);
        
        case LOW:
            while(!digitalRead(pin)) {if((micros() - start_timeout ) >= timeout) {return 0;}}
            while(digitalRead(pin)) {if((micros() - start_timeout ) >= timeout) {return 0;}}
            start_pulse = micros();
            while(!digitalRead(pin)) {if((micros() - start_timeout ) >= timeout) {return 0;}}
            return (micros() - start_pulse);
            
        default:
            return 0;        
    }   
}
