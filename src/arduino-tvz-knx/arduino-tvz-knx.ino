/*----------------------------------------------------------------------------*
 | 
 | Arduino controlling a Tecalor TVZ via KNX
 | https://github.com/hotzen/arduino-tecalor-tvz-knx
 |  
 *-- Software ----------------------------------------------------------------*
 | Arduino TP-UART KNX Library
 |   https://bitbucket.org/thorstengehrig/arduino-tpuart-knx-user-forum
 |  
 *-- Hardware ----------------------------------------------------------------*
 | Arduino UNO:
 |   https://www.arduino.cc/en/Main/ArduinoBoardUno
 | 
 | Tecalor TVZ
 |   http://www.tecalor.de/produkte/lueftungssysteme/TVZ-70-170-270-370-plus
 | 
 | Siemens BCU 5WG1117-2AB12 (KNX-coupling)
 |   https://mall.industry.siemens.com/mall/de/WW/Catalog/Product/5WG1117-2AB12
 *---------------------------------------------------------------------------*/
#include <KnxTpUart.h>

// KNX ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define KNX_PA_SELF "15.15.20" // physical address of this arduino

#define KNX_GA_LEVEL  "5/0/1" // setting/getting a level (1byte int)
#define KNX_GA_BYPASS "5/0/2" // address to check whether the bypass is active (boolean)

#define KNX_CYCLIC_SEND_SECS 10 // send level & bypass to the GAs every X seconds (0 to disable)


// DEBUGGING ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//#define DEBUG

#ifdef DEBUG
 #define DEBUG_PRINT(x) // Serial1.println(x)
                        // The UNO only has one serial, you can't debug AND tpuart on the same serial...
                        // With a MEGA you get additional Serial1 etc. to use for debugging
#else
 #define DEBUG_PRINT(x)
#endif


// APP ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define STATE_INITIAL      0
#define STATE_INIT_TVZ     1
#define STATE_CYCLIC_SEND  2
#define STATE_CHANGE_LEVEL 3

#define LEVEL_MIN 1
#define LEVEL_MAX 3


byte state; // current application-state

byte level;    // current TVZ level
byte levelNew; // request to change TVZ level
boolean bypass; // if bypass is currently active (summertime)

const unsigned long cyclicSendInterval = KNX_CYCLIC_SEND_SECS * 1000;
unsigned long cyclicSendTimer;


// LED ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define LED_PIN LED_BUILTIN

#define LED_INIT_INTERVAL 100 // time between init-blinks
#define LED_INIT_DURATION 100 // duration of an init-blink

#define LED_LEVEL_INTERVAL 1500 // time between level-blinks
#define LED_LEVEL_DURATION 250  // duration of a single level-blink
#define LED_LEVEL_PAUSE    250  // time between single level-blinks

#define LED_STATE_OFF                  0 // led is just off
#define LED_STATE_INIT_BLINKING        1 // led is on and doing init-blinks
#define LED_STATE_LEVEL_BLINKING       3 // led is on and doing level-blinks
#define LED_STATE_BETWEEN_LEVEL_BLINKS 4 // led is off, before another level-blink

byte ledState;          // current state of LED
byte ledLevelBlinks;    // level-blinks to show
byte ledLevelBlinked;   // level-blinks shown in current cycle
unsigned long ledTimer; // http://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html


KnxTpUart knx(&Serial, KNX_PA_SELF); // Init KNX TP-UART on Serial

void setup() {
  // init LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledState = LED_STATE_OFF;
  ledLevelBlinks = ledLevelBlinked = 0;
  
  // Serial: Pin 0 (RX) and 1 (TX).
  Serial.begin(19200); // connect to serial
  knx.uartReset();

  // listen to KNX GAs
  knx.addListenGroupAddress(KNX_GA_LEVEL);
  knx.addListenGroupAddress(KNX_GA_BYPASS);

  // initialize state
  state = STATE_INITIAL;
  level = 0;
  bypass = false;

  // init timers
  cyclicSendTimer = millis();
  ledTimer = millis();
}

