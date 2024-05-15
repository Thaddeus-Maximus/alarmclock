#include "DFMiniMp3.h"
#include "TM1637Display.h"
#include "encoder.h"
#include "RTC.h"

TM1637Display display(2, 3); // CLK, DIO

RotaryEncoder encoder(D8, D12,  RotaryEncoder::LatchMode::TWO03);
void encoderISR() {encoder.tick();}


#define PIN_IN1 D8
#define PIN_IN2 D12
#define BTN 7

DFMiniMp3 dfmp3(Serial1);


int volume = 15;
int dir = +2;
short alarmFlag = 0;
int brightness = 7;
int lastEncoded;

void setup() {
  RTC.begin();
  RTCTime startTime(7, Month::JUNE, 2023, 0, 0, 00, DayOfWeek::WEDNESDAY, SaveLight::SAVING_TIME_INACTIVE);
  RTC.setTime(startTime);

  attachInterrupt(digitalPinToInterrupt(encoder._pin1), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoder._pin2), encoderISR, CHANGE);
  pinMode(BTN, INPUT_PULLUP);

  Serial.begin(115200);

  Serial.println("initializing...");
  
  dfmp3.begin();

  // during development, it's a good practice to put the module
  // into a known state by calling reset().  
  // You may hear popping when starting and you can remove this 
  // call to reset() once your project is finalized
  dfmp3.reset();

  dfmp3.setVolume(volume);

  display.setBrightness(brightness);

  lastEncoded = encoder.getPosition();
}

RTCTime alarmTime;

int getAcceleration() {
  return abs(encoder.getRPM()) > 300 ? 5:1;
}

void alarmCallback() {
  Serial.println("ALARM!!!");
  alarmFlag = 1;
}

enum adjustment {CANCEL, ALARM, VOLUME, BRIGHTNESS, TIME};

// display "ALAr"
const uint8_t SEG_TIME[] = {
SEG_C | SEG_E | SEG_G,           // n
SEG_D | SEG_E | SEG_C | SEG_G,   // o
SEG_D | SEG_E | SEG_C,           // u
SEG_D | SEG_C                    // _|
};
// display "ALAr"
const uint8_t SEG_ALARM[] = {
SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // A
SEG_F | SEG_E | SEG_D ,                        // L
SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G, // A
SEG_E | SEG_G                                  // r
};
// display "VOL"
const uint8_t SEG_VOLUME[] = {
SEG_F | SEG_E | SEG_C | SEG_D | SEG_B,         // v
SEG_D | SEG_E | SEG_C | SEG_G,                 // o
SEG_F | SEG_E | SEG_D ,                        // L
0                                              // 
};
// display "Lu"
const uint8_t SEG_BRIGHTNESS[] = {
SEG_F | SEG_E | SEG_D ,          // L
SEG_D | SEG_E | SEG_C,           // u
0,
0 
};


