

/***********************************************************************
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
  
*********************************************************************/

//////////////////////////////////////////////////
#include <EEPROM.h>
int addr = 0;

#include <SPI.h>

#define HISCALL "WB7FHC"            //Callsign of kit builder
#define VDATE "2015.12.10"          //Sketch Version Date
#define DEBUG 0                     // 1 - Use Serial Monitor Debugging 0 - monitoring off
#define LCDCOLS 20                  //Our LCD is 20x4
#define LCDROWS 4

#define VERSIONDELAY 1500           // Pause before erasing version date

#define MAXSWEEP 240                // tone decoder chip cycles around if we go too high with the digital pot
#define SWEEPCOUNTBUMP 10           // how much more we will delay checking for tone match after each sweep

#define SCOPEPIN 5                  // Scope Channel 2 D5
#define RIGHT_BUTTON 6              // Switch 2
#define LEFT_BUTTON 7               // Switch 1
#define SIGNALPIN 8                 // Key UP/DOWN is read here from both tone decoder chip and telegraph key
                                    // Hook the key up between D8 and GND
                                    // Also Scope Channel 1 D8
                                    // hook osciliscope up to pins D5 and D8 to see tone decoder at work
int signalPinState = 1;             // will store the value we read on this pin

#define SPEAKERPIN 9                // Used to drive a simple speaker if you want to hear your own code
                                    
#define SLAVESELECTPIN 10           // connected to Pin 1 CS (Chip Select) of digital potentiometer 41010

int myMax =0;                       // used to capture highest value that decodes tone
int myMin =255;                     // used to capture lowest value that decodes tone

int ToneSet = EEPROM.read(addr);    // a value bumped up or down by the left button
int oldToneSet = ToneSet;           // so we can tell when the value has changed
int pitchDir = 1;                   // Determines whether left button increases or decreases side-tone pitch

#include <LiquidCrystal.h>
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(A5, A4, A3, A2, A1, A0);  //M1 boards
//LiquidCrystal lcd(A0, A1, A2, A3, A4, A5);  //M2 boards


// noiseFilter is a value used to filter out noise spikes comming from the decoder chip
// the value of noiseFilter will be the number of milliseconds we wait after reading pin 8
// before reading it again to see if we have a valid key down or key up.
int noiseFilter = EEPROM.read(addr+1);

int oldNoiseFilter = noiseFilter;   // used to see if the value has changed

int sweepCount = 0;                 // used to slow down the sweep each time it cycles without success

// sideToneIndex is a value stored in EEPROM that determines the frequency of the sidetone
// values from 1 to 8 are allowed. The frequency of the tone will be this index times 110 Hz
int sideToneIndex = EEPROM.read(addr+2);
int sideTone = 440;                // the frequency of the side tone

/////////////////////////////////////////////////////////////////////////////////////////
// The following variables store values collected and calculated from millis()
// For various timing functions

long pitchTimer = 0;     // keep track of how long since right button was pressed so we can reverse direction of pitch change
long downTime = 0;       // How long the tone was on in milliseconds
long upTime = 0;         // How long the tone was off in milliseconds

long startDownTime = 0;  // Arduino's internal timer when tone first comes on
long startUpTime = 0;    // Arduino's internal timer when tone first goes off
long lastChange = 0;     // Keep track of when we make changes so that we can
                         // postpone writing changes to EEPROM until all have been made

long lastDahTime = 0;    // Length of last dah in milliseconds
long lastDitTime = 0;    // Length of last dit in milliseconds

                         // The following values will auto adjust to the sender's speed
long averageDah = 100;   // A dah should be 3 times as long as a dit
long fullWait = 6000;    // The time between letters
long waitWait = 6000;    // The time between dits and dahs
long newWord = 0;        // The time between words
long dit = 10;           // We start by defining a dit as 10 milliseconds


