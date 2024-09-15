/////////////////////////////////////////////////////////////////
// CPE 301 Semester Project
// Eric Valdez & Emily Hernandez
/////////////////////////////////////////////////////////////////
#include <LiquidCrystal.h> //lcd library
#include <Servo.h> //servo motor library
#include <dht.h> //temperature sensor library
#include "RTClib.h" //real time clock library

RTC_DS1307 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define DHTPIN 2 //pin used for temperature sensor

dht DHT;

//pin declarations for the LCD
int rs = 7;
int en = 8;
int d4 = 12;
int d5 = 11;
int d6 = 10;
int d7 = 9;

LiquidCrystal lcd(rs, en, d4, d5, d6, d7); //LCD setup
Servo servo; //motor setup

int fan = 13; //fan pin

//timer pointers
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F;
volatile unsigned int  *myTCNT1  = (unsigned  int *) 0x84;
volatile unsigned char *myTIFR1 =  (unsigned char *) 0x36;

//pointers to port f for the button
volatile unsigned char* ddr_f  = (unsigned char*) 0x30;
volatile unsigned char* port_f = (unsigned char*) 0x31;
volatile unsigned char* pin_f  = (unsigned char*) 0x2F;

//pointers to port k for the LED
volatile unsigned char* ddr_k  = (unsigned char*) 0x107;
volatile unsigned char* port_k = (unsigned char*) 0x108;
volatile unsigned char* pin_k  = (unsigned char*) 0x106;

//forward declarations
unsigned int calc_ticks(unsigned int freq);
void my_delay(unsigned int ticks);
void primaryCase(unsigned int ticks);
void sysReset();
int distanceCheck();
void printTemp();
bool handLost(int dist);

unsigned long duration; //variable used for calculating distance
unsigned int distance; //variable used to store distance
int echoPin = 5; //echoPin set to pin 5
int trigPin = 6; //trigPin set to pin 6

