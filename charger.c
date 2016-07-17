#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
// Clock frequency = F_CPU in Makefile
#include <util/delay.h>
#include <stdio.h>

#define PERIOD1 195
#define NPERIOD1 4
#define PERIOD2 196
#define NPERIOD2 1

#define TIMER1_TOP 1023

#define DIM_PIN  PINC
#define DIM_DIR  DDRC
#define DIM_PORT PORTC
#define DIM_UP   PC2
#define DIM_DOWN PC3

/*
  #define CORR_DIR   DDRC
  #define CORR_PORT  PORTC
  #define CORR_PIN   PINC
  #define CORR_UP    PC2
  #define CORR_DOWN  PC3
*/

#define TIMEUP_DIR   DDRC
#define TIMEDN_DIR   DDRC
#define TIMEUP_PORT  PORTC
#define TIMEDN_PORT  PORTC
#define TIMEUP_PIN   PINC
#define TIMEDN_PIN   PINC
#define TIME_UP      PC4
#define TIME_DN      PC5

#define WAKE_DIR   DDRC
#define WAKE_PORT  PORTC
#define WAKE_PIN   PINC
#define WAKE_BIT   PC0

#define ALARM_DIR   DDRC
#define ALARM_PORT  PORTC
#define ALARM_PIN   PINC
#define ALARM_BIT   PC1

#define PCI_IE_MASK (_BV(PCIE1))
/*   PCINT:
	 0 = PB0
	 1 = PB1
	 2 = PB2
	 3 = PB3
	 4 = PB4
	 5 = PB5
	 6 = PB6
	 7 = PB7
*/
#define PCI0_MASK   0

/*   PCINT:
	 8 = PC0
	 9 = PC1
	 10= PC2
	 11= PC3
	 12= PC4
	 13= PC5
	 14= PC6
	 15= 
*/
#define PCI1_MASK   (_BV(PCINT9) | _BV(PCINT12) | _BV(PCINT13))
//(_BV(PCINT10) | _BV(PCINT11) | _BV(PCINT12) | _BV(PCINT13))

/*   PCINT:
	 16= PD0
	 17= PD1
	 18= PD2
	 19= PD3
	 20= PD4
	 21= PD5
	 22= PD6
	 23= PD7
*/
#define PCI2_MASK   0
// See also PCINT1_vect ISR

#include "lcd.h"

/*
  If we used the /64 prescaler, 1000000 divides evenly:
  1000000 / 64 = 15625 = 62*250 + 125
*/


ISR (PCINT1_vect) {
	// interrupt 1 routine...
}

ISR (TIMER0_COMPA_vect) {
}

ISR (TIMER1_OVF_vect) {
}

// timer is TCNT1 / TCNT1H, TCNT1L
// timer interrupt flags register is TIFR1

#define MAXDELAY (262/(F_CPU / 1000000UL))
static void long_delay_ms(uint ms) {
	while (ms >= MAXDELAY) {
		_delay_ms(MAXDELAY);
		ms -= MAXDELAY;
	}
	_delay_ms(ms);
}

static const float vreal[] =  {  0, 5.83, 6.73, 6.81, 7.13, 7.31, 7.53 };
static const uint16_t vadc[] = {  0, 509,  589,  597,  625,  642,  662 };

static const float ireal[] =   { 0,  62, 114, 162, 270, 350, 400, 500, 570 };
static const uint16_t iadc[] = { 0, 114, 189, 264, 370, 560, 670, 810, 875 };

static const float Vmargin = 0.005;
static const float Imargin = 2;

static float calibVoltage(uint v) {
	int j;
	float voltage;
	for (j=(sizeof(vreal)/sizeof(float)-2); j>=0; j--) {
		if (v >= vadc[j])
			break;
	}
	if (j == -1)
		voltage = 99;
	else
		voltage = vreal[j] + (float)(v - vadc[j]) / (float)(vadc[j+1] - vadc[j])  * (vreal[j+1] - vreal[j]);
	return voltage;
}

static float calibCurrent(uint icurrent) {
	int j;
	float current;
	for (j=(sizeof(ireal)/sizeof(float)-2); j>=0; j--) {
		if (icurrent >= iadc[j])
			break;
	}
	if (j == -1)
		current = 9999;
	else
		current = ireal[j] + (float)(icurrent - iadc[j]) / (float)(iadc[j+1] - iadc[j])  * (ireal[j+1] - ireal[j]);
	return current;
}

// ADMUX bits 7,6: REFS1,REFS0: 1,1 ==> Internal 1.1 V reference
#define ADC_REFS 0xc0

