/*
 Title:        XbeeMatrixChrono (FireDay Uno 2012)
 Description:  Makes a chronometer out of the XbeeMatrix project for
               Robopoly contests.
 Author:       Karl Kangur <karl.kangur@gmail.com>
 Date:         2012-11-16
 Version:      1.0
 Website:      http://robopoly.ch
 Comments:     Matrix control based on demo16x24.c by Bill Westfield
 Usage:        Connect bumper pins to PD2 and PD3, rising edge of
               either triggers the stop signal.
               Connect PC0 pin of the arch to PB2 of this board,
               rising edge = start, falling edge = reset.
               Don't forget to connet the grounds.
*/

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "ht1632.h"
#include "fonts.h"

// pixel offset from
#define MINUTES_TENS 16
#define MINUTES_ONES 24
#define DOTS_MINUTES 32
#define SECONDS_TENS 40
#define SECONDS_ONES 48
#define DOTS_SECONDS 56
#define MILLIS_TENS  64
#define MILLIS_ONES  72
#define CROWN_LEFT   0
#define CROWN_RIGHT  80

// helper functions
#define port_ddr(port) (port-1)
#define port_pin(port) (port-2)
#define pin_mode(port, pin, mode) (*(port_ddr(&port)) = (*(&port-1) & (~(1 << pin))) | (mode << pin))
#define digital_write(port, pin, value) (port = (port & (~(1 << pin))) | (value << pin))

// 24 modules of 8 lines = 192 uint8_ts of data
uint8_t c, addr, x, y, data;
uint8_t screen[192];
uint8_t counting = 0;

// store hours, minutes and seconds
struct time
{
  unsigned char minutes, seconds, milliseconds;
};

// time instance
time myTime;

/*
 * Set these constants to the values of the pins connected to the SureElectronics Module
 */
static const uint8_t ht1632_data = PA(1);  // Data pin (pin 7)
static const uint8_t ht1632_wrclk = PA(3); // Write clock pin (pin 5)
/*
 * ht1632_writebits
 * Write bits (up to 8) to h1632 on pins ht1632_data, ht1632_wrclk
 * Chip is assumed to already be chip-selected
 * Bits are shifted out from MSB to LSB, with the first bit sent
 * being (bits & firstbit), shifted till firsbit is zero.
 */
void ht1632_chipselect(uint8_t chipno)
{
  switch(chipno)
  {
    case 0:
      PORTA &= ~(1 << 7);
      break;
    case 1:
      PORTA &= ~(1 << 6);
      break;
    case 2:
      PORTA &= ~(1 << 5);
      break;
    case 3:
      PORTA &= ~(1 << 4);
      break;
  }
}

void ht1632_chipfree(uint8_t chipno)
{
  switch(chipno)
  {
    case 0:
      PORTA |= (1 << 7);
      break;
    case 1:
      PORTA |= (1 << 6);
      break;
    case 2:
      PORTA |= (1 << 5);
      break;
    case 3:
      PORTA |= (1 << 4);
      break;
  }
}

void ht1632_writebits(uint8_t bits, uint8_t firstbit)
{
  while(firstbit)
  {
    //digitalWrite(ht1632_wrclk, LOW);
    //digital_write(PORTA, 3, 0);
    PORTA &= ~(1 << 3);
    if(bits & firstbit)
    {
      //digitalWrite(ht1632_data, HIGH);
      //digital_write(PORTA, 1, 1);
      PORTA |= (1 << 1);
    } 
    else
    {
      //digitalWrite(ht1632_data, LOW);
      //digital_write(PORTA, 1, 0);
      PORTA &= ~(1 << 1);
    }
    //digitalWrite(ht1632_wrclk, HIGH);
    //digital_write(PORTA, 3, 1);
    PORTA |= (1 << 3);
    firstbit >>= 1;
  }
}

