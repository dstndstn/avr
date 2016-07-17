#include <avr/io.h>
#include <compat/ina90.h>
#include <util/delay.h>
#include <stdio.h>

#include "lcd.h"

void lcd_move_to(uint8_t posn) {
	lcd_write_instr(0x80 | (posn & 0x7f));
}

static int lcd_putchar(char c, FILE *stream) {
	if (c == '\n')
		lcd_write_instr(LCD_CMD_SET_DDRAM | 0x40);
	else
		lcd_write_data(c);
	return 0;
}

static FILE lcdstdout = FDEV_SETUP_STREAM(lcd_putchar, NULL, _FDEV_SETUP_WRITE);

void lcd_set_stdout() {
	stdout = &lcdstdout;
}

#define nop _NOP

#define clear_rs_bit()  LCD_PORT_RS &= ~(_BV(LCD_BIT_RS))
#define set_rs_bit()    LCD_PORT_RS |=   _BV(LCD_BIT_RS)
#define clear_e_bit()   LCD_PORT_E  &= ~(_BV(LCD_BIT_E))
#define set_e_bit()     LCD_PORT_E  |=   _BV(LCD_BIT_E)
#define set_read_bit()  LCD_PORT_RW |=   _BV(LCD_BIT_RW)
#define set_write_bit() LCD_PORT_RW &= ~(_BV(LCD_BIT_RW))
//#define get_db7654_bits()  ((LCD_PORT_DATA &  LCD_BITMASK_D7654)     >> LCD_BITSHIFT_D7654)
//#define set_db7654_bits(x)   LCD_PORT_DATA = (LCD_BITMASK_D7654 & (x << LCD_BITSHIFT_D7654))
#define get_db7654_bits()   (LCD_PIN_DATA &  LCD_BITMASK_D7654)
#define set_db7654_bits(x)   LCD_PORT_DATA = (LCD_PORT_DATA & ~LCD_BITMASK_D7654) | (LCD_BITMASK_D7654 & (x))
#define set_data_input()   LCD_DIR_DATA  &= ~(LCD_BITMASK_D7654)
#define set_data_output()  LCD_DIR_DATA  |=   LCD_BITMASK_D7654

static void delay_e() {
	_delay_us(1);
}

static void set_dir() {
	LCD_DIR_RS |= _BV(LCD_BIT_RS);
	LCD_DIR_E  |= _BV(LCD_BIT_E);
	LCD_DIR_RW |= _BV(LCD_BIT_RW);
}

void lcd_port_init() {
	set_dir();
}

static void wait_for_not_busy() {
	uchar x, y;
	uint maxdelay = 2000;
	clear_rs_bit();
	set_read_bit();
	clear_e_bit();
	set_data_input();
	_delay_us(1);

	do {
		set_e_bit();
		delay_e();
		x = get_db7654_bits();
		clear_e_bit();

		delay_e();
		set_e_bit();
		delay_e();
		y = get_db7654_bits();
		clear_e_bit();
		delay_e();

		maxdelay--;
		if (!maxdelay)
			break;
	} while (x & 0x8);

	// read address counter.
	/*
	  _delay_us(4);
	  set_e_bit();
	  delay_e();
	  x = get_db7654_bits();
	  clear_e_bit();
	  delay_e();
	  set_e_bit();
	  delay_e();
	  y = get_db7654_bits();
	  clear_e_bit();
	  delay_e();
	*/
}

void lcd_write_4bit_instr() {
	clear_rs_bit();
	set_write_bit();
	clear_e_bit();
	set_data_output();
	set_db7654_bits((LCD_CMD_FUNCTION | LCD_FUNCTION_4BITS) >> 4);

	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();
	delay_e();
}

void lcd_write_instr(uchar cmd) {
	wait_for_not_busy();

	clear_rs_bit();
	set_write_bit();
	clear_e_bit();
	set_data_output();

	set_db7654_bits(cmd >> 4);
	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();
	delay_e();

	set_db7654_bits(cmd & 0xf);
	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();
	delay_e();
}

void lcd_write_data(uchar data) {
	wait_for_not_busy();

	set_rs_bit();
	set_write_bit();
	clear_e_bit();
	set_data_output();

	set_db7654_bits(data >> 4);
	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();
	delay_e();

	set_db7654_bits(data & 0xf);
	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();
	delay_e();
}

void lcd_full_reset() {
	clear_rs_bit();
	set_write_bit();
	clear_e_bit();
	set_data_output();

	_delay_ms(15);

	set_db7654_bits(0x3);
	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();

	_delay_ms(5);

	set_db7654_bits(0x3);
	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();

	_delay_ms(1);

	set_db7654_bits(0x3);
	_delay_us(1);
	set_e_bit();
	delay_e();
	clear_e_bit();

	_delay_ms(1);

	lcd_write_4bit_instr();
}

