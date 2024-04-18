/*
 * Alarm Clock Code!
 * 
 * Clock functionality inspired by simplest_ever_UNO_digital_clock by plouc68000
 * https://create.arduino.cc/projecthub/plouc68000/simplest-uno-digital-clock-ever-4613aa
 */

#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

SoftwareSerial HC12(8,9);
// define buffer for receiving data from hc12 chip
const int RECEIVE_BUFFER_SIZE = 4;
int receiveBuffer[RECEIVE_BUFFER_SIZE];

int h = 12;
int m = 58;
int s = 20;
int flag = 1; //PM

unsigned long last_loop;
unsigned long last_button_press;

int alarm[3];
int alarm_playing = 0;
int alarm_set = 0;
int pre_open = 0;
boolean new_alarm_time_received = false;

// Pins definition for Time Set Buttons
int hs = 6; // pin 0 for Hours Setting
int ms = 7; // pin 1 for Minutes Setting

LiquidCrystal lcd(12, 10, 5, 4, 3, 2);

//melody vars

int melody[] = {
  0, 698, 784, 880, 1046, 698, 659, 1175
};

int durations[] = {
  8, 4, 4, 4, 12, 8, 8, 8
};

const int alarm_pin = 11;
unsigned long last_note = 0;
int numberOfNotes = sizeof(melody) / sizeof(int);
int curr_note = numberOfNotes;


void setup() {
  Serial.begin(9600);
  HC12.begin(9600);
  lcd.begin(16, 2);
  last_loop = millis(); // read RTC initial value
  last_button_press = millis(); // fake button press

  pinMode(alarm_pin, OUTPUT);
}

