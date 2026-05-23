#include <Wire.h>

// =====================================================
// Combined MEGA code: Keyboard Scanner + 16 Pots
// =====================================================
#define TEENSY_ADDR 8

// ---------- Keyboard Matrix ----------
const byte rowPins[8] = {23, 22, 25, 24, 27, 26, 29, 28};     
const byte colPins[8] = {10, 11, 12, 13, 51, 50, 53, 52};     
bool keyState[8][8];
bool lastKeyState[8][8];

// ---------- Potentiometers ----------
#define NUM_POTS 16
const int potPins[NUM_POTS] = {A0, A1, A2, A3, A4, A5, A6, A7,
                               A8, A9, A10, A11, A12, A13, A14, A15};
int lastPotValues[NUM_POTS];
const int potTolerance = 5;
const int POT_SEND_INTERVAL = 20; // ms
unsigned long lastPotSend = 0;

// I2C error tracking and rate limiting
unsigned long i2cErrorCount = 0;
unsigned long lastErrorReport = 0;
unsigned long lastI2CSend = 0;
const unsigned long I2C_MIN_INTERVAL = 2; // Minimum 2ms between I2C transmissions

// =====================================================
// Setup
// =====================================================
void setup() {
  Wire.begin(); // MEGA as I2C master
  Wire.setTimeout(10000); // 10ms timeout (increased from 5ms)
  Wire.setClock(100000); // Explicitly set to 100kHz (standard mode)
  
  Serial.begin(115200);
  Serial.println("MEGA Keyboard + Pot controller ready");
  
  // Keyboard pin setup
  for (int c = 0; c < 8; c++) {
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], HIGH);
  }
  for (int r = 0; r < 8; r++) {
    pinMode(rowPins[r], INPUT_PULLUP);
  }
  
  // Initialize arrays
  memset(keyState, 0, sizeof(keyState));
  memset(lastKeyState, 0, sizeof(lastKeyState));
  
  // Initialize pot readings
  for (int i = 0; i < NUM_POTS; i++) {
    lastPotValues[i] = analogRead(potPins[i]);
  }
  
  delay(100); // Give Teensy time to initialize
}

// =====================================================
// Main Loop
// =====================================================
void loop() {
  scanKeyboard();
  reportKeyChanges();
  
  unsigned long now = millis();
  if (now - lastPotSend >= POT_SEND_INTERVAL) {
    lastPotSend = now;
    readPots();
  }
  
  // Report I2C errors periodically
  if (i2cErrorCount > 0 && now - lastErrorReport > 5000) {
    Serial.print("I2C errors: ");
    Serial.println(i2cErrorCount);
    lastErrorReport = now;
  }
}

// =====================================================
// --- Keyboard Scanning ---
// =====================================================
void scanKeyboard() {
  for (int c = 0; c < 8; c++) {
    digitalWrite(colPins[c], LOW);
    delayMicroseconds(30);
    for (int r = 0; r < 8; r++) {
      keyState[r][c] = (digitalRead(rowPins[r]) == LOW);
    }
    digitalWrite(colPins[c], HIGH);
  }
}

void reportKeyChanges() {
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (keyState[r][c] != lastKeyState[r][c]) {
        byte note = c * 8 + r + 36;
        byte cmd = keyState[r][c] ? 1 : 0;
        byte vel = keyState[r][c] ? 100 : 0;
        
        // Wait for minimum interval between I2C sends
        waitForI2CReady();
        
        sendNoteEvent(cmd, note, vel);
        lastKeyState[r][c] = keyState[r][c];
        Serial.print(cmd ? "NoteOn " : "NoteOff ");
        Serial.println(note);
      }
    }
  }
}

// =====================================================
// --- Potentiometer Reading ---
// =====================================================
void readPots() {
  for (int i = 0; i < NUM_POTS; i++) {
    int raw = analogRead(potPins[i]);
    if (abs(raw - lastPotValues[i]) > potTolerance) {
      lastPotValues[i] = raw;
      
      // Wait for minimum interval between I2C sends
      waitForI2CReady();
      
      sendPot(i + 100, raw);
    }
  }
}

// =====================================================
// --- I2C Helpers ---
// =====================================================
void waitForI2CReady() {
  unsigned long now = micros();
  unsigned long elapsed = now - lastI2CSend;
  
  // If less than minimum interval has passed, wait
  if (elapsed < (I2C_MIN_INTERVAL * 1000)) {
    delayMicroseconds((I2C_MIN_INTERVAL * 1000) - elapsed);
  }
}

bool safeI2CSend(byte* data, int len) {
  Wire.beginTransmission(TEENSY_ADDR);
  
  for (int i = 0; i < len; i++) {
    Wire.write(data[i]);
  }
  
  byte error = Wire.endTransmission(true); // true = send stop bit
  lastI2CSend = micros();
  
  if (error != 0) {
    i2cErrorCount++;
    
    // Try to recover the bus
    Wire.end();
    delayMicroseconds(100);
    Wire.begin();
    Wire.setTimeout(10000);
    Wire.setClock(50000);
    
    return false;
  }
  
  return true;
}

void sendNoteEvent(byte cmd, byte note, byte vel) {
  byte data[3] = {cmd, note, vel};
  
  if (safeI2CSend(data, 3)) {
    Serial.println("SEND NOTE");
  } else {
    Serial.print("NOTE I2C ERROR: ");
    Serial.println(Wire.getWriteError());
  }
}

void sendPot(int index, int value) {
  byte data[3] = {(byte)index, highByte(value), lowByte(value)};
  
  if (safeI2CSend(data, 3)) {
    Serial.println("SEND POT");
  } else {
    Serial.print("POT I2C ERROR: ");
    Serial.println(Wire.getWriteError());
  }
}
