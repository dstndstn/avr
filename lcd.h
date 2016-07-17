#ifndef LCD_H
#define LCD_H

#include <stdint.h>

#define LCD_CMD_CLEAR 0x1
#define LCD_CMD_HOME  0x2
#define LCD_CMD_ENTRY 0x4
#define   LCD_ENTRY_INC 0x2
#define   LCD_ENTRY_DEC 0x0
#define   LCD_ENTRY_SHIFT_DISPLAY 0x1
#define LCD_CMD_DISPLAY 0x8
// "args" of the LCD_SET_DISPLAY command:
#define   LCD_DISPLAY_ON   0x4
#define   LCD_CURSOR_ON    0x2
#define   LCD_CURSOR_BLINK 0x1
#define LCD_CMD_SHIFT  0x10
#define   LCD_SHIFT_DISPLAY 0x4
#define   LCD_SHIFT_CURSOR  0x0
#define   LCD_SHIFT_RIGHT    0x0
#define   LCD_SHIFT_LEFT    0x0
#define LCD_CMD_FUNCTION   0x20
#define   LCD_FUNCTION_8BITS 0x10
#define   LCD_FUNCTION_4BITS 0x0
#define   LCD_FUNCTION_2LINES 0x8
#define   LCD_FUNCTION_1LINE  0x0
#define   LCD_FUNCTION_FONT_5X10 0x4
#define   LCD_FUNCTION_FONT_5X8  0x0
#define LCD_CMD_SET_CGRAM      0x40
#define LCD_CMD_SET_DDRAM      0x80

#include "lcd-config.h"

typedef uint8_t uchar;
typedef uint16_t uint;

void lcd_write_instr(uchar cmd);
void lcd_write_data(uchar data);
void lcd_write_4bit_instr();
void lcd_port_init();
void lcd_full_reset();

void lcd_move_to(uint8_t posn);

void lcd_set_stdout();

#endif