////////////////////////////////////////////////////////////////////////////////////////////
// These variables handle line scrolling and word wrap on the LCD panel                   //
int LCDline = 1;            // keeps track of which line we're printing on                //
int lineEnd = LCDCOLS + 1;  // One more than number of characters across display          //
int letterCount = 0;        // keeps track of how may characters were printed on the line //
int lastWordCount = 0;      // keeps track of how may characters are in the current word  //
int lastSpace = 0;          // keeps track of the location of the last 'space'            //
//                                                                                        //
// The next line stores the text that we are currently printing on a line,                //
// The charcters in the current word,                                                     //
// Our top line of text,                                                                  //
// Our second line of text,                                                               //
// and our third line of text                                                             //
// For a 20x4 display these are all 20 characters long                                    //
char currentLine[] = "12345678901234567890";                                              //
char    lastWord[] = "                    ";                                              //
char       line1[] = "                    ";                                              //
char       line2[] = "                    ";                                              //
char       line3[] = "                    ";                                              //
////////////////////////////////////////////////////////////////////////////////////////////

boolean ditOrDah = true;         // We have either a full dit or a full dah
boolean characterDone = true;    // A full character has been received
boolean justDid = true;          // Makes sure we only print one space during long gaps
boolean speaker = false;         // We need to know if a speaker is connected
boolean sideToneSet = false;     // We need to know if we are changing the side tone

int myBounce = 2;                // Used as a short delay between key up and down
int myCount = 0;

int wpm = 0;              // We'll print the sender's speed on the LCD when we scroll

int myNum = 0;           // We will turn dits and dahs into a binary number stored here

/////////////////////////////////////////////////////////////////////////////////
// Now here is the 'Secret Sauce'
// The Morse Code is embedded into the binary version of the numbers from 2 - 63
// The place a letter appears here matches myNum that we parsed out of the code
// #'s are miscopied characters
char mySet[] ="##TEMNAIOGKDWRUS##QZYCXBJP#L#FVH09#8###7#####/-61#######2###3#45";
char lcdGuy = ' ';       // We will store the actual character decoded here

//////////////////////////////////////////////////////////
// define two special characters for LCD a didit and a dah
  byte didit[8] = {
     B00000,
     B00000,
     B11011,
     B11011,
     B11011,
     B00000,
     B00000,
   };   

     byte dahdah[8] = {
     B00000,
     B00000,
     B11111,
     B11111,
     B11111,
     B00000,
     B00000,
   };    

/////////////////////////////////////////////////////////////////////////////////

void setup() {
  pinMode(SIGNALPIN, INPUT); // this reads our key and the tone decoder chip
  pinMode(SCOPEPIN, OUTPUT); // drives the blue LED and scope probe 2
  digitalWrite(SIGNALPIN,1); // turn on internal pull up resistor
  pinMode(LEFT_BUTTON, INPUT); // this reads left button
  digitalWrite(LEFT_BUTTON,1); // turn on internal pull up resistor
  pinMode(RIGHT_BUTTON, INPUT); // this reads right button
  digitalWrite(RIGHT_BUTTON,1); // turn on internal pull up resistor
  
  pinMode(SPEAKERPIN,INPUT);
  digitalWrite(SPEAKERPIN,1); // turn on internal pull up resistor
  
  // Find out if a speaker is attached
  int spk = digitalRead(SPEAKERPIN);
  if (spk==0)speaker = true; // The pin will be grounded through the speaker coil
  
  pinMode(SPEAKERPIN,OUTPUT);// switch from input to output for our side tone
  
  if (sideToneIndex>9) sideToneIndex = 4; // initialize at 440 Hz
  sideTone = sideToneIndex * 110;

  if(noiseFilter>8) noiseFilter=4;        // initialize at 4 ms
  
  SPI.begin();              // The SPI bus is used to set the value of the digital pot 41010
  delay(1000);
  pinMode (SLAVESELECTPIN, OUTPUT);

  if(ToneSet>245)ToneSet=110;  // initialize this value if it has not been stored yet      
  digitalPotWrite(ToneSet);
  

//build LCD graphic character for memu prompt  
//send dadididadah (-..--) to change sidetone to pitch
  //lcd.createChar(0,didit); 
 // lcd.createChar(1,dahdah);

  lcd.begin(LCDCOLS, LCDROWS);      // Cuzz we have a 20x4 display
  printHeader();
  /*
  if (LCDROWS > 2){
    lcd.setCursor(5,3);
    lcd.print(VDATE);
    delay(VERSIONDELAY);
    lcd.setCursor(5,3);
    lcd.print("          ");
    LCDline = 2;
    lcd.setCursor(0,2);
  }
  */
  
 
#ifdef DEBUG  
  Serial.begin(9600);
  Serial.println("Morseduino 02");
  Serial.println(VDATE);
  Serial.print("Speaker is ");
  if (speaker){
    Serial.println("Connected");
  }else{
    Serial.println("Disconnected");
  }


#endif

  
}

