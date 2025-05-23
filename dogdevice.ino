#include <SPI.h>
#include <Wire.h>
#include <RH_NRF24.h>
#include <EEPROM.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include "fonts/font8x8_90clock.h"
#include <RHReliableDatagram.h>
// LCD //////////
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define INCLUDE_SCROLLING 0
//#define USE_SERIAL
SSD1306AsciiWire display;
//////////////////
// LED registers //////////
const int ledDataPin = 6;   // DS (Data)
const int ledLatchPin = 7;  // STCP (Latch)
const int ledClockPin = 5;  // SHCP (Clock)
///////////////////////////
// Buttons registers //////////
const uint8_t ISRDataPin = 2;   // connected to 74HC165 QH (9) pin
const uint8_t ISRLatchPin = 3;  // connected to 74HC165 SH/LD (1) pin
const uint8_t ISRClockPin = 4;  // connected to 74HC165 CLK (2) pin
///////////////////////////
const uint8_t TimersCnt = 8;
const uint8_t DisplayTimersStep = SCREEN_WIDTH / TimersCnt;
unsigned long previousMillis = 0;
unsigned long sendSyncTime = 0;
bool isSynchronizing = false;
unsigned long syncRetries = 0;
unsigned long lastSyncTime = 0;

struct Button {
  unsigned long holdMillis = 0;
  bool isPressedHandler = false;
  bool isPressedOnce = false;
  bool isLongPressed = false;
  bool isDoubleLongPressed = false;
};

struct Timer {
  uint8_t hours = 0;
  uint8_t minutes = 0;
  uint8_t seconds = 0;
  bool isEnabled = true;
};

Timer timers[TimersCnt];
Button buttons[TimersCnt];
///////////////////////////
// Radio //////////
RH_NRF24 nrf24(10, 9);
int pingSent = 0;
int pingRec = 0;
// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(nrf24);
///////////////////////////
uint8_t minHoursYellowLed = 2;
uint8_t minHoursRedLed = 4;
uint8_t powerMode = 4;
uint8_t disableSync = 0;
byte clientId = 0;
int initialSyncing = 5;
///////////////////////////
const int E_ID = 0;
const int E_YEL = 1;
const int E_RED = 2;
const int TRANS_POWER = 3;
const int DISABLE_SYNC = 4;
// Menu //////////
bool menuOpened = false;
byte menuPos = 0;
unsigned long lastSendPing = 0;
const uint8_t FONT_SIZE = 8;
uint8_t recBuf[26];

void setup() {

#ifdef USE_SERIAL
  Serial.begin(38400);  // initialize serial bus
  while (!Serial)
    ;
  Serial.println(F("=============="));
  Serial.println(F("Init Start"));
#endif

  clientId = EEPROM.read(E_ID);
  minHoursYellowLed = EEPROM.read(E_YEL);
  minHoursRedLed = EEPROM.read(E_RED);
  powerMode = EEPROM.read(TRANS_POWER);
  disableSync = EEPROM.read(DISABLE_SYNC);
  if (disableSync > 1)
    disableSync = 0;
  manager.setThisAddress(clientId);
#ifdef USE_SERIAL
  Serial.print(F("ClientId: "));
  Serial.println(clientId);
#endif

  if (minHoursYellowLed < 1 || minHoursYellowLed > 10)
    minHoursYellowLed = 2;
  if (minHoursRedLed < 1 || minHoursRedLed > 10)
    minHoursRedLed = 4;


  // 74HC165 shift register
  pinMode(ISRDataPin, INPUT);
  pinMode(ISRLatchPin, OUTPUT);
  pinMode(ISRClockPin, OUTPUT);
  digitalWrite(ISRLatchPin, HIGH);
  digitalWrite(ISRClockPin, HIGH);

  pinMode(ledDataPin, OUTPUT);
  pinMode(ledLatchPin, OUTPUT);
  pinMode(ledClockPin, OUTPUT);

  ////////// Radio /////////////////
  Serial.println(F("Start init radio"));
  if (!manager.init()) {
    Serial.println(F("nrf24 Radio init failed!"));
  } else {
    Serial.println(F("nrf24 Radio initialized!"));
  }
  Serial.println(F("Init radio Success!"));


  Serial.println(F("Start init display"));
  display.begin(&Adafruit128x64, SCREEN_ADDRESS);
  Serial.println(F("Start init display font"));
  display.setFont(font8x8_90c);
  Serial.println(F("Init display Success!"));

  if (powerMode >= 4) {
    powerMode = 0;
    EEPROM.write(TRANS_POWER, powerMode);
  }

  if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, powerMode)) {
    Serial.println(F("nrf24 setRF: Failed"));
    return;
  } else {
    Serial.println(F("nrf24 setRF: Success!"));
  }
}