void loop() {
  led(level);

  switch (state) {
    case STATE_INITIAL:
      state = STATE_INIT_TVZ;
      break;

    case STATE_INIT_TVZ:
      //TODO: reading TVZ level & bypass-state / is that even possible?
      level = 2; // dummy
      bypass = false; // dummy
      
      state = STATE_CYCLIC_SEND;
      break;

    case STATE_CYCLIC_SEND:
      if (cyclicSendTimerElapsed()) {
        cyclicSend();
      }
      break;

    case STATE_CHANGE_LEVEL:
      changeLevelTo( levelNew );
      state = STATE_CYCLIC_SEND;
      break;
  }
}

void cyclicSend() {
  boolean sentLevel = knx.groupWrite1ByteInt(KNX_GA_LEVEL, level);
  DEBUG_PRINT("cyclicSend() sent level: " + sentLevel);
  
  boolean sentBypass = knx.groupWriteBool(KNX_GA_BYPASS, bypass);
  DEBUG_PRINT("cyclicSend() sent bypass: " + sentBypass);
}

void changeLevelTo(int newLevel) {
  //TODO: trigger optokopplers

  level = newLevel;
}

void onReadToPhysicalAddress(KnxTelegram* telegram, String targetAddress) {
  DEBUG_PRINT("onReadToPhysicalAddress() unsupported");
}

void onReadToGroupAddress(KnxTelegram* telegram, String targetAddress) {
  
  if (targetAddress == KNX_GA_LEVEL) {
    knx.groupAnswer1ByteInt(targetAddress, level);
    
  } else if (targetAddress = KNX_GA_BYPASS) {
    knx.groupAnswerBool(targetAddress, bypass);
    
  } else {
    DEBUG_PRINT("onReadToGroupAddress() unknown GA: " + targetAddress);
  }
}

void onWriteToPhysicalAddress(KnxTelegram* telegram, String targetAddress) {
  DEBUG_PRINT("onWriteToPhysicalAddress() unsupported");
}

void onWriteToGroupAddress(KnxTelegram* telegram, String targetAddress) {
  
  if (targetAddress == KNX_GA_LEVEL) {
    onNewLevel( telegram->get1ByteIntValue() );
    
  } else {
    DEBUG_PRINT("onWriteToGroupAddress() unknown GA: " + targetAddress);
  }
}

void onNewLevel(byte newLevel) {
  if (level != newLevel) {
    if (newLevel >= LEVEL_MIN && newLevel <= LEVEL_MAX) {
      DEBUG_PRINT("onNewLevel() changing to level " + newLevel);
      levelNew = newLevel;
      state = STATE_CHANGE_LEVEL;  
    } else {
      DEBUG_PRINT("onNewLevel() invalid new level " + newLevel);
    }
  } else {
    DEBUG_PRINT("onNewLevel() already on level " + newLevel);
  }
}

void onTelegram(KnxTelegram* telegram) {
  switch (telegram->getCommand()) {
    case KNX_COMMAND_READ:
      if (telegram->isTargetGroup()) {
        String targetAddress = getTargetGroupAddress(telegram);
        DEBUG_PRINT("KNX_TELEGRAM/READ@GA(" + targetAddress + ")");
        onReadToGroupAddress(telegram, targetAddress);
      } else {
        String targetAddress = getTargetPhysicalAddress(telegram);
        DEBUG_PRINT("KNX_TELEGRAM/READ@PA(" + targetAddress + ")");
        onReadToPhysicalAddress(telegram, targetAddress);
      }
      break;

    case KNX_COMMAND_WRITE:
      if (telegram->isTargetGroup()) {
        String targetAddress = getTargetGroupAddress(telegram);
        DEBUG_PRINT("KNX_TELEGRAM/WRITE@GA(" + targetAddress + ")");
        onWriteToGroupAddress(telegram, targetAddress);
      } else {
        String targetAddress = getTargetPhysicalAddress(telegram);
        DEBUG_PRINT("KNX_TELEGRAM/WRITE@PA(" + targetAddress + ")");
        onWriteToPhysicalAddress(telegram, targetAddress);
      }
      break;

    case KNX_COMMAND_ANSWER:
      DEBUG_PRINT("KNX_TELEGRAM/ANSWER");
      break;

    case KNX_COMMAND_INDIVIDUAL_ADDR_WRITE: // programming-mode
      DEBUG_PRINT("KNX_TELEGRAM/INDIVIDUAL_ADDR_WRITE");
      break;

    case KNX_COMMAND_INDIVIDUAL_ADDR_REQUEST:
      DEBUG_PRINT("KNX_TELEGRAM/INDIVIDUAL_ADDR_REQUEST");
      break;

    case KNX_COMMAND_INDIVIDUAL_ADDR_RESPONSE:
      DEBUG_PRINT("KNX_TELEGRAM/INDIVIDUAL_ADDR_RESPONSE");
      break;

    case KNX_COMMAND_MASK_VERSION_READ:
      DEBUG_PRINT("KNX_TELEGRAM/MASK_VERSION_READ");
      break;

    case KNX_COMMAND_MASK_VERSION_RESPONSE:
      DEBUG_PRINT("KNX_TELEGRAM/MASK_VERSION_RESPONSE");
      break;

    case KNX_COMMAND_RESTART:
      DEBUG_PRINT("KNX_TELEGRAM/RESTART");
      break;

    case KNX_COMMAND_ESCAPE:
      DEBUG_PRINT("KNX_TELEGRAM/ESCAPE");
      break;
  }
}