///////////////////////////////////////////////////////////////////
// This method sends the value to the digital pot on the SPI bus
// Parameter: 0-245
int digitalPotWrite(int value)
{
  //lcd.print(value,DEC);
  digitalWrite(SLAVESELECTPIN, LOW);
  SPI.transfer(0x11);
  SPI.transfer(value);
  digitalWrite(SLAVESELECTPIN, HIGH);
}

/////////////////////////////////////////////////////////////////////////////////
// This method prints Splash text on LCD
void printHeader(){
  lcd.clear();           // Get rid of any garbage that might appear on startup
  //delay(2000);
  lcd.print(HISCALL);
  lcd.print(" MORSEDUINO 02");
  lcd.setCursor(0,1);
  lcd.print("TS=");
  lcd.print(ToneSet);
  lcd.print(" QRNf=");
  lcd.print(noiseFilter);
  lcd.print(" SPK=");
  lcd.print(speaker);  // 1=speaker connected, 0=speaker disconnected
  lcd.setCursor(0,LCDROWS - 2);
  justDid = true;     // so we don't start with a space at the beginning of the line
}

 void loop() {
  /*
   signalPinState = digitalRead(SIGNALPIN); // What is the tone decoder doing?
   if (!signalPinState) keyIsDown();        // LOW, or 0, means tone is being decoded
   if (signalPinState) keyIsUp();           // HIGH, or 1, means no tone is there
   
   // Now check to see if a buttons is pressed
   if (!digitalRead(LEFT_BUTTON))  changePitch();        
   if (!digitalRead(RIGHT_BUTTON)) checkRIGHT_BUTTON();
   
   // Wait 1.5 seconds before writing changes to EEPROM
   // lastChange will be 0 if no buttons have been pressed
   if (lastChange){
     if (millis()-lastChange > 1500) resetDefaults();
   }
 */


 }
 
 void checkRIGHT_BUTTON(){

   if (sideToneSet){
     // We also use this button to change the side tone if a speaker is attached
     sideToneIndex++;
     if(sideToneIndex>9)sideToneIndex=9;
     setSideTone();
     return;
   }
   
   bumpFilter();
   delay(500); 
   // if button is held down start the auto sweep routine
   if (!digitalRead(RIGHT_BUTTON)) {
     noiseFilter--;  //undo the filter bump
     if (noiseFilter < 0) noiseFilter = 0;
     sweep();
   }
 }
 
 
void bumpFilter(){
  oldNoiseFilter = noiseFilter;
  noiseFilter++;
  if (noiseFilter>8) noiseFilter=0; // wrap value around
  lcd.clear();
  lcd.print("QRN Filter: ");
  lcd.print(noiseFilter);

  // reset the scroll and word wrap variables
  LCDline = 1;
  lcd.setCursor(0,1);
  lastSpace=0;          // clear the last space pointer
  lastWordCount=0;      // clear the last word length
  letterCount = 0;

  // reset the adjust speed variables
  downTime = 0;
  upTime = 0;
  dit = 10;             // We start by defining a dit as 10 milliseconds
  averageDah = 100;     // A dah should be 3 times as long as a dit
  fullWait = 6000;      // The time between letters
  waitWait = 6000;   
  myMax =0;
  myMin =255;
    
}


void resetDefaults(){
  printHeader();
  
  lastSpace=0;          // clear the last space pointer
  lastWordCount=0;      // clear the last word length
  letterCount = 0;
  downTime = 0;
  upTime = 0;
  dit = 10;             // We start by defining a dit as 10 milliseconds
  averageDah = 100;     // A dah should be 3 times as long as a dit
  fullWait = 6000;      // The time between letters
  waitWait = 6000;   
  myMax = 0;
  myMin = 255;
  LCDline = 2;
 
  // only write to EEPROM if value has changed
  if (oldToneSet != ToneSet) EEPROM.write(addr,ToneSet);
  if (oldNoiseFilter != noiseFilter) EEPROM.write(addr+1,noiseFilter);
  lastChange = 0;      // turn timer off until next changes are made
  
  oldToneSet = ToneSet;
  oldNoiseFilter = noiseFilter;
}

