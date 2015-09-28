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
#define KNX_GA_SEND "0/7/1"
#define KNX_GA_LISTEN "9/2/0"


// DEBUGGING ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define DEBUG true


// LED ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define LED_PIN LED_BUILTIN

#define LED_INIT_INTERVAL 100 // time between init-blinks
#define LED_INIT_DURATION 100 // duration of an init-blink

#define LED_LEVEL_INTERVAL 1500 // time between level-blinks
#define LED_LEVEL_DURATION 250  // duration of a single level-blink
#define LED_LEVEL_DISTANCE 250  // time between single level-blinks

#define LED_STATE_OFF                  0 // led is just off
#define LED_STATE_INIT_BLINKING        1 // led is on and doing init-blinks
#define LED_STATE_LEVEL_BLINKING       3 // led is on and doing level-blinks
#define LED_STATE_BETWEEN_LEVEL_BLINKS 4 // led is off, before another level-blink

byte ledState;          // current state of LED
byte ledLevelBlinks;    // level-blinks to show
byte ledLevelBlinked;   // level-blinks shown in current cycle
unsigned long ledTimer; // http://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html


// CORE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define STATE_INITIAL  0
#define STATE_INIT_TVZ 1
#define STATE_WORKING  2

byte state; // current application-state
byte fanLevel; // current fan-level


// INIT ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
KnxTpUart knx(&Serial, KNX_PA_SELF); // Init KNX TP-UART on Serial


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void setup() {
  initLED();
  initKNX();
  initTVZ();
  initTimers();

  state = STATE_INITIAL;
}

void initLED() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledState = LED_STATE_OFF;
  ledLevelBlinks = ledLevelBlinked = 0;
}

void initKNX() {
  // Serial: Pin 0 (RX) and 1 (TX).
  Serial.begin(19200); // connect to serial
  knx.uartReset();

  // listen to KNX GAs
  knx.addListenGroupAddress(KNX_GA_LISTEN);
  delay(1000); //TODO: why?
}

void initTVZ() {
  fanLevel = 0;
}

void initTimers() {
  ledTimer = millis();
}

// LOOP #############################################################
void loop() {
  led(fanLevel);

  switch (state) {
    case STATE_INITIAL:
      state = STATE_INIT_TVZ;
      break;

    case STATE_INIT_TVZ:
      
      break;

    case STATE_WORKING:

      break;
  }
}

boolean led(boolean on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
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
          led(true);
          ledState = LED_STATE_INIT_BLINKING;
        }
      } else { // show level-blinks
        if (ledTimeElapsed(LED_LEVEL_INTERVAL)) { // ready for first blink
          led(true);
          ledState = LED_STATE_LEVEL_BLINKING;
          ledLevelBlinked = 1;
        }
      }
      break;

    case LED_STATE_INIT_BLINKING:
      if (ledTimeElapsed(LED_INIT_DURATION)) { // was on long enough
        led(false);
        ledState = LED_STATE_OFF;
      }
      break;

    case LED_STATE_LEVEL_BLINKING: // led is on
      if (ledTimeElapsed(LED_LEVEL_DURATION)) { // was on long enough
        led(false);
        ledState = (ledLevelBlinked < ledLevelBlinks) ? LED_STATE_BETWEEN_LEVEL_BLINKS : LED_STATE_OFF;
      }
      break;

    case LED_STATE_BETWEEN_LEVEL_BLINKS: // led is off, between blinks
      if (ledTimeElapsed(LED_LEVEL_DISTANCE)) { // was off long enough
        led(true);
        ledState = LED_STATE_LEVEL_BLINKING;
        ledLevelBlinked += 1;
      }
      break;
  }
}


void onReadToPhysicalAddress(KnxTelegram* telegram, String targetAddress) {

}

void onReadToGroupAddress(KnxTelegram* telegram, String targetAddress) {

}

void onWriteToPhysicalAddress(KnxTelegram* telegram, String targetAddress) {

}

void onWriteToGroupAddress(KnxTelegram* telegram, String targetAddress) {

}

void onTelegram(KnxTelegram* telegram) {
  switch (telegram->getCommand()) {
    case KNX_COMMAND_READ:
      if (telegram->isTargetGroup()) {
        String targetAddress = getTargetGroupAddress(telegram);
        if (DEBUG) debugPrint("KNX_TELEGRAM/READ@GA(" + targetAddress + ")");
        onReadToGroupAddress(telegram, targetAddress);
      } else {
        String targetAddress = getTargetPhysicalAddress(telegram);
        if (DEBUG) debugPrint("KNX_TELEGRAM/READ@PA(" + targetAddress + ")");
        onReadToPhysicalAddress(telegram, targetAddress);
      }
      break;

    case KNX_COMMAND_WRITE:
      if (telegram->isTargetGroup()) {
        String targetAddress = getTargetGroupAddress(telegram);
        if (DEBUG) debugPrint("KNX_TELEGRAM/WRITE@GA(" + targetAddress + ")");
        onWriteToGroupAddress(telegram, targetAddress);
      } else {
        String targetAddress = getTargetPhysicalAddress(telegram);
        if (DEBUG) debugPrint("KNX_TELEGRAM/WRITE@PA(" + targetAddress + ")");
        onWriteToPhysicalAddress(telegram, targetAddress);
      }
      break;

    case KNX_COMMAND_ANSWER:
      if (DEBUG) debugPrint("KNX_TELEGRAM/ANSWER");
      break;

    case KNX_COMMAND_INDIVIDUAL_ADDR_WRITE: // programming-mode
      if (DEBUG) debugPrint("KNX_TELEGRAM/INDIVIDUAL_ADDR_WRITE");
      break;

    case KNX_COMMAND_INDIVIDUAL_ADDR_REQUEST:
      if (DEBUG) debugPrint("KNX_TELEGRAM/INDIVIDUAL_ADDR_REQUEST");
      break;

    case KNX_COMMAND_INDIVIDUAL_ADDR_RESPONSE:
      if (DEBUG) debugPrint("KNX_TELEGRAM/INDIVIDUAL_ADDR_RESPONSE");
      break;

    case KNX_COMMAND_MASK_VERSION_READ:
      if (DEBUG) debugPrint("KNX_TELEGRAM/MASK_VERSION_READ");
      break;

    case KNX_COMMAND_MASK_VERSION_RESPONSE:
      if (DEBUG) debugPrint("KNX_TELEGRAM/MASK_VERSION_RESPONSE");
      break;

    case KNX_COMMAND_RESTART:
      if (DEBUG) debugPrint("KNX_TELEGRAM/RESTART");
      break;

    case KNX_COMMAND_ESCAPE:
      if (DEBUG) debugPrint("KNX_TELEGRAM/ESCAPE");
      break;
  }
}

void serialEvent() {
  KnxTpUartSerialEventType evtType = knx.serialEvent();
  switch (evtType) {
    case TPUART_RESET_INDICATION:
      if (DEBUG) debugPrint("TPUART_RESET_INDICATION");
      break;

    case KNX_TELEGRAM:
      onTelegram( knx.getReceivedTelegram() );
      break;

    case IRRELEVANT_KNX_TELEGRAM:
      if (DEBUG) debugPrint("IRRELEVANT_KNX_TELEGRAM");
      break;

    case UNKNOWN:
      if (DEBUG) debugPrint("UNKNOWN");
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

void debugPrint(String msg) {
  Serial.println(msg);
}
