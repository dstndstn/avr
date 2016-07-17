#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
// Clock frequency = F_CPU in Makefile
#include <util/delay.h>
#include <stdio.h>

#define PERIOD1 195
#define NPERIOD1 4
#define PERIOD2 196
#define NPERIOD2 1

#define TIMER1_TOP 1023
#define LIGHT_MAX   TIMER1_TOP
#define LIGHT OCR1A
#define DDROC DDRB
#define OC1 PB1

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

// Bright up the lights over what time period (in minutes)?
#define WAKETIME    20

// how much to increment the light level each second.
#define LIGHT_STEP ((float)LIGHT_MAX / (float)(WAKETIME * 60.0))

#include "lcd.h"

static void reset_prescaler() {
	// reset the prescaler.
	// Warning, this can also affect Timer1 if it uses the prescaler!
	GTCCR |= _BV(PSRSYNC);
}

static char hours, minutes, seconds;
static char wakehours, wakeminutes;
static uint correction;

static volatile char timeup, timedn, alarm_toggle;
static volatile uchar alarm_deadzone = 0;

static uchar alarm_active = 0;

// when to start bringing up the lights...
static char alarmhrs, alarmmins;

// how many seconds has it been since the wakeup time was set?
static char setdelay;

// light level.
static float light;

// alarm on or off?
static char alarm;

/*
  If we used the /64 prescaler, 1000000 divides evenly:
  1000000 / 64 = 15625 = 62*250 + 125
*/

static void write_time() {
	char hr;
	char ampm;
	char wakehr;
	char wakeampm;
	char pct;

	ampm = ((hours >= 12) ? 'p' : 'a');
	hr = hours;
	if (hr == 0)
		hr = 12;
	else if (hr > 12)
		hr -= 12;

	wakeampm = ((wakehours >= 12) ? 'p' : 'a');
	wakehr = wakehours;
	if (wakehr == 0)
		wakehr = 12;
	else if (wakehr > 12)
		wakehr -= 12;

	lcd_write_instr(LCD_CMD_HOME);
	printf("  %s%i:%s%i:%s%i%cm    ",
		   (hr<10?" ":""), hr,
		   (minutes<10?"0":""), minutes,
		   (seconds<10?"0":""), seconds,
		   ampm);

	pct = (char)((100.0 * light) / (float)(LIGHT_MAX));
	printf("L: %s%s%i %%", (pct < 100 ? " ":""), (pct < 10 ? " ":""), (int)pct);

	lcd_write_instr(LCD_CMD_SET_DDRAM | 0x40);
	//printf("correction: %i   ", correction);

	printf("Wake:%s%i:%s%i%cm  ",
		   (wakehr<10?" ":""), wakehr,
		   (wakeminutes<10?"0":""), wakeminutes,
		   wakeampm);

	printf("Alarm: %s", (alarm ? "ON " : "OFF"));
	/*
	  printf("%s%i:%s%i%cm %i:%i:%i=%i",
	  (wakehr<10?" ":""), wakehr,
	  (wakeminutes<10?"0":""), wakeminutes,
	  wakeampm,
	  nexthr, nextmin, nextsec, nextlevel);
	*/

}

ISR (PCINT1_vect) {
	if (TCNT0 > 50) {
		timeup  = timeup  || !(TIMEUP_PIN & _BV(TIME_UP));
		timedn  = timedn  || !(TIMEDN_PIN & _BV(TIME_DN));
	}
	if (!alarm_deadzone)
		alarm_toggle = alarm_toggle || !(ALARM_PIN & _BV(ALARM_BIT));
}