void changePitch(){
    if (sideToneSet){
     sideToneIndex--;
     if(sideToneIndex < 1)sideToneIndex=1;
     setSideTone();
     return;
   }

  delay(200); // autorepeat
  
  lastChange = millis();  // reset timer so we know to save change later
  oldToneSet = ToneSet; 
  // if it has been more than 1 second since this button was pressed reverse the
  // direction of the pitch change  
  if (millis() - pitchTimer > 1000){
    pitchDir = pitchDir * -1;
  }
  pitchTimer = millis();
  ToneSet = ToneSet + pitchDir;
  if (ToneSet > 255) ToneSet = 255;
  if (ToneSet < 0) ToneSet = 0;

  if (oldToneSet == ToneSet) return;

  digitalPotWrite(ToneSet);
  lcd.setCursor(0,LCDROWS - 1);
  lcd.print(ToneSet);               // show us the new value
  lcd.print("    ");
}

void sweep()
{
 
  sweepCount = 0;
  oldToneSet = ToneSet;
  lcd.clear();
  lcd.print("Sweep");
  myMin = 255;
  myMax = 0;
 
  // We keep calling the sweep methods as long as our
  // best minimum is greater than our best maximum 
  while (myMin > myMax)
   {
    sweepCount = sweepCount + SWEEPCOUNTBUMP; // used to create a delay that increases
                                  // with each pass
    #ifdef DEBUG
    Serial.print("\nsweepCount=");
    Serial.println(sweepCount);
    #endif
    
    sweepUp();
    sweepDown();
   }
    // Find a value that is half way between the min and max
    ToneSet = (myMax - myMin)/2;
    ToneSet = ToneSet + myMin;
    digitalPotWrite(ToneSet);   // send this value to the digital pot
    #ifdef DEBUG
    Serial.print("\nTone Parked at:");
    Serial.println(ToneSet);
    #endif
    lcd.clear();
    resetDefaults();   // store the changes right away don't wait
}

void sweepUp()
{
 
  #ifdef DEBUG  
  Serial.println("\nSweep Up");  
  #endif
  
  lcd.clear();
  lcd.print("Sweep Up");
  //lcd.setCursor(8,0);
  
  for (ToneSet = 0; ToneSet <= MAXSWEEP; ToneSet+=2)
    {
      digitalPotWrite(ToneSet);
      delay(sweepCount); // this delay gets longer with each pass
      if (!digitalRead(8)){
        if (ToneSet > myMax){
          myMax=ToneSet;
          #ifdef DEBUG
          Serial.print("myMax=");
          Serial.println(myMax);
          #endif
          lcd.print('.'); // show our hits on the display
          }
        }
    }
 }
 
