#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#define LCD_PORT_RS   PORTD
#define LCD_PORT_E    PORTD
#define LCD_PORT_RW   PORTD
#define LCD_PORT_DATA PORTD

#define LCD_PIN_DATA  PIND

#define LCD_DIR_RS    DDRD
#define LCD_DIR_E     DDRD
#define LCD_DIR_RW    DDRD
#define LCD_DIR_DATA  DDRD

#define LCD_BIT_RS     PD4
#define LCD_BIT_E      PD5
#define LCD_BIT_RW     PD6
#define LCD_BITMASK_D7654  (_BV(PD0) | _BV(PD1) | _BV(PD2) | _BV(PD3))
//#define LCD_BITSHIFT_D7654 0

#endif