void loop() 
{
  RTCTime currentTime;
  RTC.getTime(currentTime);

  display.showNumberDecEx(currentTime.getHour()*100+currentTime.getMinutes(), 0b01000000, true);  // number, dot mask, show leading zeroes, Expect: 00:00
  
  
  if (alarmFlag < 0) {
    int delta = encoder.getPosition() - lastEncoded;
    alarmFlag += delta;
    if (alarmFlag >= 0) {
      dfmp3.stop();
      alarmFlag = 0;
    }
  } else {
    int delta = encoder.getPosition() - lastEncoded;
    lastEncoded += delta;

    brightness += delta;
    if (brightness > 7) brightness = 7;
    if (brightness < 0) brightness = 0;
    if (delta) {
      display.setBrightness(brightness);
    }
  }

  lastEncoded = encoder.getPosition();

  if (!digitalRead(BTN)) { // if button pressed
    uint32_t pushstart = millis();
    delay(20);

    uint8_t mode = CANCEL;

    while(!digitalRead(BTN)){
             if (millis()-pushstart > 3500) {
        mode = BRIGHTNESS;
      } else if (millis()-pushstart > 2500) {
        mode = TIME;
      } else if (millis()-pushstart > 1500) {
        mode = ALARM;
      } else if (millis()-pushstart > 500) {
        mode = VOLUME;
      }

      if (mode == CANCEL) display.clear();
      if (mode == BRIGHTNESS) display.setSegments(SEG_BRIGHTNESS);
      if (mode == TIME) display.setSegments(SEG_TIME);
      if (mode == ALARM) display.setSegments(SEG_ALARM);
      if (mode == VOLUME) display.setSegments(SEG_VOLUME);
    }

    uint32_t releaseTime = millis();

    // flash the mode being entered
    while(millis()<releaseTime+1000) {
      if ((millis()/200) % 2) {
        display.clear();
      } else {
        if (mode == CANCEL) display.clear();
        if (mode == BRIGHTNESS) display.setSegments(SEG_BRIGHTNESS);
        if (mode == TIME) display.setSegments(SEG_TIME);
        if (mode == ALARM) display.setSegments(SEG_ALARM);
        if (mode == VOLUME) display.setSegments(SEG_VOLUME);
      }
    }
    
    delay(20);

    if (mode == VOLUME) {
      dfmp3.playMp3FolderTrack(1);
      
      while(digitalRead(BTN)) {
        int delta = encoder.getPosition() - lastEncoded;
        lastEncoded += delta;

        volume += delta;
        if (volume > 30) volume = 30;
        if (volume < 0) volume = 0;
        if (delta) {
          dfmp3.setVolume(volume);
        }

        display.showNumberDec(volume);
      }
      dfmp3.stop();
    }
    if (mode == BRIGHTNESS) {
      while(digitalRead(BTN)) {
        int delta = encoder.getPosition() - lastEncoded;
        lastEncoded += delta;

        brightness += delta;
        if (brightness > 7) brightness = 7;
        if (brightness < 0) brightness = 0;
        if (delta) {
         display.setBrightness(brightness);
        }

        display.showNumberDec(brightness);
      }
    }
    if (mode == ALARM) {
      int minutes = alarmTime.getMinutes();
      int hours   = alarmTime.getHour();
      
      while(digitalRead(BTN)) {
        int accelr = getAcceleration();
        int delta = encoder.getPosition() - lastEncoded;
        lastEncoded += delta;

        minutes += delta*accelr;
        while (minutes >= 60) {
          minutes -= 60;
          hours++;
        }
        while (minutes < 0) {
          minutes += 60;
          hours--;
        }
        while (hours < 0) {
          hours = 23;
          minutes = 59;
        }
        while (hours >= 24) {
          hours = 0;
          minutes = 0;
        }

        display.showNumberDecEx(hours*100+minutes, millis()/500 % 2 ? 0b01000000 : 0b00000000, true);
      }
      alarmTime.setMinute(minutes);
      alarmTime.setHour(hours);
      AlarmMatch matchTime;
      matchTime.addMatchHour();
      matchTime.addMatchMinute();
      RTC.setAlarmCallback(alarmCallback, alarmTime, matchTime);
      //RTC.setHour(hours);
    }
    if (mode == TIME) {
      int minutes = currentTime.getMinutes();
      int hours   = currentTime.getHour();
      
      while(digitalRead(BTN)) {
        int accelr = getAcceleration();
        int delta = encoder.getPosition() - lastEncoded;
        lastEncoded += delta;

        minutes += delta*accelr;
        while (minutes >= 60) {
          minutes -= 60;
          hours++;
        }
        while (minutes < 0) {
          minutes += 60;
          hours--;
        }
        while (hours < 0) {
          hours = 23;
          minutes = 59;
        }
        while (hours >= 24) {
          hours = 0;
          minutes = 0;
        }

        display.showNumberDecEx(hours*100+minutes, millis()/500 % 2 ? 0b01000000 : 0b00000000, true);
      }
      RTCTime startTime(7, Month::JUNE, 2023, hours, minutes, 00, DayOfWeek::WEDNESDAY, SaveLight::SAVING_TIME_INACTIVE);
      RTC.setTime(startTime);
      //RTC.setHour(hours);
    }
    display.clear();
    delay(250);
  }

  dfmp3.loop();

  if (alarmFlag > 0) {
    dfmp3.setVolume(volume);
    dfmp3.playMp3FolderTrack(1);
    alarmFlag = -24;
  }
}