void sweepDown(){
  #ifdef DEBUG  
  Serial.println("\nSweep Down");  
  #endif


  lcd.clear();
  lcd.print("Sweep Down");
  //lcd.setCursor(10,0);
  
     for ( ToneSet = MAXSWEEP; ToneSet >= 0; ToneSet-=2)
    {

      digitalPotWrite(ToneSet);
      delay(sweepCount);
      if (!digitalRead(8)){
        if (ToneSet < myMin){
          myMin=ToneSet;
          #ifdef DEBUG
          Serial.print("myMin=");
          Serial.println(myMin);
          #endif
          lcd.print('.');
          }
       }
    }
}


  


 void keyIsDown() {
   // The LEDs on the decoder and Arduino will blink on in unison
   //digitalWrite(13,1);            // turn on Arduino's LED
   if (noiseFilter) delay(noiseFilter);
   signalPinState = digitalRead(SIGNALPIN); // What is the tone decoder doing?
 if (signalPinState) return;
   
   digitalWrite(SCOPEPIN,0);
   if (speaker) tone (SPEAKERPIN,sideTone);
   if (startUpTime>0){  
     // We only need to do once, when the key first goes down
     startUpTime=0;    // clear the 'Key Up' timer
     }
   // If we haven't already started our timer, do it now
   if (startDownTime == 0){
       startDownTime = millis();  // get Arduino's current clock time
      }

     characterDone=false; // we're still building a character
     ditOrDah=false;      // the key is still down we're not done with the tone
     delay(myBounce);     // Take a short breath here
     
   if (myNum == 0) {      // myNum will equal zero at the beginning of a character
      myNum = 1;          // This is our start bit  - it only does this once per letter
      }
 }
 
  void keyIsUp() {
   // The LED on the Arduino will blink on and off with the code 
   //digitalWrite(13,0);    // turn off Arduino's LED
   if (noiseFilter) delay(noiseFilter);
   signalPinState = digitalRead(SIGNALPIN); // What is the tone decoder doing?
   if (!signalPinState) return;
       
   
   // If we haven't already started our timer, do it now
   if (startUpTime == 0){startUpTime = millis();}
   
   // Find out how long we've gone with no tone
   // If it is twice as long as a dah print a space
   
   upTime = millis() - startUpTime;
   
  if (upTime<20) return;
  
   if (speaker) noTone(SPEAKERPIN);
   digitalWrite(SCOPEPIN,1);


   
   if (upTime > (averageDah*2)) {    
      printSpace();
   }
   
   // Only do this once after the key goes up
   if (startDownTime > 0){
     downTime = millis() - startDownTime;  // how long was the tone on?
     startDownTime=0;      // clear the 'Key Down' timer
   }
 
   if (!ditOrDah) {   
     // We don't know if it was a dit or a dah yet
      shiftBits();    // let's go find out! And do our Magic with the bits
    }

    // If we are still building a character ...
    if (!characterDone) {
       // Are we done yet?
       if (upTime > dit) { 
         // BINGO! we're done with this one  
         printCharacter();       // Go figure out what character it was and print it       
         characterDone=true;     // We got him, we're done here
         myNum=0;                // This sets us up for getting the next start bit
         }
         downTime=0;               // Reset our keyDown counter
       }
   }
   
   
void shiftBits() {
  // we know we've got a dit or a dah, let's find out which
  // then we will shift the bits in myNum and then add 1 or not add 1
  
  if (downTime < dit/3) return;  // ignore my keybounce
  myNum = myNum << 1;   // shift bits left
  ditOrDah = true;        // we will know which one in two lines 
  
  
  // If it is a dit we add 1. If it is a dah we do nothing!
  if (downTime < dit) {
     myNum++;           // add one because it is a dit
     } else {
  
    // The next three lines handle the automatic speed adjustment:
    averageDah = (downTime+averageDah) / 2;  // running average of dahs
    dit = averageDah / 3;    
    // normal dit would be this
    dit = dit * 2;    // double it to get the threshold between dits and dahs
     }
  }

void setSideTone(){
  // Send dadididada ( -..--) to enter and exit this mode
  if (!sideToneSet) lcd.clear();
  lcd.setCursor(0,0);
  delay(200);
  sideTone = sideToneIndex * 110;
  lcd.print("SIDE TONE FREQ:");
  lcd.print(sideTone);
  lcd.print("Hz");
  lcd.setCursor(1,1);
  lcd.print("Send: ");
  lcd.write(byte(1));
  lcd.write(byte(0));
  lcd.write(byte(1));
  lcd.write(byte(1));
  lcd.print(" to exit");
  lcd.setCursor(0,2);
  lcd.print("LT/RT = LOWER/HIGHER");
  lcd.setCursor(0,3);
  LCDline = 3;
  justDid = true;
  sideToneSet=true;
  tone (SPEAKERPIN,sideTone);
  delay(100);
  noTone(speaker);
}

void printCharacter() {           
  
  justDid = false;         // OK to print a space again after this
  
  if (myNum==44 && speaker){
    if (!sideToneSet){
      setSideTone();
    }else{
      sideToneSet = false;
      EEPROM.write(addr+2,sideToneIndex);

      resetDefaults();
    }
    return;
  }
  
  // Punctuation marks will make a BIG myNum
  if (myNum > 63) {  
    printPunctuation();  // The value we parsed is bigger than our character array
                         // It is probably a punctuation mark so go figure it out.
    return;              // Go back to the main loop(), we're done here.
  }
  lcdGuy = mySet[myNum]; // Find the letter in the character set
  sendToLCD();           // Go figure out where to put in on the display
}