static void ht1632_sendcmd(uint8_t chip, uint8_t command)
{
  ht1632_chipselect(chip);  // Select chip
  ht1632_writebits(HT1632_ID_CMD, 1<<2);  // send 3 bits of id: COMMMAND
  ht1632_writebits(command, 1<<7);  // send the actual command
  ht1632_writebits(0, 1);       /* one extra dont-care bit in commands. */
  ht1632_chipfree(chip); //done
}

static void ht1632_senddata(uint8_t chip, uint8_t address, uint8_t data)
{
  ht1632_chipselect(chip);  // Select chip
  ht1632_writebits(HT1632_ID_WR, 1<<2);  // send ID: WRITE to RAM
  ht1632_writebits(address, 1<<6); // Send address
  ht1632_writebits(data, 1<<3); // send 4 bits of data
  ht1632_chipfree(chip); // done
}

int main()  // flow chart from page 17 of datasheet
{
  asm("CLI");
  DDRA = 0xf0;
  PORTA = 0xf0;
  
  // led pin
  //pinMode(PC(2), OUTPUT);
  pin_mode(PORTC, 2, 1);
  
  // clock and data lines
  pin_mode(PORTA, 1, 1);
  pin_mode(PORTA, 3, 1);
  
  // system startup routine
  for(uint8_t c = 0; c < 4; c++)
  {
    ht1632_sendcmd(c, HT1632_CMD_SYSDIS);  // Disable system
    ht1632_sendcmd(c, HT1632_CMD_COMS11);  // 16*32, PMOS drivers
    ht1632_sendcmd(c, HT1632_CMD_MSTMD);   // Master Mode
    ht1632_sendcmd(c, HT1632_CMD_SYSON);   // System on
    ht1632_sendcmd(c, HT1632_CMD_LEDON);   // LEDs on
    for (uint8_t i=0; i<128; i++)
      ht1632_senddata(c, i, 0);  // clear the display!
  }
  
  // set up end bumper interrupts for bonth finishes

  // External Interrupt Control Register A
  EICRA |= (1 << ISC20) + (1 << ISC11) + (1 << ISC01);
  // External Interrupt Mask Register
  EIMSK |= (1 << INT2) + (1 << INT1) + (1 << INT0);
  
  // set up timer interrupt to call every 10 milliseconds (100Hz)
  TCCR1A = 0;
  TCCR1B = (1 << WGM12)|(1 << CS11)|(1 << CS10);
  TCNT1 = 0;
  OCR1A = 1250 - 1;
  //TIMSK1 |= (1 << OCIE1A);
  
  initChrono();
  asm("SEI");
  
  while(1);
  return 0;
}

// stop 1
ISR(INT0_vect)
{
  PORTC ^= (1 << 2);
  if(counting)
  {
    // show crown left
    TIMSK1 &= ~(1 << OCIE1A);
    counting = 0;
    writeToPosition(crown_left, CROWN_LEFT, 0, 1);
    writeToPosition(crown_right, CROWN_LEFT+8, 0, 1);
  }
}

// stop 2
ISR(INT1_vect)
{
  PORTC ^= (1 << 2);
  if(counting)
  {
    // show crown right
    TIMSK1 &= ~(1 << OCIE1A);
    counting = 0;
    writeToPosition(crown_left, CROWN_RIGHT, 0, 1);
    writeToPosition(crown_right, CROWN_RIGHT+8, 0, 1);
  }
}

// start/stop: PB2
ISR(INT2_vect)
{
  PORTC ^= (1 << 2);
  // read the external interrupt pin: 1 = start, 0 = reset
  if(PINB & (1 << 2))
  {
    // enable timer counter
    counting = 1;
    TIMSK1 |= (1 << OCIE1A);
  }
  else
  {
    TIMSK1 &= ~(1 << OCIE1A);
    counting = 0;
    initChrono();
  }
}

