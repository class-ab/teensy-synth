#include <Arduino.h>
#include <Wire.h>
#include <string.h>

// =====================================================
// Combined MEGA code: Keyboard Scanner + 16 Pots
// =====================================================
#define MEGA_I2C_ADDRESS 0x08
#define I2C_PACKET_SIZE 3
#define I2C_PACKETS_PER_FRAME 4
#define I2C_FRAME_SIZE (I2C_PACKET_SIZE * I2C_PACKETS_PER_FRAME)
#define PACKET_QUEUE_CAPACITY 128
#define PACKET_QUEUE_MASK (PACKET_QUEUE_CAPACITY - 1)

volatile uint8_t packetQueue[PACKET_QUEUE_CAPACITY][I2C_PACKET_SIZE];
volatile uint8_t queueHead = 0;
volatile uint8_t queueTail = 0;
volatile uint8_t queueOverflowCount = 0;
uint8_t lastReportedOverflowCount = 0;

void onI2CRequest();
bool enqueuePacket(uint8_t first, uint8_t second, uint8_t third);
void scanKeyboard();
void reportKeyChanges();
void readPots();

// ---------- Keyboard Matrix ----------
const byte rowPins[8] = {23, 22, 25, 24, 27, 26, 29, 28};     
const byte colPins[8] = {10, 11, 12, 13, 51, 50, 53, 52};
// Deprecated bank from the previous scan: {48, 46, 44, 42, 43, 45, 47, 49}.
bool keyState[8][8];
bool lastKeyState[8][8];
bool noteActive[8][8] = {{false}};

// ---------- Potentiometers ----------
#define NUM_POTS 16
const int potPins[NUM_POTS] = {A0, A1, A2, A3, A4, A5, A6, A7,
                               A8, A9, A10, A11, A12, A13, A14, A15};
int lastPotValues[NUM_POTS];
const int potTolerance = 5;
const int POT_SEND_INTERVAL = 20; // ms
unsigned long lastPotSend = 0;

void setup() {
  Wire.begin(MEGA_I2C_ADDRESS);
  Wire.onRequest(onI2CRequest);
  
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
    enqueuePacket(i + 100, highByte(lastPotValues[i]), lowByte(lastPotValues[i]));
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

  if (queueOverflowCount != lastReportedOverflowCount) {
    lastReportedOverflowCount = queueOverflowCount;
    Serial.print("I2C packet queue overflows: ");
    Serial.println(queueOverflowCount);
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
      bool firstContactPressed = keyState[r][c];
      bool firstContactRose = firstContactPressed && !lastKeyState[r][c];
      byte note = c * 8 + r + 36;

      if (!noteActive[r][c] && firstContactRose) {
        enqueuePacket(1, note, 0);
        noteActive[r][c] = true;
      }

      if (noteActive[r][c] && !firstContactPressed) {
        enqueuePacket(0, note, 0);
        noteActive[r][c] = false;
      }

      lastKeyState[r][c] = firstContactPressed;
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
      
      enqueuePacket(i + 100, highByte(raw), lowByte(raw));
    }
  }
}

// I2C request callbacks run in interrupt context: copy queued bytes only.
void onI2CRequest() {
  uint8_t frame[I2C_FRAME_SIZE];
  memset(frame, 0xFF, sizeof(frame));

  for (uint8_t packet = 0; packet < I2C_PACKETS_PER_FRAME; packet++) {
    uint8_t tail = queueTail;
    if (tail == queueHead) {
      break;
    }

    uint8_t slot = tail & PACKET_QUEUE_MASK;
    for (uint8_t i = 0; i < I2C_PACKET_SIZE; i++) {
      frame[packet * I2C_PACKET_SIZE + i] = packetQueue[slot][i];
    }
    queueTail = tail + 1;
  }

  Wire.write(frame, sizeof(frame));
}

bool enqueuePacket(uint8_t first, uint8_t second, uint8_t third) {
  uint8_t head = queueHead;
  if ((uint8_t)(head - queueTail) >= PACKET_QUEUE_CAPACITY) {
    queueOverflowCount++;
    return false;
  }

  uint8_t slot = head & PACKET_QUEUE_MASK;
  packetQueue[slot][0] = first;
  packetQueue[slot][1] = second;
  packetQueue[slot][2] = third;
  queueHead = head + 1;
  return true;
}
