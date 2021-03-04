#include <Wire.h> // Wire Bibliothek einbinden
#include <LiquidCrystal_I2C.h> // Vorher hinzugefügte LiquidCrystal_I2C Bibliothek einbinden
#include <TimeLib.h>
#include <TimeAlarms.h>
LiquidCrystal_I2C lcd(0x27, 16, 2); //Hier wird festgelegt um was für einen Display es sich handelt. In diesem Fall eines mit 16 Zeichen in 2 Zeilen und der HEX-Adresse 0x27. Für ein vierzeiliges I2C-LCD verwendet man den Code "LiquidCrystal_I2C lcd(0x27, 20, 4)"

//ENUM
enum {idle, toggleAlarm, alarmRunning, alarmSetHourStart, alarmSetHour, alarmSetMinute, timeSetHourStart, timeSetHour, timeSetMinute, timeSetSecond};
enum {on, off, set, ongoing};

//Hardware pinning
const int MODUS = 2; //Pin with Interruptfunction 0
const int UP = 3; //Pin with Interruptfunction 1
const int OK = 19; //Pin with Interruptfunction 4 -> Note: 2 and 3 already used through LCD
const int SNOOZE = 18; //Pin with Interruptfunction 5
const int BUZZER = 13;

//Globale Variabeln
int lcdBacklightDelay = 10000;
volatile long buttonPressedTimeStamp;
volatile boolean buttonModusPressedState = false;
volatile boolean buttonUpPresssedState = false;
volatile boolean buttonOkPressedState = false;
volatile boolean buttonSnoozePressedState = false;

//mode
byte mode = idle;

//time
boolean timeFreeze = false;

//alarm
byte alarmState = off;
byte tempAlarmState;
int alarmTimeHour = 0;
int alarmTimeMinute = 0;
int alarmTimeoutHour;
int alarmTimeoutMinute;

//lcd
int lcdTimeHour;
int lcdTimeMinute;
int lcdTimeSecond;
boolean cursorVisible = false;


void setup()
{
  Serial.begin(9600); //DEBUG
  Serial.println("Serial begin");
  pinMode(MODUS, INPUT);
  pinMode(UP, INPUT);
  pinMode(OK, INPUT);
  pinMode(SNOOZE, INPUT);
  pinMode(BUZZER, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(MODUS), buttonModusPressed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(UP), buttonUpPresssed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(OK), buttonOkPressed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SNOOZE), buttonSnoozePressed, CHANGE);
  digitalWrite(BUZZER, LOW);
  lcd.init(); //Im Setup wird der LCD gestartet
  lcd.backlight(); //Hintergrundbeleuchtung einschalten (lcd.noBacklight(); schaltet die Beleuchtung aus).
  setTime(0, 0, 0, 1, 1, 1970); //Time Lib Systemtime setzen
  Alarm.timerRepeat(1, everySecond);
  buttonPressedTimeStamp = millis(); //Initial stamp for display timeout
  updateDisplayContent();
  Serial.println("Init finished");

}

void loop()
{
  statehandler();
  Alarm.delay(0);
}

//Time Lib based timer which occure every second
void everySecond() {
  updateDisplayContent();
  checkBacklightTimeOut();
}