void printSpace() {
  if (justDid) return;  // only one space, no matter how long the gap
  justDid = true;       // so we don't do this twice
  //Farns = 2 + !digitalRead(farnsRead)*2;


  lastWordCount=0;      // start counting length of word again
  currentLine[letterCount]=' ';  // add a space to the variable that stores the current line
  lastSpace=letterCount;         // keep track of this, our last, space
  
  // Now we need to clear all the characters out of our last word array
  for (int i=0; i<20; i++) {
    lastWord[i]=' ';
   }
   
  lcdGuy=' ';            // this is going to go to the LCD 
  
  // We don't need to print the space if we are at the very end of the line
  if (letterCount < 20) { 
    sendToLCD();         // go figure out where to put it on the display
 }
}

void printPunctuation() {
  // Punctuation marks are made up of more dits and dahs than
  // letters and numbers. Rather than extend the character array
  // out to reach these higher numbers we will simply check for
  // them here. This funtion only gets called when myNum is greater than 63
  
  // Thanks to Jack Purdum for the changes in this function
  // The original uses if then statements and only had 3 punctuation
  // marks. Then as I was copying code off of web sites I added
  // characters we don't normally see on the air and the list got
  // a little long. Using 'switch' to handle them is much better.


  switch (myNum) {
    case 71:
      lcdGuy = ':';
      break;
    case 76:
      lcdGuy = ',';
      break;
    case 84:
      lcdGuy = '!';
      break;
    case 94:
      lcdGuy = '-';
      break;
    case 97:
      lcdGuy = 39;    // Apostrophe
      break;
    case 101:
      lcdGuy = '@';
      break;
    case 106:
      lcdGuy = '.';
      break;
    case 115:
      lcdGuy = '?';
      break;
    case 246:
      lcdGuy = '$';
      break;
    case 122:
      lcdGuy = 's';
      sendToLCD();
      lcdGuy = 'k';
      break;
    default:
      lcdGuy = '#';    // Should not get here
      break;
  }
  sendToLCD();    // go figure out where to put it on the display
}

void sendToLCD(){
  // Do this only if the character is a 'space'
  if (lcdGuy > ' '){
   lastWord[lastWordCount] = lcdGuy; // store the space at the end of the array
   if (lastWordCount < lineEnd - 1) {
     lastWordCount++;   // only bump up the counter if we haven't reached the end of the line
   }
  }
  currentLine[letterCount] = lcdGuy; // now store the character in our current line array
 
  letterCount++;                     // we're counting the number of characters on the line
  if(letterCount==lineEnd-5) clearSpeed();
  if(letterCount > 5){
      // to extend EEPROM life we will only write when a charcter is printed and the value has changed.
      if (noiseFilter != oldNoiseFilter) {
        EEPROM.write(addr+1,noiseFilter);
        oldNoiseFilter = noiseFilter;
        //Serial.println("noiseFilter saved to EEPROM");
      }
  }

  // If we have reached the end of the line we will go do some chores
  if (letterCount == lineEnd) {
    newLine();  // check for word wrap and get ready for the next line
    return;     // so we don't need to do anything more here
  }
  
  lcd.print(lcdGuy); // print our character at the current cursor location
 
}

//////////////////////////////////////////////////////////////////////////////////////////
// The following functions handle word wrapping and line scrolling for a 4 line display //
//////////////////////////////////////////////////////////////////////////////////////////

void newLine() {
  // sendToLCD() will call this routine when we reach the end of the line
  if (lastSpace == 0){
    // We just printed an entire line without any spaces in it.
    // We cannot word wrap this one so this character has to go at 
    // the beginning of the next line.
    
    // First we need to clear all the characters out of our last word array
    for (int i=0; i<20; i++) {
      lastWord[i]=' ';
     }
     
     lastWord[0]=lcdGuy;  // store this character in the first position of our next word
     lastWordCount=1;     // set the length to 1
   }
  truncateOverFlow();    // Trim off the first part of a word that needs to go on the next line
  
  linePrep();            // Store the current line so we can move it up later
  reprintOverFlow();     // Print the truncated text and space padding on the next line 
  printHisSpeed();
  }
  