void loop() {
  UpdateTimers();
  ReadButtons();

  if (menuOpened) {
    DrawMenu();
    if (disableSync == 0) {
      ProcessDebugPing();
    }
  } else {
    ApplyButtonsToTimers();
    DrawTimers();
    if (disableSync != 1) {
      CheckSynchronized();
    }
  }

  if (disableSync != 1) {
    ProcessRadioCommon();
    InitialSync();
  }

  // display.setCursor(1 * FONT_SIZE, 0);
  // if(millis() % 2000 < 1000)
  // display.print(111111, DEC);
  //   else
  // display.print(F("      "));
}

void InitialSync() {
  if (initialSyncing <= 0)
    return;

  if (millis() - lastSyncTime < 500) {
    return;
  }
  lastSyncTime = millis();
  initialSyncing--;

  uint8_t data[] = "syncReq";
  auto to = 1 - clientId;
  if (manager.sendtoWait(data, sizeof(data), to)) {
#ifdef USE_SERIAL
    Serial.print("send synced to ");
    Serial.println(to);
#endif
  } else {
#ifdef USE_SERIAL
    Serial.print("sent synced failed to ");
    Serial.println(to);
#endif
  }
}

//1 - up
//2 - down
//3 - >
//4 - <
void OpenMenu() {
  display.clear();
  menuOpened = !menuOpened;
  pingRec = 0;
  pingSent = 0;
  ClearButtons();
}

void CloseMenu() {
  menuOpened = false;
  display.clear();
  ClearButtons();
}

void ClearButtons() {
  for (int i = 0; i < TimersCnt; i++) {
    auto iterButton = &buttons[i];
    iterButton->isPressedHandler = false;
    iterButton->isDoubleLongPressed = false;
    iterButton->isLongPressed = false;
    iterButton->isPressedOnce = false;
  }
}