void statehandler() {

  //gather interrupt flags for this turn and reset all flags
  noInterrupts(); //Disable Interrupts for this critical time frame (if interrupt would occure in the meanwile -> they will delayed starts after funtction interrupts() )
  boolean currentButtonModusPressedState = buttonModusPressedState;
  if (currentButtonModusPressedState) {
    Serial.println("currentButtonModusPressedState is true");
  }
  boolean currentButtonUpPresssedState = buttonUpPresssedState;
  if (currentButtonUpPresssedState) {
    Serial.println("currentButtonUpPresssedState is true");
  }
  boolean currentButtonOkPressedState = buttonOkPressedState;
  if (currentButtonOkPressedState) {
    Serial.println("currentButtonOkPressedState is true");
  }
  boolean currentButtonSnoozePressedState = buttonSnoozePressedState;
  if (currentButtonSnoozePressedState) {
    Serial.println("currentButtonSnoozePressedState is true");
  }
  buttonModusPressedState = false;
  buttonUpPresssedState = false;
  buttonOkPressedState = false;
  buttonSnoozePressedState = false;
  interrupts();
  int systemTimeHour = hour();
  int systemTimeMinute = minute();
  switch (mode) {
    case idle:
      cursorVisible = false;
      if (alarmState == on && systemTimeHour == alarmTimeHour && systemTimeMinute == alarmTimeMinute) { 
        alarmTimeoutHour = systemTimeHour;
        alarmTimeoutMinute = systemTimeMinute + 5;
        if (alarmTimeoutMinute > 59) {
          if (++alarmTimeoutHour == 24) {
            alarmTimeoutHour = 0;
          }
          alarmTimeoutMinute = alarmTimeoutMinute - 60;
        }
        alarmState = ongoing;
        mode = alarmRunning;
      }
      else if (currentButtonModusPressedState) {
        mode = toggleAlarm;
      }
      break;

    case toggleAlarm:
      cursorVisible = true;
      lcd.setCursor(13, 1);
      if (currentButtonModusPressedState) {
        tempAlarmState = alarmState;
        mode = alarmSetHourStart;
      }
      else if (currentButtonUpPresssedState) {
        if (alarmState == on) {
          alarmState = off;
        }
        else {
          alarmState = on;
        }
      }
      else if (currentButtonOkPressedState) {
        mode = idle;
      }
      break;

    case alarmRunning:
      cursorVisible = false;
      alarm();
      if (currentButtonOkPressedState) {
        mode = idle;
        alarmOff();
      }
      else if (currentButtonSnoozePressedState) {
        mode = idle;
        snooze(systemTimeHour, systemTimeMinute);
      } else if (systemTimeHour == alarmTimeoutHour && systemTimeMinute == alarmTimeoutMinute) {
        mode = idle;
        alarmOff();
      }
      break;

    case alarmSetHourStart:
      cursorVisible = true;
      lcd.setCursor(8, 1);
      alarmState = set;
      if (currentButtonModusPressedState) {
        alarmState = tempAlarmState;
        mode = timeSetHourStart;
      }
      else if (currentButtonUpPresssedState) {
        mode = alarmSetHour;
        alarmTimeHourUp();
      }
      else if (currentButtonOkPressedState) {
        mode = alarmSetMinute;
      }
      break;

    case alarmSetHour:
      cursorVisible = true;
      lcd.setCursor(8, 1);
      if (currentButtonUpPresssedState) {
        alarmTimeHourUp();
      }
      else if (currentButtonOkPressedState) {
        mode = alarmSetMinute;
      }
      break;

    case alarmSetMinute:
      cursorVisible = true;
      lcd.setCursor(11, 1);
      if (currentButtonUpPresssedState) {
        alarmTimeMinuteUp();
      }
      else if (currentButtonOkPressedState) {
        mode = idle;
        alarmState = on;
      }
      break;

    case timeSetHourStart:
      cursorVisible = true;
      lcd.setCursor(8, 0);
      if (!timeFreeze) {
        lcdTimeHour = hour();
        lcdTimeMinute = minute();
        lcdTimeSecond = second();
        timeFreeze = true;
      }
      if (currentButtonModusPressedState) {
        timeFreeze = false;
        mode = idle;
      }
      else if (currentButtonUpPresssedState) {
        mode = timeSetHour;
        systemTimeHourUp();
      }
      else if (currentButtonOkPressedState) {
        mode = timeSetMinute;
      }
      break;

    case timeSetHour:
      cursorVisible = true;
      lcd.setCursor(8, 0);
      if (currentButtonUpPresssedState) {
        systemTimeHourUp();
      }
      else if (currentButtonOkPressedState) {
        mode = timeSetMinute;
      }
      break;

    case timeSetMinute:
      cursorVisible = true;
      lcd.setCursor(11, 0);
      if (currentButtonUpPresssedState) {
        systemTimeMinuteUp();
      }
      else if (currentButtonOkPressedState) {
        mode = timeSetSecond;
      }
      break;

    case timeSetSecond:
      cursorVisible = true;
      lcd.setCursor(14, 0);
      if (currentButtonUpPresssedState) {
        systemTimeSecondUp();
      }
      else if (currentButtonOkPressedState) {
        setNewSystemTime();
        mode = idle;
        timeFreeze = false;
      }
      break;
  }
  //update Display on userinput
  if (currentButtonModusPressedState || currentButtonUpPresssedState || currentButtonOkPressedState || currentButtonSnoozePressedState) {
    lcd.backlight();
    updateDisplayContent();
  }
  Serial.println(mode);


}

