#include <Arduino.h>

const int RELAY_CONTROL_PIN = 4;
const int RELAY_CONTACT_PIN = 6;

const unsigned long PAUSE_MS = 1000;
const unsigned long TIMEOUT_MS = 100;
const unsigned long DEBOUNCE_US = 5000;

const int RELAY_ON = HIGH;
const int RELAY_OFF = LOW;


// Дані з interrupt
volatile bool edgeDetected = false;
volatile unsigned long lastEdgeTime = 0;


// Стани
enum State {
  WAIT_BEFORE_ON,
  WAIT_FOR_ON,
  WAIT_BEFORE_OFF,
  WAIT_FOR_OFF
};

State state = WAIT_BEFORE_ON;

unsigned long stateStartMs = 0;
unsigned long commandStartUs = 0;

unsigned long onTime = 0;

unsigned long measurementCount = 0;

uint64_t totalOnTime = 0;
uint64_t totalOffTime = 0;


// =====================================================
// INTERRUPT
// =====================================================

void IRAM_ATTR contactISR() {
  lastEdgeTime = micros();
  edgeDetected = true;
}


// =====================================================
// Перевірка контакту після debounce
// =====================================================

bool contactReady(int expectedLevel, unsigned long &time) {

  if (!edgeDetected) {
    return false;
  }

  unsigned long edge;

  noInterrupts();
  edge = lastEdgeTime;
  interrupts();

  // Чекаємо, поки брязкіт закінчиться
  if (micros() - edge < DEBOUNCE_US) {
    return false;
  }

  // Перевіряємо реальний стан контакту
  if (digitalRead(RELAY_CONTACT_PIN) != expectedLevel) {
    return false;
  }

  edgeDetected = false;

  time = edge;

  return true;
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(RELAY_CONTROL_PIN, OUTPUT);

  // COM -> GND
  // NO  -> GPIO6
  pinMode(RELAY_CONTACT_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_CONTROL_PIN, RELAY_OFF);

  attachInterrupt(
    digitalPinToInterrupt(RELAY_CONTACT_PIN),
    contactISR,
    CHANGE
  );

  stateStartMs = millis();

  Serial.println("Relay test started");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  unsigned long nowMs = millis();


  // ==================================================
  // Чекаємо перед ON
  // ==================================================

  if (state == WAIT_BEFORE_ON) {

    if (nowMs - stateStartMs >= PAUSE_MS) {

      edgeDetected = false;

      commandStartUs = micros();

      digitalWrite(RELAY_CONTROL_PIN, RELAY_ON);

      stateStartMs = nowMs;
      state = WAIT_FOR_ON;
    }
  }


  // ==================================================
  // Чекаємо ON
  // ==================================================

  else if (state == WAIT_FOR_ON) {

    unsigned long edgeTime;

    if (contactReady(LOW, edgeTime)) {

      onTime = edgeTime - commandStartUs;

      stateStartMs = nowMs;
      state = WAIT_BEFORE_OFF;
    }

    // Захист від зависання
    else if (nowMs - stateStartMs >= TIMEOUT_MS) {

      Serial.println("ON TIMEOUT");

      stateStartMs = nowMs;
      state = WAIT_BEFORE_OFF;
    }
  }


  // ==================================================
  // Чекаємо перед OFF
  // ==================================================

  else if (state == WAIT_BEFORE_OFF) {

    if (nowMs - stateStartMs >= PAUSE_MS) {

      edgeDetected = false;

      commandStartUs = micros();

      digitalWrite(RELAY_CONTROL_PIN, RELAY_OFF);

      stateStartMs = nowMs;
      state = WAIT_FOR_OFF;
    }
  }


  // ==================================================
  // Чекаємо OFF
  // ==================================================

  else if (state == WAIT_FOR_OFF) {

    unsigned long edgeTime;

    if (contactReady(HIGH, edgeTime)) {

      unsigned long offTime =
        edgeTime - commandStartUs;

      measurementCount++;

      totalOnTime += onTime;
      totalOffTime += offTime;


      double avgOn =
        (double)totalOnTime / measurementCount;

      double avgOff =
        (double)totalOffTime / measurementCount;


      Serial.print("#");
      Serial.print(measurementCount);

      Serial.print(" | ON: ");
      Serial.print(onTime);
      Serial.print(" us");

      Serial.print(" | OFF: ");
      Serial.print(offTime);
      Serial.print(" us");

      Serial.print(" | AVG ON: ");
      Serial.print(avgOn, 1);

      Serial.print(" us | AVG OFF: ");
      Serial.print(avgOff, 1);

      Serial.println(" us");


      stateStartMs = nowMs;
      state = WAIT_BEFORE_ON;
    }

    // Захист від зависання
    else if (nowMs - stateStartMs >= TIMEOUT_MS) {

      Serial.println("OFF TIMEOUT");

      stateStartMs = nowMs;
      state = WAIT_BEFORE_ON;
    }
  }
}