ISR (TIMER0_COMPA_vect) {
	static uchar x;
	static uchar setwake;
	static uchar update = 0;

	// how many times in a row has each button been on?
	static uchar ntimeup = 0;
	static uchar ntimedn = 0;

	_delay_loop_2(correction);

	GTCCR |= _BV(TSM);
	// reset the counter.
	TCNT0 = 0;
	reset_prescaler();
	GTCCR &= ~(_BV(TSM));

	x++;
	if (x == NPERIOD1) {
		// adjust the OCR0A value during the final cycle
		OCR0A = PERIOD2;
	}

	/*
	  if (!(CORR_PIN & _BV(CORR_UP))) {
	  correction+=10;
	  lcd_write_instr(LCD_CMD_SET_DDRAM | 0x40);
	  printf("correction: %i   ", correction);
	  }
	  if (!(CORR_PIN & _BV(CORR_DOWN))) {
	  correction-=10;
	  lcd_write_instr(LCD_CMD_SET_DDRAM | 0x40);
	  printf("correction: %i   ", correction);
	  }
	*/

	setwake = !(WAKE_PIN & _BV(WAKE_BIT));

	// alarm_toggle
	alarm_toggle = alarm_toggle || !(ALARM_PIN & _BV(ALARM_BIT));
	if (alarm_toggle) {
		if (!alarm_deadzone) {
			alarm = (alarm ? 0 : 1);
			update = 1;
			alarm_toggle = 0;
			alarm_deadzone = 2;

			if (!alarm)
				// Turn off the alarm...
				alarm_active = 0;
			// FIXME: else turn it on, if applicable...
		}
	}
	if (alarm_deadzone) {
		alarm_deadzone--;
	}

	timeup  = timeup  || !(TIMEUP_PIN & _BV(TIME_UP));
	timedn  = timedn  || !(TIMEDN_PIN & _BV(TIME_DN));

	if (timeup || timedn) {
		uchar step;
		char *pmins, *phrs;
		uchar* pn;

		if (timeup)
			pn = &ntimeup;
		else
			pn = &ntimedn;

		if (setwake) {
			step = 5;
			pmins = &wakeminutes;
			phrs  = &wakehours;
		} else {
			if (*pn >= 25) {
				step = 10;
			} else {
				if (*pn >= 20) {
					step = 7;
				} else if (*pn >= 15) {
					step = 5;
				} else if (*pn >= 10) {
					step = 3;
				} else {
					step = 1;
				}
				(*pn)++;
			}
			pmins = &minutes;
			phrs  = &hours;
		}

		if (timeup) {
			*pmins += step;
			if (*pmins >= 60) {
				*pmins -= 60;
				(*phrs)++;
				if (*phrs == 23)
					*phrs = 0;
			}
		}
		if (timedn) {
			*pmins -= step;
			if (*pmins < 0) {
				*pmins += 60;
				(*phrs)--;
				if (*phrs == -1)
					*phrs = 23;
			}
		}

		update = 1;
	}

	if (!timeup)
		ntimeup = 0;
	if (!timedn)
		ntimedn = 0;

	if (timeup || timedn) {
		setdelay = 0;
	} else if (setdelay < 25) {
		setdelay++;
		if (setdelay == 25) {
			// compute when to start bringing up the lights.
			alarmhrs  = wakehours;
			alarmmins = wakeminutes - WAKETIME;
			while (alarmmins < 0) {
				alarmmins += 60;
				alarmhrs--;
			}
			if (alarmhrs < 0) {
				alarmhrs += 24;
			}

			// FIXME: if (time - waketime) < WAKETIME, set alarm_active?
		}
	}

	timeup = timedn = 0;

	if (x < (NPERIOD1 + NPERIOD2)) {
		if (update)
			write_time();
		return;
	}

	x = 0;
	OCR0A = PERIOD1;

	seconds++;
	if (seconds == 60) {
		minutes++;
		seconds = 0;
		if (minutes == 60) {
			hours++;
			minutes = 0;
			if (hours == 24)
				hours = 0;
		}
	}

	if ((hours   == alarmhrs) &&
		(minutes == alarmmins) &&
		(seconds == 0)) {
		alarm_active = 1;
	}

	if (alarm_active) {
		if (alarm) {
			light += LIGHT_STEP;
			if (light >= LIGHT_MAX) {
				light = LIGHT_MAX;
				// Done!
				alarm_active = 0;
			}
			LIGHT = light;
		}
	}

	write_time();
}

ISR (TIMER1_OVF_vect) {
	static uint8_t step;

	// Read inputs every N overflows...
	step++;
	if (step < 2)
		return;
	step = 0;

	// Read input...
	if (!(DIM_PIN & _BV(DIM_UP))) {
		// inc
		light += 1.0;
		if (light > LIGHT_MAX)
			light = LIGHT_MAX;
	}
	if (!(DIM_PIN & _BV(DIM_DOWN))) {
		// dec
		light -= 1.0;
		if (light < 0.0)
			light = 0.0;
	}

	LIGHT = light;
}