void DrawMenu() {
  const uint8_t MenuDrawStep = 1;

  int16_t pos = 15;
  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(F("Menu"));
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  if (menuPos == 1)
    display.print(F(">"));
  else
    display.print(F(" "));

  display.print(F("Yel:"));
  display.print(minHoursYellowLed, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  if (menuPos == 2)
    display.print(F(">"));
  else
    display.print(F(" "));
  display.print(F("Red:"));
  display.print(minHoursRedLed, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  if (menuPos == 3)
    display.print(F(">"));
  else
    display.print(F(" "));
  display.print(F("Id :"));
  display.print(clientId, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  if (menuPos == 4)
    display.print(F(">"));
  else
    display.print(F(" "));
  display.print(F("Pow:"));
  display.print(powerMode, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  if (menuPos == 5)
    display.print(F(">"));
  else
    display.print(F(" "));
  display.print(F("NoSn:"));
  display.print(disableSync, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(F("S:"));
  display.print(pingSent, DEC);

  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(F("R:"));
  display.print(pingRec, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(F("Dif:"));
  display.print(pingSent - pingRec, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(F("Ret:"));
  display.print(syncRetries, DEC);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');


  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  display.setCursor(pos-- * FONT_SIZE, 0);
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');
  display.print(' ');

  if (buttons[0].isLongPressed || buttons[0].isDoubleLongPressed) {
    CloseMenu();
  }

  if ((buttons[1].isPressedOnce || buttons[1].isLongPressed || buttons[1].isDoubleLongPressed) && menuPos > 0) {
    buttons[1].isPressedOnce = false;
    buttons[1].isLongPressed = false;
    buttons[1].isDoubleLongPressed = false;
    menuPos--;
  }

  if ((buttons[2].isPressedOnce || buttons[2].isLongPressed || buttons[2].isDoubleLongPressed) && menuPos < 5) {
    buttons[2].isPressedOnce = false;
    buttons[2].isPressedOnce = false;
    buttons[2].isLongPressed = false;
    buttons[2].isDoubleLongPressed = false;
    menuPos++;
  }

  if (buttons[3].isPressedOnce || buttons[3].isLongPressed || buttons[3].isDoubleLongPressed) {
    buttons[3].isPressedOnce = false;
    buttons[3].isPressedOnce = false;
    buttons[3].isLongPressed = false;
    buttons[3].isDoubleLongPressed = false;
    if (menuPos == 1) {
      if (minHoursYellowLed < 9 && minHoursYellowLed < minHoursRedLed - 1) {
        minHoursYellowLed++;
        EEPROM.write(E_YEL, minHoursYellowLed);
      }
    } else if (menuPos == 2) {
      if (minHoursRedLed < 9) {
        minHoursRedLed++;
        EEPROM.write(E_RED, minHoursRedLed);
      }

    } else if (menuPos == 3) {
      clientId++;
      if (clientId > 10)
        clientId = 0;
      EEPROM.write(E_ID, clientId);
      manager.setThisAddress(clientId);
    } else if (menuPos == 4) {
      powerMode++;
      if (powerMode >= 4)
        powerMode = 0;
      EEPROM.write(TRANS_POWER, powerMode);
      if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, powerMode)) {
        Serial.println(F("nrf24 setRF: Failed"));
        return;
      } else {
        Serial.println(F("nrf24 setRF: Success!"));
      }
    } else if (menuPos == 5) {
      if (disableSync == 0)
        disableSync = 1;
      else
        disableSync = 0;
      EEPROM.write(DISABLE_SYNC, disableSync);
    }
  }

  if (buttons[4].isPressedOnce || buttons[4].isLongPressed || buttons[4].isDoubleLongPressed) {
    buttons[4].isPressedOnce = false;
    buttons[4].isPressedOnce = false;
    buttons[4].isLongPressed = false;
    buttons[4].isDoubleLongPressed = false;

    if (menuPos == 1 && minHoursYellowLed > 2) {
      minHoursYellowLed--;
      EEPROM.write(1, minHoursYellowLed);
    }

    if (menuPos == 2 && minHoursRedLed > 2 && minHoursRedLed > minHoursYellowLed + 1) {
      minHoursRedLed--;
      EEPROM.write(2, minHoursRedLed);
    } else if (menuPos == 3) {
      clientId--;
      if (clientId > 10)
        clientId = 10;
      EEPROM.write(E_ID, clientId);
      manager.setThisAddress(clientId);
    } else if (menuPos == 4) {
      powerMode--;
      if (powerMode < 0 || powerMode > 10)
        powerMode = 0;
      EEPROM.write(TRANS_POWER, powerMode);
      if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, powerMode)) {
        Serial.println(F("nrf24 setRF: Failed"));
        return;
      } else {
        Serial.println(F("nrf24 setRF: Success!"));
      }
    } else if (menuPos == 5) {
      if (disableSync == 0)
        disableSync = 1;
      else
        disableSync = 0;
      EEPROM.write(DISABLE_SYNC, disableSync);
    }
  }
}

void ProcessDebugPing() {
  auto curMilis = millis();
  if (curMilis - lastSendPing > 500) {
    lastSendPing = curMilis;

    uint8_t data[] = "ping";
    uint8_t to = 1 - clientId;

    if (manager.sendtoWait(data, sizeof(data), to)) {
#ifdef USE_SERIAL
      Serial.print("send ping to ");
      Serial.println(to);
#endif
      pingSent++;
    } else {
#ifdef USE_SERIAL
      Serial.print("sent ping failed to ");
      Serial.println(to);
#endif
      pingSent--;
    }
  }
}

void ProcessRadioCommon() {
  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(100, DEC);
  if (manager.available()) {
    uint8_t len = sizeof(recBuf);
    uint8_t from;
    if (manager.recvfromAck(recBuf, &len, &from)) {
      char* msg = (char*)recBuf;
#ifdef USE_SERIAL
      Serial.print("Device ");
      Serial.print(clientId);
      Serial.print(" got request from ");
      Serial.print(from);
      Serial.print(". Request: ");
      Serial.println(msg);
#endif

      if (len >= 26 && msg[0] == 'S' && msg[1] == 'N') {
#ifdef USE_SERIAL
        Serial.println("Got SN.");
#endif
        for (int i = 0; i < 8; i++) {
          auto hr = recBuf[2 + (i * 3)];

          if (hr == 255) {
            timers[i].isEnabled = false;
            timers[i].hours = 0;
            timers[i].minutes = 0;
            timers[i].seconds = 0;
          } else {
            auto min = recBuf[3 + (i * 3)];
            auto sec = recBuf[4 + (i * 3)];
            timers[i].isEnabled = true;
            timers[i].hours = hr;
            timers[i].minutes = min;
            timers[i].seconds = sec;
          }
        }
        if (initialSyncing <= 0) {
          uint8_t data[] = "synced";
          if (manager.sendtoWait(data, sizeof(data), from)) {
#ifdef USE_SERIAL
            Serial.print("send synced to ");
            Serial.println(from);
#endif
          } else {
#ifdef USE_SERIAL
            Serial.print("sent synced failed to ");
            Serial.println(from);
#endif
          }
        }
        initialSyncing = 0;
      } else if (strcmp(msg, "ping") == 0) {
        uint8_t data[] = "ping_resp";
        if (manager.sendtoWait(data, sizeof(data), from)) {
          Serial.println("sent ping_resp");
        } else {
          Serial.println("sent ping_resp failed");
        }
      } else if (strcmp(msg, "ping_resp") == 0) {
        pingRec++;
#ifdef USE_SERIAL
        Serial.println("Got ping_resp.");
#endif
      } else if (strcmp(msg, "synced") == 0) {
        isSynchronizing = false;
#ifdef USE_SERIAL
        Serial.println("Sync finished.");
#endif
      } else if (strcmp(msg, "syncReq") == 0) {
#ifdef USE_SERIAL
        Serial.println("Sync request received.");
#endif
        ClientSendTimers();
      } else {
#ifdef USE_SERIAL
        Serial.print("Got UNKNOWN request from ");
        Serial.println(from);
#endif
      }
    } else {
#ifdef USE_SERIAL
      Serial.print("Failed receive request from ");
      Serial.println(from);
#endif
      pingRec--;
    }
  }

  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(120, DEC);
}

void UpdateLeds() {
  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(700, DEC);
  // Pull the latchPin low to start sending data
  digitalWrite(ledLatchPin, LOW);
  delayMicroseconds(1);

  auto doubleTimers = TimersCnt * 2;
  int currentTimerI = 0;
  for (int i = 0; i < doubleTimers; i++) {
    //display.setCursor(0 * FONT_SIZE, 0);
    //display.print(710 + i, DEC);
    digitalWrite(ledClockPin, LOW);  // Prepare to shift data
    delayMicroseconds(1);

    auto currentTimer = timers[TimersCnt - currentTimerI - 1];
    int hoursTimer = currentTimer.hours;
    bool showLed = false;
    if (i % 2 == 0) {
      if (hoursTimer > minHoursRedLed) {
        showLed = true;
      }
    } else {

      if (hoursTimer > minHoursYellowLed && hoursTimer <= minHoursRedLed) {
        showLed = true;
      }
      currentTimerI = (currentTimerI + 1) % TimersCnt;
    }

    if (showLed && currentTimer.isEnabled)
      digitalWrite(ledDataPin, HIGH);
    else
      digitalWrite(ledDataPin, LOW);

    delayMicroseconds(1);
    // Shift the data
    digitalWrite(ledClockPin, HIGH);
    delayMicroseconds(1);
  }

  // Pull the latchPin high to update the outputs
  digitalWrite(ledLatchPin, HIGH);
  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(799, DEC);
}

void ReadButtons() {
  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(300, DEC);
  // Pulse the latch pin to load the parallel inputs
  digitalWrite(ISRLatchPin, LOW);
  delayMicroseconds(5);
  digitalWrite(ISRLatchPin, HIGH);
  delayMicroseconds(5);

  // Read 16 buttons (two 74HC165 registers)
  for (int i = 0; i < TimersCnt; i++) {
    //display.setCursor(0 * FONT_SIZE, 0);
    //display.print(301 + i, DEC);
    digitalWrite(ISRClockPin, LOW);
    delayMicroseconds(5);

    auto iterButton = &buttons[TimersCnt - i - 1];
    auto isButtonPressed = digitalRead(ISRDataPin);

    //display.setCursor(0 * FONT_SIZE, 0);
    //display.print(311 + i, DEC);

    if (isButtonPressed) {
      iterButton->isPressedHandler = true;
      iterButton->isDoubleLongPressed = false;
      iterButton->isLongPressed = false;
      iterButton->isPressedOnce = false;
    } else {
      if (iterButton->isPressedHandler) {
        iterButton->isPressedHandler = false;
        auto holdTime = millis() - iterButton->holdMillis;
        if (holdTime > 3000) {
          iterButton->isDoubleLongPressed = true;
        } else if (holdTime > 2000) {
          iterButton->isLongPressed = true;
        } else {
          iterButton->isPressedOnce = true;
        }
      }
      iterButton->holdMillis = millis();
    }

    //display.setCursor(0 * FONT_SIZE, 0);
    //display.print(321 + i, DEC);

    digitalWrite(ISRClockPin, HIGH);
    delayMicroseconds(5);

    //display.setCursor(0 * FONT_SIZE, 0);
    //display.print(331 + i, DEC);
  }

  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(399, DEC);
}

void ApplyButtonsToTimers() {
  for (int i = 0; i < TimersCnt; i++) {
    auto iterTimer = &timers[i];
    auto iterButton = &buttons[i];
    if (iterButton->isPressedOnce) {
      iterButton->isPressedOnce = false;
      iterTimer->seconds = 0;
      iterTimer->minutes = 0;
      iterTimer->hours = 0;
      StartSyncTime();
    }
    if (iterButton->isLongPressed) {
      iterTimer->isEnabled = !iterTimer->isEnabled;
      iterButton->isLongPressed = false;
      if (initialSyncing <= 0) {
        StartSyncTime();
      }
    }
  }

  auto iterButton = &buttons[0];
  if (iterButton->isDoubleLongPressed) {
    iterButton->isDoubleLongPressed = false;
    iterButton = &buttons[7];
    if (iterButton->isPressedHandler) {
      OpenMenu();
    }
  }
}

void StartSyncTime() {
  if (disableSync)
    return;
  isSynchronizing = true;
  ClientSendTimers();
}

void CheckSynchronized() {
  if (!isSynchronizing)
    return;
  if (millis() - sendSyncTime > 2000) {
    syncRetries++;

    if (syncRetries > 10) {
      syncRetries = 0;
      isSynchronizing = false;
    }
#ifdef USE_SERIAL
    Serial.print("Try sync with other device. Retry: ");
    Serial.println(syncRetries);
#endif
    ClientSendTimers();
  }
}

void ClientSendTimers() {
  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(400, DEC);
  sendSyncTime = millis();
  uint8_t data[26];  // 2 bytes for "SN", 16 bytes for 8 timers (hr, min)

  // Add "SN" header
  data[0] = 'S';
  data[1] = 'N';

  for (int i = 0; i < 8; i++) {
    if (timers[i].isEnabled) {
      data[2 + (i * 3)] = timers[i].hours;    // Hour
      data[3 + (i * 3)] = timers[i].minutes;  // Minute
      data[4 + (i * 3)] = timers[i].seconds;  // sec
    } else {
      data[2 + (i * 3)] = 255;
      data[3 + (i * 3)] = 255;
      data[4 + (i * 3)] = 255;
    }
  }

  uint8_t to = 1 - clientId;
  if (manager.sendtoWait(data, sizeof(data), to)) {
#ifdef USE_SERIAL
    Serial.print("sent SN to ");
    Serial.println(to);
#endif
  } else {
#ifdef USE_SERIAL
    Serial.print("sent SN failed to ");
    Serial.println(to);
#endif
  }

  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(420, DEC);
}

void UpdateTimers() {
  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(500, DEC);
  unsigned long currentMillis = millis();
  if (previousMillis > currentMillis)
    previousMillis = 0;

  bool updateLeds = false;
  if (currentMillis - previousMillis >= 1000) {
    previousMillis += 1000;
    for (int i = 0; i < TimersCnt; i++) {
      if (timers[i].hours == 9 && timers[i].minutes == 59 && timers[i].seconds == 59) {
        continue;
      }
      updateLeds = true;
      timers[i].seconds++;
      if (timers[i].seconds >= 60) {
        timers[i].seconds = 0;
        timers[i].minutes++;

        if (timers[i].minutes >= 60) {
          timers[i].minutes = 0;
          timers[i].hours++;
        }
      }
    }
    if (updateLeds)
      UpdateLeds();
  }

  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(520, DEC);
}

void DrawTimers() {
  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(600, DEC);
  int16_t pos = 10;

  for (int i = 0; i < TimersCnt; i++) {
    auto iterTimer = &timers[i];
    display.setCursor(pos-- * (FONT_SIZE + 4), 0);
    display.print(i + 1, DEC);
    display.print(F(":"));

    if (i < 9) {
      display.print(F(" "));
    }

    if (!iterTimer->isEnabled) {
      display.print(F("Off   "));
    } else {
      display.print(iterTimer->hours, DEC);
      display.print(F(":"));
      if (iterTimer->minutes < 10)
        display.print(0, DEC);

      display.print(iterTimer->minutes, DEC);

      display.print(F("       "));
      //if (showSeconds) {
      // display.print(':');
      // if (iterTimer->seconds < 10)
      //   display.print(' ');

      // display.print(iterTimer->seconds, DEC);
    }
    //}
  }

  if (disableSync == 0) {
    if (initialSyncing > 0) {
      display.setCursor(0 * FONT_SIZE, 0);
      display.print(F("Sync 0"));
    } else if (isSynchronizing) {
      display.setCursor(0 * FONT_SIZE, 0);
      display.print(F("Sync..."));
    } else {
      display.setCursor(0 * FONT_SIZE, 0);
      display.print(F("         "));
    }
  }


  //display.setCursor(0 * FONT_SIZE, 0);
  //display.print(620, DEC);
}