ISR(TIMER1_COMPA_vect)
{
  if(myTime.milliseconds < 99)
  {
    myTime.milliseconds++;
    // must take less than a hundreth of a second to process
    if(counting)
    {
      writeToPosition(b[myTime.milliseconds / 10], MILLIS_TENS, 0, 1);
      writeToPosition(b[myTime.milliseconds % 10], MILLIS_ONES, 0, 1);
    }
  }
  else if(myTime.seconds < 59)
  {
    myTime.milliseconds = 0;
    myTime.seconds++;
    
    if(myTime.minutes == 1 && myTime.seconds == 30)
    {
      TIMSK1 &= ~(1 << OCIE1A);
    }
    
    if(counting)
    {
      writeToPosition(b[0], MILLIS_TENS, 0, 1);
      writeToPosition(b[0], MILLIS_ONES, 0, 1);
      writeToPosition(b[myTime.seconds / 10], SECONDS_TENS, 0, 1);
      writeToPosition(b[myTime.seconds % 10], SECONDS_ONES, 0, 1);
    }
  }
  else if(myTime.minutes < 99)
  {
    myTime.milliseconds = 0;
    myTime.seconds = 0;
    myTime.minutes++;
    if(counting)
    {
      writeToPosition(b[0], MILLIS_TENS, 0, 1);
      writeToPosition(b[0], MILLIS_ONES, 0, 1);
      writeToPosition(b[0], SECONDS_TENS, 0, 1);
      writeToPosition(b[0], SECONDS_ONES, 0, 1);
      writeToPosition(b[myTime.minutes / 10], MINUTES_TENS, 0, 1);
      writeToPosition(b[myTime.minutes % 10], MINUTES_ONES, 0, 1);
    }
  }
  else
  {
    initChrono();
  }
}

void initChrono()
{
  myTime.minutes = 0;
  myTime.seconds = 0;
  myTime.milliseconds = 0;
  
  // clear display
  for(uint8_t c = 0; c < 4; c++)
  {
    for (uint8_t i=0; i<128; i++)
      ht1632_senddata(c, i, 0);  // clear the display!
  }
  // write 00:00:00 to screen
  writeToPosition(b[0], MINUTES_TENS, 0, 1);
  writeToPosition(b[0], MINUTES_ONES, 0, 1);
  writeToPosition(b_dots, DOTS_MINUTES, 0, 1);
  writeToPosition(b[0], SECONDS_TENS, 0, 1);
  writeToPosition(b[0], SECONDS_ONES, 0, 1);
  writeToPosition(b_dots, DOTS_SECONDS, 0, 1);
  writeToPosition(b[0], MILLIS_TENS, 0, 1);
  writeToPosition(b[0], MILLIS_ONES, 0, 1);
}

// returns a character line from program memory
uint8_t getLine(const uint8_t* element, uint8_t line)
{
  // program memory is 16 bits wide
  return pgm_read_word(&(element[line])) & 0xff;
}

// returns a nibble
uint8_t getColumn(const uint8_t* element, uint8_t col, uint8_t nibble)
{
  return (((getLine(element, 0+4*nibble) >> (7 - col)) & 1) << 3) + (((getLine(element, 1+4*nibble) >> (7 - col)) & 1) << 2) + (((getLine(element, 2+4*nibble) >> (7 - col)) & 1) << 1) + ((getLine(element, 3+4*nibble) >> (7 - col)) & 1);
}

void writeToPosition(const uint8_t* element, uint8_t posx, uint8_t posy, uint8_t width)
{
  for(x = 0; x < 8*width; x++)
  {
    // write one column (4 nibbles) at a time
    ht1632_senddata((posx + x) / 24, (posx%24 + x) * 4+0, getColumn(element, x, 0));
    ht1632_senddata((posx + x) / 24, (posx%24 + x) * 4+1, getColumn(element, x, 1));
    ht1632_senddata((posx + x) / 24, (posx%24 + x) * 4+2, getColumn(element, x, 2));
    ht1632_senddata((posx + x) / 24, (posx%24 + x) * 4+3, getColumn(element, x, 3));
  }
}