// timer is TCNT1 / TCNT1H, TCNT1L
// timer interrupt flags register is TIFR1

/*
  static void long_delay_ms(uint ms) {
  while (ms >= 250) {
  _delay_ms(250);
  ms -= 250;
  }
  _delay_ms(ms);
  }
*/

int main(void) {
	//uint i;

    /* Timer 1 is 10-bit PWM. */
    TCCR1A = _BV(WGM10) | _BV(WGM11) | _BV(COM1A1);
	/* Use clock input. */
    TCCR1B |= _BV(CS10);
	// clock/1024
	//TCCR1B = _BV(CS12) | _BV(CS10);
	// clock/64
	//TCCR1B = _BV(CS11) | _BV(CS10);

    /* Turn out the light, turn out the light.... */
    LIGHT = light = 0;

    /* Enable OC1 as output. */
    DDROC = _BV(OC1);

	/* Enable timer 1 overflow interrupt. */
    TIMSK1 = _BV(TOIE1);

	// Timer 0: 8-bit CTC-mode counter.
	TCCR0A = _BV(WGM01);
	// clock/1024
	TCCR0B |= _BV(CS02) | _BV(CS00);
	// TCNT0, OCR0A, OCR0B
	// Set output compare value:
	OCR0A = PERIOD1;

	// Enable timer 0 output compare match interrupt A
	TIMSK0 |= _BV(OCIE0A);

	/* Set dim port inputs & pull-ups. */
	DIM_DIR &= ~(_BV(DIM_UP) | _BV(DIM_DOWN));
	DIM_PORT |= (_BV(DIM_UP) | _BV(DIM_DOWN));

	/* Set correction port inputs & pull-ups */
	/*
	  CORR_DIR &= ~(_BV(CORR_UP) | _BV(CORR_DOWN));
	  CORR_PORT |= (_BV(CORR_UP) | _BV(CORR_DOWN));
	*/

	/* Set time-setting ports to input, and set pull-ups. */
	TIMEUP_DIR  &= ~(_BV(TIME_UP));
	TIMEDN_DIR  &= ~(_BV(TIME_DN));
	TIMEUP_PORT  |= _BV(TIME_UP);
	TIMEDN_PORT  |= _BV(TIME_DN);

	/* Set pin-changed interrupt for time-setting buttons. */
	PCICR  |= PCI_IE_MASK;
	PCMSK0 |= PCI0_MASK;
	PCMSK1 |= PCI1_MASK;
	PCMSK2 |= PCI2_MASK;

	/* Set wake port to input with pull-up. */
	WAKE_DIR  &= ~(_BV(WAKE_BIT));
	WAKE_PORT |=   _BV(WAKE_BIT);

	/* Set alarm toggle port to input with pull-up. */
	ALARM_DIR  &= ~(_BV(ALARM_BIT));
	ALARM_PORT |=   _BV(ALARM_BIT);

	lcd_port_init();
	_delay_ms(20);
	lcd_full_reset();
	lcd_write_instr(LCD_CMD_FUNCTION |
					LCD_FUNCTION_4BITS | LCD_FUNCTION_2LINES | LCD_FUNCTION_FONT_5X8);
	lcd_write_instr(LCD_CMD_DISPLAY |
					LCD_DISPLAY_ON | LCD_CURSOR_ON | LCD_CURSOR_BLINK);
	lcd_write_instr(LCD_CMD_ENTRY | LCD_ENTRY_INC);
	lcd_write_instr(LCD_CMD_HOME);

	lcd_set_stdout();

	lcd_write_instr(LCD_CMD_DISPLAY | LCD_DISPLAY_ON);
	lcd_write_instr(LCD_CMD_CLEAR);

	correction = 590;

	// reset the counter.
	TCNT0 = 0;
	reset_prescaler();
	hours = minutes = 0;
	seconds = 0;

	wakehours = 7;
	wakeminutes = 0;

	timeup = timedn = 0;
	alarm_toggle = 0;

	hours = 6;
	minutes = 39;
	seconds = 45;

	setdelay = 0;

    sei();

	for (;;) {
        sleep_mode();
	}

	return 0;
}
