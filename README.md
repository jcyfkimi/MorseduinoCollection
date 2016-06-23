# MorseduinoCollection
A collection of WB7FHC's Morseduino project. Including sechmatic, gerber file, BOM list and arduino source code.

BELOW IS THE ORIGINAL LINCESE INFORMATION
***********************************************************************
   WB7FHC's Simple Morse Code Decoder v. 2.0 [03/15/15]
   (c) 2015, Budd Churchward - WB7FHC
   This is an Open Source Project
   http://opensource.org/licenses/MIT
   
   Search YouTube for 'WB7FHC' to see several videos of this project
   as it was developed.
   
   MIT license, all text above must be included in any redistribution
   **********************************************************************
   
   The program will automatically adjust to the speed of code that
   is being sent. The first few characters may come out wrong while it
   homes in on the speed. If you are not seeing solid copy, press the
   restart button on your Arduino.
   
   The software tracks the speed of the sender's dahs to make
   its adjustments. The more dahs you send at the beginning
   the sooner it locks into solid copy.
   
   This project is built around the 20x4 LCD display. The sketch includes
   funtions for word wrap and scrolling. If a word extends beyond the 20
   column line, it will drop down to the next line. When the bottom line
   is filled, all lines will scroll up one row and new text will continue
   to appear at the bottom.
   
   This version makes use of the 4 digit parallel method of driving the
   display.
   
   If you are planning on using a 16x2 you will want to make some changes.
   Frankly, I don't think scrolling makes sense with only two lines.
   Sometimes long words or missed spaces will result in only two words
   left on your display. If you don't have a 20x4 (they're really only a
   few bucks more) you might want to leave out the word wrap and scrolling.
   
   Hook up your LCD panel to the Arduino using these pins:
     LCD pin  1 to GND
     LCD pin  2 to +5V
     LCD pin  4 to A5
     LCD pin  6 to A4
     LCD pin 11 to A3
     LCD pin 12 to A2
     LCD pin 13 to A1
     LCD pin 14 to A0
     LCD pin 15 to +5V
     LCD pin 16 to GND
     
  
  If you hook a speaker between D9 and GND and a telegraph key
  between D8 and ground you can hear yourself practice Morse Code.
  Send -..-- to change the pitch of the tone.
  
  If you want to see how your noise filter is working you can hook
  one oscilloscope probe to D5 and another to D8. D8 will show the
  raw input to the ATMega and D5 will show the processed signal which
  is the one that will be decoded.
  
*********************************************************************