void loop() {
  // print time on line 1
  lcd.setCursor(0, 0);
  lcd.print("Time ");
  if (h < 10)lcd.print("0");
  lcd.print(h);
  lcd.print(":");
  if (m < 10)lcd.print("0");
  lcd.print(m);
  // uncomment to add seconds to the clock
//   lcd.print(":");
//   if(s<10)lcd.print("0");
//   lcd.print(s);

  if (flag == 0) lcd.print(" AM");
  if (flag == 1) lcd.print(" PM");

  lcd.setCursor(0, 1); // for Line 2
  
  if (alarm_playing == 1) {
    lcd.print("Wake Up!!       ");
  } else if (alarm_set == 1) {
    lcd.print("Alarm: ");
    if (alarm[0] < 10)lcd.print("0");
    lcd.print(alarm[0]);
    lcd.print(":");
    if (alarm[1] < 10)lcd.print("0");
    lcd.print(alarm[1]);
    if (alarm[2] == 0) lcd.print(" AM");
    if (alarm[2] == 1) lcd.print(" PM");
  } else {
    lcd.print("                "); //clear the second line
  }


  if (millis() - last_loop > 997) {
    s = s + 1; // increment seconds 
    last_loop = millis();
  }

  int button1 = digitalRead(hs); // Read Buttons
  int button2 = digitalRead(ms);

  // button one pressed
  if(millis() - last_button_press > 200 and last_button_press < millis()) {
    if (button1 == 1) { //snooze
      if (alarm_playing) {
        alarm_playing = 0;
        alarm_set = 1;
        pre_open = 1;
        alarm[1] += 9; // add 9 mins to timer
        HC12.write(2); // lower the blinds 
        HC12.write(6); // close blinds 

        // increment clock properly
        if (alarm[1] > 60) {
          alarm[1] = alarm[1] - 60;
          alarm[0] = alarm[0] + 1;
          if(alarm[0] == 12) {
            alarm[2] = alarm[2] + 1;
  
            if (alarm[2] == 2) {
              alarm[2] = 0;
            }
          }
        }
        if (alarm[0] == 13) {
          alarm[0] = 1;
        }
        last_button_press = millis() + 2000; // don't allow button presses for 2 seconds after snooze
      } else {
        h = h + 1; // increment hours 
        if (h == 12) {
          flag = flag + 1;
      
          if (flag == 2)flag = 0;
        }
        last_button_press = millis();
      }
    }
  }

  // button two pressed
  if(millis() - last_button_press > 200 and last_button_press < millis()) {
    if (button2 == 1) {
      if (alarm_playing) {
        alarm_playing = 0; //stop the alarm
        alarm[0] = 0;
        last_button_press = millis() + 2000; // don't allow button presses for 2 seconds after stop
      } else {
        s = 0;
        m = m + 1; // increment minutes 
        if( m == 60 ) {
          m = 0;
        }
        last_button_press = millis();
      }
    }
  }

  // increment the clock
  if (s == 60) {
    s = 0;
    m = m + 1;
  }
  if (m == 60)
  {
    m = 0;
    h = h + 1;
    if (h == 12) {
      flag = flag + 1;

      if (flag == 2)flag = 0;
    }
  }
  if (h == 13)
  {
    h = 1;
  }

  //calculate the next minute to trigger pre-opening
  int next_time[3] = {h, m, flag};
  next_time[1] += 1;
  if (next_time[1] == 60)
  {
    next_time[1] = 0;
    next_time[0] = next_time[0] + 1;
    if (next_time[0] == 12) {
      next_time[2] = next_time[2] + 1;

      if (next_time[2] == 2)next_time[2] = 0;
    }
  }
  if (next_time[0] == 13)
  {
    next_time[0] = 1;
  }

  // pre-open the blinds 
  if (next_time[0] == alarm[0] and next_time[1] == alarm[1] and next_time[2] == alarm[2] and pre_open == 1) { // alarm sound
    Serial.println("open blinds");
    pre_open = 0;
    HC12.write(7); // open blinds 
  }
  
  // Ring the alarm
  if (h == alarm[0] and m == alarm[1] and flag == alarm[2] and s == 0 and alarm_playing == 0) { // alarm sound
    HC12.write(1); // raise the blinds 
    curr_note = 0;
    last_note = millis();
    alarm_playing = 1; //set the alarm flag
    alarm_set = 0;
  }

  // play alarm tones
  if (alarm_playing == 1) {
    if (curr_note < numberOfNotes) {
      tone(alarm_pin, map(melody[curr_note], 600, 1200, 200, 300)); //re-mapped original values to work with the sine wave tinkering

      if (millis() - last_note > (48 * durations[curr_note])) {
        curr_note ++;
        last_note = millis();
      }
    } else {
      curr_note = 0;
    }
  } else {
    noTone(alarm_pin);
  }

  int buffer_ind = 0;
  while(HC12.available()){
    // TODO check if newline char is received
    Serial.print("Received: ");
    int readByte = (int) HC12.read();
    Serial.print(readByte);
    if(!HC12.available()){ Serial.print('\n');}
    receiveBuffer[buffer_ind] = readByte;
    buffer_ind++;
    new_alarm_time_received = true;
  }
  while(Serial.available()){
    HC12.write(Serial.read());
  }
  if(new_alarm_time_received){
    if(receiveBuffer[0] == 4){
      
      // time interval mode - convert time
      // set alarm to current time first, then we will increment it
      alarm[0] = h;
      alarm[1] = m;
      alarm[2] = flag;
      // Increment minute first, if alarm time > 60 then increment the hour by 1
      if(s > 30) {
        receiveBuffer[2] += 1;
      }
      if(alarm[1]+receiveBuffer[2] >= 60){
        alarm[1] = alarm[1] + receiveBuffer[2] - 60;
        alarm[0]++;
        if(alarm[0] == 12){ //if hour is 12 switch am/pm
          if(alarm[2]==0){alarm[2]=1;}
          else{alarm[2]=0;}
        }
      }
      else{
        alarm[1] = alarm[1]+receiveBuffer[2];
      }
      // Increment the hour, if hour alarm time > 12 subtract 12 and change the flag
      if(alarm[0] < 12 && alarm[0] + receiveBuffer[1] >= 12){
        if(alarm[2]==0){alarm[2]=1;}
        else{alarm[2]=0;}
      }
      
      if(alarm[0] + receiveBuffer[1] > 12){
        alarm[0] = alarm[0] + receiveBuffer[1] - 12;
      } 
      
      else{
        alarm[0] = alarm[0] + receiveBuffer[1];
      }
      alarm_set = 1;
      pre_open = 1;
      Serial.print("alarm time ");
      for(int i = 0; i<2;i++){
        Serial.print(alarm[i]);
      }
      Serial.print('\n');
    }
    else if(receiveBuffer[0] == 5){
      // absolute time - store
      // convert 24h time to 12h time
      if(receiveBuffer[1] == 0){
        alarm[0] = 12;
        alarm[2] = 0;
      }
      else if(receiveBuffer[1] > 12){
        alarm[0] = receiveBuffer[1] - 12;
        alarm[2] = 1;
      }
      else{
        alarm[0] = receiveBuffer[1];
        alarm[2] = 0;
      }
      alarm[1] = receiveBuffer[2];
    
      alarm_set = 1;
      pre_open = 1;
      Serial.print("alarm time ");
      for(int i = 0; i<2;i++){
        Serial.print(alarm[i]);
      }
      Serial.print('\n');
    }
    new_alarm_time_received = false;
  }
  

}