//setup function
void setup() {
  lcd.begin(16, 2);
  *ddr_f &= 0x7F; //set port F to input for the button
  *ddr_k |= 0x01; //set port K to output for the LED
  *myTCCR1A = 0x00;
  *myTCCR1B = 0x00;
  *myTCCR1C = 0x00;
  pinMode(trigPin, OUTPUT); //trigPin setup for distance sensor
  pinMode(echoPin, INPUT); //echoPin setup for distance sensor
  servo.attach(4); //servo motor set to pin 4
  servo.write(0); //set motor to position 0 aka "closed" in this simulation
  pinMode(fan, OUTPUT); //fan setup
  Serial.begin(57600);


#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

unsigned int ticks; //variable used for the timer ticks
unsigned int ticks2; //variable used for the timer used with the distance sensor

//loop function
void loop() {
  analogWrite(fan, 0); //make sure fan begins off
  ticks = calc_ticks(1); //create ticks for a one second delay
  ticks2 = ticks/1000; //create ticks for a millisecond delay
  
  if(*pin_f & 0x80) //checks to see if button was pressed
  {
    for(int del = 0; del < 30000; del++) //acts as a delay for the debouncing
    {}
    if(*pin_f & 0x80) //if button is still pressed
    {
      while(*pin_f & 0x80) //make sure only one action is taken per press
      {}
      DateTime now = rtc.now(); //create RTC object

      Serial.print(now.year(), DEC); //print year
      Serial.print('/');
      Serial.print(now.month(), DEC); //print month
      Serial.print('/');
      Serial.print(now.day(), DEC); //print day
      Serial.print(" (");
      Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
      Serial.print(") ");
      Serial.print(now.hour(), DEC); //print start hour
      Serial.print(':');
      Serial.print(now.minute(), DEC); //print start minute
      Serial.print(':');
      Serial.print(now.second(), DEC); //print start second
      Serial.println();

      primaryCase(ticks, ticks2); //begin primary case
      sysReset(); //reset system
    }
  }
}

//function to calculate the ticks of the delay
unsigned int calc_ticks(unsigned int freq) {
  //Given a frequency, calculate the # of ticks
  float period = 1.0/freq; //calculate period
  float dutyCycled = period/2.0; //uses 50% duty cycle
  float clkTick = 0.0000000625; //16Mhz clock 
  unsigned int ticks = dutyCycled/clkTick; //calculate ticks
  return ticks;
}

//function to create the delay
void my_delay(unsigned int ticks)
{
  // stop the timer
  *myTCCR1B &= (0x00);
  // set the counts
  *myTCNT1 = (unsigned int) (65536 - ticks);
  // start the timer
  * myTCCR1B |= (0x04);
  // wait for overflow
  while((*myTIFR1 & (0x01))==0); // 0b 0000 0000
  // stop the timer
  *myTCCR1B &= (0x00);   // 0b 0000 0000
  // reset TOV - You actually have to set this bit to 1 to reset it, not 0 as would be logical (don't ask)
  *myTIFR1 |= (0x01);
}

//function to handle the main parts of the program
void primaryCase(unsigned int ticks, unsigned int ticks2)
{
    int dist = 10000; //set distance to high number to wait for hand to be presented
    digitalWrite(trigPin, LOW);
    while(dist > 20) //waits for a hand to be presented within arbitrary 20cm
    {
      dist = distanceCheck(); //check for hand every second
    }
    int chk = DHT.read11(DHTPIN);
    servo.write(180); //simulate turning on the faucet
    for( int count = 29; count>8; count-=2) //count from 30-10 decrementing every two seconds
    {
      printTemp();        
      lcd.setCursor(0,1); //set LCD to bottom left
      lcd.print(count + 1); //display time
      dist = distanceCheck(); //delay one second and calculate distance
      if(dist > 20) //check if hand has left the arbitrary boundary of 20cm
      {
        if(handLost(dist))//flash warning LED and check if hand returns for 10 seconds
        {
          primaryCase(ticks, ticks2);
          return;
        }
        return; //hand never came back so exit function and reset system
      }
      if(count != 9) //check to see if time left is a two digit number
      {
        lcd.setCursor(0,1); //set LCD to bottom left
        lcd.print(count); //display time
        dist = distanceCheck(); //delay one second and calculate distance
        if(dist > 20) //check if hand has left the arbitrary boundary of 20cm
        {
          if(handLost(dist))//flash warning LED and check if hand returns for 10 seconds
          {
            primaryCase(ticks, ticks2);
            return;
          }
          return; //hand never came back so exit function and reset system
        }
      }
    }
    
    lcd.clear(); //clear to allow for single digits to appear correctly
    
    for( int count = 8; count>-1; count-=2) //count from 9-0 decrementing every two seconds
    {
      printTemp();
      lcd.setCursor(0,1); //set LCD to bottom left
      lcd.print(count + 1); //display time
      dist = distanceCheck(); //delay one second and calculate distance
      if(dist > 20) //check if hand has left the arbitrary boundary of 20cm
      {
        if(handLost(dist))//flash warning LED and check if hand returns for 10 seconds
        {
          primaryCase(ticks, ticks2);
          return;
        }
        return; //hand never came back so exit function and reset system
      }
      
      lcd.setCursor(0,1); //set LCD to bottom left
      lcd.print(count); //display time
      dist = distanceCheck(); //delay one second and calculate distance
      if(dist > 20) //check if hand has left the arbitrary boundary of 20cm
      {
        if(handLost(dist))//flash warning LED and check if hand returns for 10 seconds
        {
          primaryCase(ticks, ticks2);
          return;
        }
        return; //hand never came back so exit function and reset system
      }
    }
    lcd.clear(); //clear screen
    lcd.setCursor(0,0); //set LCD to top left side
    lcd.print("Hands Clean"); //display the string "Hands Clean" on LCD
    *port_k |= 0x01; //Light up green LED for completed handwashing
    analogWrite(fan, 255); //start fan
    for(int i = 0; i < 14; i++) //loop for 15 seconds
    {
      my_delay(ticks); //one second delay
    }
}

//function to handle all system reset requirements
void sysReset() 
{
  lcd.clear(); //LCD clears
  servo.write(0); //simulate turning off the faucet
  *port_k &= 0x00; //turn off LED
  analogWrite(fan, 0); //stop fan
}

//function for calculating distance and using a one second delay
int distanceCheck()
{
   digitalWrite(trigPin, HIGH);
   my_delay(ticks); //one second delay
   digitalWrite(trigPin, LOW);
   duration = pulseIn(echoPin, HIGH); 
   distance = duration * 0.034/2; //calculate distance
   //Serial.println(distance);
   return distance; //return the distance
}

//function to print the temperature to the LCD
void printTemp()
{
   lcd.setCursor(0,0); //set LCD to top left side
   lcd.print("Temp: "); //display the string "Temp:" on LCD
   lcd.print(DHT.temperature); //Print temperature in celsius
   lcd.print((char)223); //display degree symbol
   lcd.print("C"); //denote celsius
}

//function for if the hand is lost
bool handLost(int dist)
{
  lcd.setCursor(0,1); //set LCD to bottom left
  lcd.print("30"); //put 30 seconds back on the timer
  for(int i = 0; i < 5; i++) //flash LED as warning for 10 seconds
  {
    *port_k |= 0x01; //turn on LED
    dist = distanceCheck(); //check for distance again after one second
    
    if(dist < 20) //if hand has come back
    {
      *port_k &= 0x00; //turn off LED
      return true;
      //primaryCase(ticks, ticks2); //begin the function again
    } 
    
    *port_k &= 0x00; //turn off LED
    dist = distanceCheck(); //check for distance again after one second
    if(dist < 20) //if hand has come back
    {
      *port_k &= 0x00; //turn off LED
      return true;
      //primaryCase(ticks, ticks2); //begin the function again
    } 
  }
  return false;
}