void serialEvent() {
  KnxTpUartSerialEventType evtType = knx.serialEvent();
  switch (evtType) {
    case TPUART_RESET_INDICATION:
      DEBUG_PRINT("TPUART_RESET_INDICATION");
      break;

    case KNX_TELEGRAM:
      onTelegram( knx.getReceivedTelegram() );
      break;

    case IRRELEVANT_KNX_TELEGRAM:
      DEBUG_PRINT("IRRELEVANT_KNX_TELEGRAM");
      break;

    case UNKNOWN:
      DEBUG_PRINT("UNKNOWN");
      break;
  }
}

String getTargetPhysicalAddress(KnxTelegram* telegram) {
  return String(0 + telegram->getTargetArea())   + "."
         + String(0 + telegram->getTargetLine()) + "."
         + String(0 + telegram->getTargetMember());
}

String getTargetGroupAddress(KnxTelegram* telegram) {
  return String(0 + telegram->getTargetMainGroup())     + "/"
         + String(0 + telegram->getTargetMiddleGroup()) + "/"
         + String(0 + telegram->getTargetSubGroup());
}

boolean cyclicSendTimerElapsed() {
  if (cyclicSendInterval > 0) {
    boolean elapsed = ((millis() - cyclicSendTimer) > cyclicSendInterval);
    if (elapsed) cyclicSendTimer += cyclicSendInterval;
    return elapsed;
  } else {
    return false;
  }
}

boolean ledTimeElapsed(unsigned long ms) {
  boolean elapsed = ((millis() - ledTimer) > ms);
  if (elapsed) ledTimer += ms;
  return elapsed;
}

void led(int level) {
  ledLevelBlinks = level;
  switch (ledState) {
    case LED_STATE_OFF: // led is off
      if (ledLevelBlinks == 0) { // show fast init-blinks
        if (ledTimeElapsed(LED_INIT_INTERVAL)) {
          digitalWrite(LED_PIN, HIGH);
          ledState = LED_STATE_INIT_BLINKING;
        }
      } else { // show level-blinks
        if (ledTimeElapsed(LED_LEVEL_INTERVAL)) { // ready for first blink
          digitalWrite(LED_PIN, HIGH);
          ledState = LED_STATE_LEVEL_BLINKING;
          ledLevelBlinked = 1;
        }
      }
      break;

    case LED_STATE_INIT_BLINKING:
      if (ledTimeElapsed(LED_INIT_DURATION)) { // was on long enough
        digitalWrite(LED_PIN, LOW);
        ledState = LED_STATE_OFF;
      }
      break;

    case LED_STATE_LEVEL_BLINKING: // led is on
      if (ledTimeElapsed(LED_LEVEL_DURATION)) { // was on long enough
        digitalWrite(LED_PIN, LOW);
        ledState = (ledLevelBlinked < ledLevelBlinks) ? LED_STATE_BETWEEN_LEVEL_BLINKS : LED_STATE_OFF;
      }
      break;

    case LED_STATE_BETWEEN_LEVEL_BLINKS: // led is off, between blinks
      if (ledTimeElapsed(LED_LEVEL_PAUSE)) { // was off long enough
        digitalWrite(LED_PIN, HIGH);
        ledState = LED_STATE_LEVEL_BLINKING;
        ledLevelBlinked += 1;
      }
      break;
  }
}
