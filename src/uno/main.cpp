#include <Arduino.h>
#include <Wire.h>

#define UNO_I2C_ADDRESS 0x09
#define NUM_POTS 4
#define I2C_PACKET_SIZE 3
#define I2C_PACKETS_PER_FRAME 4
#define I2C_FRAME_SIZE (I2C_PACKET_SIZE * I2C_PACKETS_PER_FRAME)
#define PACKET_QUEUE_CAPACITY 16
#define PACKET_QUEUE_MASK (PACKET_QUEUE_CAPACITY - 1)

const uint8_t potPins[NUM_POTS] = {A0, A1, A2, A3};
const int potTolerance = 2;
const unsigned long SEND_INTERVAL = 20;

volatile uint8_t packetQueue[PACKET_QUEUE_CAPACITY][I2C_PACKET_SIZE];
volatile uint8_t queueHead = 0;
volatile uint8_t queueTail = 0;
unsigned int lastPotValues[NUM_POTS];
unsigned long lastSendTime = 0;

void onI2CRequest();
bool enqueuePacket(uint8_t first, uint8_t second, uint8_t third);

void setup() {
  Wire.begin(UNO_I2C_ADDRESS);
  Wire.onRequest(onI2CRequest);

  for (uint8_t i = 0; i < NUM_POTS; i++) {
    lastPotValues[i] = analogRead(potPins[i]);
    enqueuePacket(i, highByte(lastPotValues[i]), lowByte(lastPotValues[i]));
  }
}

void loop() {
  unsigned long now = millis();
  if (now - lastSendTime < SEND_INTERVAL) {
    return;
  }
  lastSendTime = now;

  for (uint8_t i = 0; i < NUM_POTS; i++) {
    unsigned int raw = analogRead(potPins[i]);
    if (abs((int)raw - (int)lastPotValues[i]) > potTolerance) {
      lastPotValues[i] = raw;
      enqueuePacket(i, highByte(raw), lowByte(raw));
    }
  }
}

void onI2CRequest() {
  uint8_t frame[I2C_FRAME_SIZE];
  for (uint8_t i = 0; i < I2C_FRAME_SIZE; i++) {
    frame[i] = 0xFF;
  }

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
    return false;
  }

  uint8_t slot = head & PACKET_QUEUE_MASK;
  packetQueue[slot][0] = first;
  packetQueue[slot][1] = second;
  packetQueue[slot][2] = third;
  queueHead = head + 1;
  return true;
}