void clearSpeed(){
  lcd.setCursor(lineEnd-3,3);
  lcd.print("  ");
  lcd.setCursor(letterCount-1, LCDline);
}

void printHisSpeed(){
  lcd.setCursor(lineEnd-3,3);
  wpm = dit/2;
  wpm = 1400 / wpm ;
  //wpm = wpm + wpm * .08);
  lcd.print(wpm);
  lcd.setCursor(letterCount, LCDline); 
}

void truncateOverFlow(){
  // Our word is running off the end of the line so we will
  // chop it off at the last space and put it at the beginning of the next line
  
  if (lastSpace==0) {return;}  // Don't do this if there was no space in the last line
  
  // Move the cursor to the place where the last space was printed on the current line
  lcd.setCursor(lastSpace,LCDline);
  
  letterCount = lastSpace;    // Change the letter count to this new shorter length
  
  // Print 'spaces' over the top of all the letters we don't want here any more
  for (int i = lastSpace; i < 20; i++) {
     lcd.print(' ');         // This space goes on the display
     currentLine[i] = ' ';   // This space goes in our array
  }
}


void linePrep(){
     LCDline++;           // This is our line number, we make it one higher
     
     // What we do next depends on which line we are moving to
     // The first three cases are pretty simple because we working on a cleared
     // screen. When we get to the bottom, though, we need to do more.
     switch (LCDline) {
     case 1:
       // We just finished line 0
       // don't need to do anything because this for the top line
       // it is going to be thrown out when we scroll anyway.
       break;
     case 2:
       // We just finished line 1
       // We are going to move the contents of our current line into the line1 array
       for (int j=0; j<20; j++){
         line1[j] = currentLine[j];
       }
        break;
     case 3:
       // We just finished line 2
       // We are going to move the contents of our current line into the line2 holding bin
       for (int j=0; j<20; j++){
         line2[j] = currentLine[j];
       }
       break;
     case 4:
       // We just finished line 3
       // We are going to move the contents of our current line into the line3 holding bin
       for (int j=0; j<20; j++){
         line3[j] = currentLine[j];
       }
       //This is our bottom line so we will keep coming back here
       LCDline = 3;  //repeat this line over and over now. There is no such thing as line 4
       
       myScroll();  //move everything up a line so we can do the bottom one again
       break;
   }
        
}

void myScroll(){
  // We will move each line of text up one row
  
  int i = 0;  // we will use this variables in all our for loops
  
  lcd.setCursor(0,0);      // Move the cursor to the top left corner of the display
  lcd.print(line1);        // Print line1 here. Line1 is our second line,
                           // our top line is line0 ... on the next scroll
                           // we toss this away so we don't store line0 anywhere
 
  // Move everything stored in our line2 array into our line1 array
  for (i = 0; i < 20; i++) {
    line1[i] = line2[i];
  }
  
  lcd.setCursor(0,1);      // Move the cursor to the beginning of the second line
  lcd.print(line1);        // Print the new line1 here
 
  // Move everything stored in our line3 array into our line2 array
  for (i = 0; i < 20; i++) {
    line2[i]=line3[i];
  }
  lcd.setCursor(0,2);      // Move the cursor to the beginning of the third line
  lcd.print(line2);        // Print the new line2 here
 
  // Move everything stored in our currentLine array into our line3 array
  for (i = 0; i < 20; i++) {
    line3[i] = currentLine[i];
  }

}

void reprintOverFlow(){
  // Here we put the word that wouldn't fit at the end of the previous line
  // Back on the display at the beginning of the new line
  
  // Load up our current line array with what we have so far
   for (int i = 0; i < 20; i++) {
     currentLine[i] = lastWord[i];
  } 
    

  lcd.setCursor(0, LCDline);              // Move the cursor to the beginning of our new line 
  lcd.print(lastWord);                    // Print the stuff we just took off the previous line
  letterCount = lastWordCount;            // Set up our character counter to match the text
  lcd.setCursor(letterCount, LCDline); 
  lastSpace=0;          // clear the last space pointer
  lastWordCount=0;      // clear the last word length
}