void updateDisplayContent() {
  //local display variable
  char lcdActualTime[16];
  char lcdActualAlarmTime[16];
  char lcdAlarmState[] = "   ";
  //update time if not in time set mode
  if (!timeFreeze) {
    lcdTimeHour = hour();
    lcdTimeMinute = minute();
    lcdTimeSecond = second();
  }
  //write set correct string for current alarm mode
  switch (alarmState) {
    case on:
      strncpy( lcdAlarmState, "ON ", sizeof(lcdAlarmState) );
      break;
    case off:
      strncpy( lcdAlarmState, "OFF", sizeof(lcdAlarmState) );
      break;
    case set:
      strncpy( lcdAlarmState, "SET", sizeof(lcdAlarmState) );
      break;
    case ongoing:
      strncpy( lcdAlarmState, "RUN", sizeof(lcdAlarmState) );
      break;
  }
  //write to lcd
  lcd.noCursor();
  lcd.setCursor(0, 0);
  sprintf(lcdActualTime, "Time:  %02d:%02d:%02d ", lcdTimeHour, lcdTimeMinute, lcdTimeSecond);
  lcd.print(lcdActualTime);
  lcd.setCursor(0, 1);
  sprintf(lcdActualAlarmTime, "Alarm: %02d:%02d %s", alarmTimeHour, alarmTimeMinute, lcdAlarmState);
  lcd.print(lcdActualAlarmTime);
  if (cursorVisible) {
    lcd.cursor();
  }
}

void alarm() {
  lcd.backlight();
  digitalWrite(BUZZER, HIGH);
}

void snooze(int systemTimeHour, int systemTimeMinute) {
  digitalWrite(BUZZER, LOW);
  alarmTimeHour = systemTimeHour;
  alarmTimeMinute = systemTimeMinute + 5;
  if (alarmTimeMinute > 59) {
    alarmTimeMinute = alarmTimeMinute - 60;
    if (++alarmTimeHour == 24) {
      alarmTimeHour = 0;
    }
  }
  alarmState = on;
}

void alarmOff() {
  digitalWrite(BUZZER, LOW);
  alarmState = off;
}

void alarmTimeHourUp() {
  if (++alarmTimeHour == 24) {
    alarmTimeHour = 0;
  }
}

void alarmTimeMinuteUp() {
  if (++alarmTimeMinute == 60) {
    alarmTimeMinute = 0;
  }
}

void systemTimeHourUp() {
  if (++lcdTimeHour == 24) {
    lcdTimeHour = 0;
  }
}

void systemTimeMinuteUp() {
  if (++lcdTimeMinute == 60) {
    lcdTimeMinute = 0;
  }
}

void systemTimeSecondUp() {
  if (++lcdTimeSecond == 60) {
    lcdTimeSecond = 0;
  }
}

void setNewSystemTime() {
  setTime(lcdTimeHour, lcdTimeMinute, lcdTimeSecond, 1, 1, 1970); //Time Lib Systemtime setzen
}

void checkBacklightTimeOut() {
  if (alarmState != ongoing) {
    if (millis() - buttonPressedTimeStamp >= lcdBacklightDelay) {
      lcd.noBacklight();
    }
  }
}

//Interrupt Routine
void buttonModusPressed() {
  volatile static unsigned long lastInterruptTimeOnModus = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - lastInterruptTimeOnModus > 50)  // ignores interupts for 50milliseconds
  {
    if (digitalRead(MODUS) == HIGH) {
      buttonPressedTimeStamp = millis();
      buttonModusPressedState = true;
    }
  }
  lastInterruptTimeOnModus = interrupt_time;
}

void buttonUpPresssed() {
  volatile static unsigned long lastInterruptTimeOnUp = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - lastInterruptTimeOnUp > 50)  // ignores interupts for 50milliseconds
  {
    if (digitalRead(UP) == HIGH) {
      buttonPressedTimeStamp = millis();
      buttonUpPresssedState = true;
    }
  }
  lastInterruptTimeOnUp = interrupt_time;
}

void buttonOkPressed() {
  volatile static unsigned long lastInterruptTimeOnOk = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - lastInterruptTimeOnOk > 50)  // ignores interupts for 50milliseconds
  {
    if (digitalRead(OK) == HIGH) {
      buttonPressedTimeStamp = millis();
      buttonOkPressedState = true;
    }
  }
  lastInterruptTimeOnOk = interrupt_time;
}

void buttonSnoozePressed() {
  volatile static unsigned long lastInterruptTimeOnSnooze = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - lastInterruptTimeOnSnooze > 50)  // ignores interupts for 50milliseconds
  {
    if (digitalRead(SNOOZE) == HIGH) {
      buttonPressedTimeStamp = millis();
      buttonSnoozePressedState = true;
    }
  }
  lastInterruptTimeOnSnooze = interrupt_time;
}