int main(void) {
	uint i;

	long_delay_ms(100);

	lcd_port_init();
	long_delay_ms(20);
	lcd_full_reset();
	long_delay_ms(20);
	lcd_write_instr(LCD_CMD_FUNCTION |
					LCD_FUNCTION_4BITS | LCD_FUNCTION_2LINES | LCD_FUNCTION_FONT_5X8);
	lcd_write_instr(LCD_CMD_DISPLAY |
					LCD_DISPLAY_ON | LCD_CURSOR_ON | LCD_CURSOR_BLINK);
	lcd_write_instr(LCD_CMD_ENTRY | LCD_ENTRY_INC);
	lcd_write_instr(LCD_CMD_HOME);

	lcd_set_stdout();

	lcd_write_instr(LCD_CMD_DISPLAY | LCD_DISPLAY_ON);
	lcd_write_instr(LCD_CMD_CLEAR);

    //sei();

	lcd_write_instr(LCD_CMD_HOME);
	printf("Hello world!");
	//lcd_write_instr(LCD_CMD_SET_DDRAM | 0x40);

	long_delay_ms(1000);

	lcd_write_instr(LCD_CMD_HOME);
	printf("ABCDEFGHIJKLMNOPQRST"
		   "UVWXYZabcdefghijklmn"
		   "opqrstuvwxyz01234567"
		   "890!@#$%%^&*() .,><\"'");

	long_delay_ms(1000);

	power_timer1_enable();
	// WGM*: 10-bit fast PWM mode -- aka mode 7.
	TCCR1A = _BV(COM1A1) | _BV(WGM11) | _BV(WGM10);
	TCCR1B = _BV(CS10) | _BV(WGM12);
#define OUTMAX 0x3ff

	// WGM*:  8-bit fast PWM mode -- aka WGM mode 5.
	/*
	 TCCR1A = _BV(COM1A1) | _BV(WGM10);
	 TCCR1B = _BV(CS10) | _BV(WGM12);
	 #define OUTMAX 0xff
	 */

	// COM1A1: Use output pin OC1A = PB1 = pin 15.
	// CS10: use full-speed clock as timer input.
	// enable output on PB1.
	DDRB |= _BV(DDB1);

	power_adc_enable();

	// 16-bit read: read low byte first.
	// 16-bit write: write high byte first.
	// in C, compiler should handle this...

	// disable digital inputs on ADC input pins.
	DIDR0 = _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D);

	// enable ADC
	ADCSRA = _BV(ADEN);

	lcd_write_instr(LCD_CMD_CLEAR);



	// Phase 1: constant-current, I = 10-30% of rated charge capacity
	//    3.2 Ah * 20% = 640 mA.

	//uint Iset = 450;
	float Iset = 400;

	// Switch to phase 2 when battery voltage >= the limit.

	// Phase 2: constant-voltage, V = 7.3 V

	// 7.3 V  ==>
	//uint Vset = 640;
	float Vset = 7.3;

	// Switch to phase 3 when charge current drops to 3% of rated
	//    3.2 Ah * 3% = 96 mA  =>
	//uint Ifloat = 260;
	float Ifloat = 96;

	// Phase 3: constant-voltage, V = 6.8 V
	//uint Vfloat = 625;
	float Vfloat = 6.8;
	
	uint32_t avg[4] = {0,0,0,0};

	int phase = 0;

	uint8_t calibmode = 0;

	for (i=0;; i++) {
		uint nwait;
		uint8_t adc_lo, adc_hi;
		uint adcval[4];
		int j;
		//char* spin = "<({|})>";
		char* spin = ".oOo. ";
#define NSPIN 6
#define SKIP 512
		uint skip = ((i % SKIP) != 0);
		uint icurrent, ivoltage;
		uint16_t outval;

		for (j=0; j<4; j++) {
			ADMUX = ADC_REFS | j; // MUX0 = ADC0 = PC0 = pin 23 of 28.

			// start conversion.
			ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADPS1) | _BV(ADPS0);
			// ADPS* - prescaler - want 50-200 kHz.
			// 1 MHz clock --> prescaling of 5 to 20.
			// --> ADPS{210} = 011 or 100 (factor 8 or 16, resp)

			// wait for conversion to finish
			nwait = 0;
			while (ADCSRA & _BV(ADSC)) {
				_delay_us(100);
				nwait++;
				if (nwait == 10000)
					break;
			}

			//value in ADCH, ADCL;
			// must read ADCL first.
			adc_lo = ADCL;
			adc_hi = ADCH;
			adcval[j] = (adc_hi << 8) | adc_lo;
			avg[j] += adcval[j];
			//if (skip)
			//break;
		}

		if (skip)
			continue;

		outval = OCR1A;

		for (j=0; j<4; j++)
			avg[j] = (avg[j] + SKIP/2) / SKIP;

		if (avg[3] > 512) {
			if (calibmode == 0)
				outval = avg[0];
			calibmode = 1;
			lcd_move_to(60);
			printf("CALIB         ");
		} else {
			calibmode = 0;
			lcd_move_to(60);
			printf("              ");
		}

		icurrent = avg[1];
		ivoltage = avg[2];

		float voltage, current;

		voltage = calibVoltage(ivoltage);
		current = calibCurrent(icurrent);

		if (!calibmode) {
			if (phase == 0) {
				if (current < Iset)
					outval += 2;
				else
					phase = 1;
			}
			if (phase == 1) {
				if (current > Iset+Imargin)
					outval--;
				else if (current < Iset-Imargin)
					outval++;
			}
			if (phase == 0 || phase == 1) {
				// check for voltage reaching the limit.
				if (voltage >= Vset) {
					phase = 2;
				}
			}
			if (phase == 2) {
				if (voltage > Vset+Vmargin) {
					outval--;
				} else if (voltage < Vset-Vmargin) {
					outval++;
				}
				// check for current dropping down to limit.
				if (current <= Ifloat) {
					phase = 3;
				}
			}
			if (phase == 3) {
				if (voltage > Vfloat+Vmargin) {
					outval--;
				} else if (voltage < Vfloat-Vmargin) {
					outval++;
				}
			}
		}

		lcd_write_instr(LCD_CMD_HOME);
		printf("%c %4li I=%4li V=%4li", // %4li                   ",
			   spin[(i/SKIP)%NSPIN],
			   avg[0], avg[1], avg[2]); //, avg[3]);


		// don't overflow!
		if (outval > OUTMAX)
			outval = OUTMAX;
		// OCR1A: set PWM duty cycle
		OCR1A = outval;

		lcd_move_to(40);
		printf("Phase %i: V=%4.2f V    ",
			   phase, (double)voltage);

		lcd_move_to(20);
		printf("I=%6.1f mA ctl=%4i",
			   (double)current, outval);

		for (j=0; j<4; j++)
			avg[j] = 0;

	}


	return 0;
}
