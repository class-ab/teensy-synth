#include <Wire.h>
#include <Audio.h>
// =====================================================
// Teensy 3-VCO Synth + MUX + I2C System (MASTER)
// 6 voices × 3 VCOs = 18 waveform generators
// Per-voice VCF and VCA
// =====================================================
#define MEGA_ADDRESS 0x08  // MEGA I2C slave address

// =====================================================
// POTENTIOMETER SCALING CONFIGURATION
// =====================================================
// Define which inputs use LINEAR or LOGARITHMIC scaling
// LINEAR: Direct value mapping (default)
// LOGARITHMIC: Exponential curve for more natural control
// 
// Usage: Set to true for LOGARITHMIC, false for LINEAR
// =====================================================

struct PotScaling {
  const char* name;
  bool isLogarithmic;
};

// MUX Pot Scaling (MUX_0 through MUX_9)
// MUX Pot Scaling (MUX_0 through MUX_9)
PotScaling muxPotScaling[10] = {
  {"MUX_0", false},   // VCO1 Detune - LINEAR
  {"MUX_1", false},   // VCO2 Detune - LINEAR
  {"MUX_2", false},   // VCO3 Detune - LINEAR
  {"MUX_3", false},   // VCO1 Volume - LINEAR
  {"MUX_4", false},   // VCO2 Volume - LINEAR
  {"MUX_5", false},   // VCO3 Volume - LINEAR
  {"MUX_6", false},    // VCF Cutoff - LOGARITHMIC (recommended for filter)
  {"MUX_7", true},   // VCF Attack - LINEAR
  {"MUX_8", true},   // VCF Sustain - LINEAR
  {"MUX_9", true}    // VCF Resonance - LINEAR
};

// MEGA Pot Scaling (MEGA_0 through MEGA_15)
PotScaling megaPotScaling[16] = {
  {"MEGA_0", true},   // VCF Decay - LINEAR
  {"MEGA_1", true},   // VCF Release - LINEAR
  {"MEGA_2", false},   // VCF Env Amount - LINEAR
  {"MEGA_3", false},   // (unused)
  {"MEGA_4", true},   // VCA Attack - LINEAR
  {"MEGA_5", true},   // VCA Decay - LINEAR
  {"MEGA_6", false},   // (unused)
  {"MEGA_7", false},   // (unused)
  {"MEGA_8", true},   // VCA Sustain - LINEAR
  {"MEGA_9", false},   // (unused)
  {"MEGA_10", false},  // (unused)
  {"MEGA_11", true},  // VCA Release - LINEAR
  {"MEGA_12", false},  // Reverb Roomsize - LINEAR
  {"MEGA_13", false},  // Reverb Damping - LINEAR
  {"MEGA_14", false},  // Reverb Mix - LINEAR
  {"MEGA_15", false}   // (unused)
};

// =====================================================
// SCALING FUNCTIONS
// =====================================================
// Convert raw pot value (0-1023) based on scaling type
int applyPotScaling(int rawValue, bool isLogarithmic) {
  if (!isLogarithmic) {
    return rawValue;  // Linear - return as-is
  }
  
  // Logarithmic scaling using exponential curve
  // This creates a more natural feel for controls like filter cutoff
  float normalized = rawValue / 1023.0;
  float scaled = (exp(normalized * 4.6052) - 1.0) / 99.0;  // exp(4.6052) ≈ 100
  return (int)(scaled * 1023.0);
}

// Reverse function - convert from logarithmic to linear if needed
int reversePotScaling(int scaledValue, bool isLogarithmic) {
  if (!isLogarithmic) {
    return scaledValue;  // Linear - return as-is
  }
  
  // Reverse the logarithmic scaling
  float scaled = scaledValue / 1023.0;
  float normalized = log(scaled * 99.0 + 1.0) / 4.6052;
  return (int)(normalized * 1023.0);
}

// =====================================================
// VCO CONTROL MAPPING - EDIT HERE
// =====================================================
// VCO 1 Controls
#define VCO1_WAVEFORM "MUX_10"
#define VCO1_DETUNE   "MUX_0"
#define VCO1_OCTAVE   "MUX_13"
// VCO 2 Controls
#define VCO2_WAVEFORM "MUX_11"
#define VCO2_VOLUME   "MUX_4"
#define VCO2_DETUNE   "MUX_1"
#define VCO2_OCTAVE   "MUX_14"
// VCO 3 Controls
#define VCO3_WAVEFORM "MUX_12"
#define VCO3_VOLUME   "MUX_5"
#define VCO3_DETUNE   "MUX_2"
#define VCO3_OCTAVE   "MUX_15"
// Master Volume Control
#define MASTER_VOLUME "MUX_3"
// VCF (Filter) Controls
#define VCF_CUTOFF     "MUX_6"
#define VCF_RESONANCE  "MUX_9"
#define VCF_ENV_AMOUNT "MEGA_2"
#define VCF_ATTACK     "MUX_7"
#define VCF_DECAY      "MEGA_0"
#define VCF_SUSTAIN    "MUX_8"
#define VCF_RELEASE    "MEGA_1"
// VCA (Amplifier) Controls
#define VCA_ATTACK     "MEGA_4"
#define VCA_DECAY      "MEGA_5"
#define VCA_SUSTAIN    "MEGA_8"
#define VCA_RELEASE    "MEGA_11"
// Reverb Controls
#define REVERB_ROOMSIZE "MEGA_12"
#define REVERB_DAMPING  "MEGA_13"
#define REVERB_MIX      "MEGA_14"

// =====================================================
// MUX Configuration
// =====================================================
const int S0 = 5, S1 = 4, S2 = 3, S3 = 2, SIG_PIN = 14;
const int firstSwitchChannel = 10;
const int numSwitches = 6;
const int totalPositions = 4;
const int switchTolerance = 5;
const int ignoreThreshold = 5;
const int firstPotChannel = 0;
const int numPots = 10;
const int potTolerance = 2;
const int detuneAmt = 1;

int currentPos[numSwitches] = {0};
int lastPotValues[numPots] = {0};

int positions[numSwitches][totalPositions] = {
  {613, 662, 746, 808},
  {411, 464, 558, 643},
  {506, 564, 659, 732},
  {516, 566, 660, 733},
  {462, 513, 610, 690},
  {646, 697, 776, 834}
};

// Storage for MEGA pot values
int megaPotValues[16] = {0};

// =====================================================
// Audio System
// =====================================================
AudioSynthWaveform waveform[18];
AudioMixer4 vcoMixer[6];
AudioFilterStateVariable filter[6];
AudioEffectEnvelope filterEnv[6];
AudioEffectEnvelope ampEnv[6];
AudioMixer4 voiceMixer1, voiceMixer2;
AudioMixer4 preEffectsMix;
AudioEffectFreeverb reverb1;
AudioMixer4 reverbMixer;
AudioMixer4 finalMix;
AudioOutputI2S i2s1;

// Audio connections (same as original)
AudioConnection patchCord1(waveform[0], 0, vcoMixer[0], 0);
AudioConnection patchCord2(waveform[1], 0, vcoMixer[0], 1);
AudioConnection patchCord3(waveform[2], 0, vcoMixer[0], 2);
AudioConnection patchCord4(waveform[3], 0, vcoMixer[1], 0);
AudioConnection patchCord5(waveform[4], 0, vcoMixer[1], 1);
AudioConnection patchCord6(waveform[5], 0, vcoMixer[1], 2);
AudioConnection patchCord7(waveform[6], 0, vcoMixer[2], 0);
AudioConnection patchCord8(waveform[7], 0, vcoMixer[2], 1);
AudioConnection patchCord9(waveform[8], 0, vcoMixer[2], 2);
AudioConnection patchCord10(waveform[9], 0, vcoMixer[3], 0);
AudioConnection patchCord11(waveform[10], 0, vcoMixer[3], 1);
AudioConnection patchCord12(waveform[11], 0, vcoMixer[3], 2);
AudioConnection patchCord13(waveform[12], 0, vcoMixer[4], 0);
AudioConnection patchCord14(waveform[13], 0, vcoMixer[4], 1);
AudioConnection patchCord15(waveform[14], 0, vcoMixer[4], 2);
AudioConnection patchCord16(waveform[15], 0, vcoMixer[5], 0);
AudioConnection patchCord17(waveform[16], 0, vcoMixer[5], 1);
AudioConnection patchCord18(waveform[17], 0, vcoMixer[5], 2);
AudioConnection patchCord19(vcoMixer[0], 0, filter[0], 0);
AudioConnection patchCord20(vcoMixer[1], 0, filter[1], 0);
AudioConnection patchCord21(vcoMixer[2], 0, filter[2], 0);
AudioConnection patchCord22(vcoMixer[3], 0, filter[3], 0);
AudioConnection patchCord23(vcoMixer[4], 0, filter[4], 0);
AudioConnection patchCord24(vcoMixer[5], 0, filter[5], 0);
AudioConnection patchCord31(filter[0], 0, ampEnv[0], 0);
AudioConnection patchCord32(filter[1], 0, ampEnv[1], 0);
AudioConnection patchCord33(filter[2], 0, ampEnv[2], 0);
AudioConnection patchCord34(filter[3], 0, ampEnv[3], 0);
AudioConnection patchCord35(filter[4], 0, ampEnv[4], 0);
AudioConnection patchCord36(filter[5], 0, ampEnv[5], 0);
AudioConnection patchCord37(ampEnv[0], 0, voiceMixer1, 0);
AudioConnection patchCord38(ampEnv[1], 0, voiceMixer1, 1);
AudioConnection patchCord39(ampEnv[2], 0, voiceMixer1, 2);
AudioConnection patchCord40(ampEnv[3], 0, voiceMixer1, 3);
AudioConnection patchCord41(ampEnv[4], 0, voiceMixer2, 0);
AudioConnection patchCord42(ampEnv[5], 0, voiceMixer2, 1);
AudioConnection patchCord43(voiceMixer1, 0, preEffectsMix, 0);
AudioConnection patchCord44(voiceMixer2, 0, preEffectsMix, 1);
AudioConnection patchCord48(preEffectsMix, 0, reverb1, 0);
AudioConnection patchCord49(preEffectsMix, 0, reverbMixer, 0);
AudioConnection patchCord50(reverb1, 0, reverbMixer, 1);
AudioConnection patchCord51(reverbMixer, 0, finalMix, 0);
AudioConnection patchCord52(reverbMixer, 0, finalMix, 1);
AudioConnection patchCord53(finalMix, 0, i2s1, 0);

// =====================================================
// VCO State Variables
// =====================================================
struct VCOSettings {
  int waveform;
  float volume;
  float detune;
  int octave;
};

VCOSettings vco[3] = {
  {WAVEFORM_SINE, 1.0, 0.0, 3},    // VCO1 - volume always 1.0 (controlled by master)
  {WAVEFORM_SINE, 0.33, 0.0, 3},   // VCO2
  {WAVEFORM_SINE, 0.33, 0.0, 3}    // VCO3
};

float masterVolume = 0.5;  // Master volume control

struct FilterSettings {
  float cutoff;
  float resonance;
  float envAmount;
  float attack;
  float decay;
  float sustain;
  float release;
};

FilterSettings vcfSettings = {1000.0, 0.7, 0.0, 10.0, 100.0, 0.5, 200.0};

struct FilterEnvState {
  bool active;
  unsigned long noteOnTime;
  unsigned long noteOffTime;
  bool released;
  float lastValue;
};

FilterEnvState filterEnvState[6];

struct AmplifierSettings {
  float attack;
  float decay;
  float sustain;
  float release;
};

AmplifierSettings vcaSettings = {10.0, 100.0, 0.8, 200.0};

struct ReverbSettings {
  float roomsize;
  float damping;
  float mix;
};

ReverbSettings reverbSettings = {0.5, 0.5, 0.3};

float activeFreq[6] = {0.0};

// =====================================================
// Timing
// =====================================================
unsigned long lastMuxTime = 0;
const unsigned long MUX_INTERVAL = 50;
unsigned long lastFilterUpdate = 0;
const unsigned long FILTER_UPDATE_INTERVAL = 5;
unsigned long lastI2CPoll = 0;
const unsigned long I2C_POLL_INTERVAL = 0.01;  // Poll very fast (2ms) for notes

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Teensy 3-VCO Synth (I2C MASTER) - ±5 Semitone Detune + Master Volume");

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  AudioMemory(180);

  // Initialize waveforms
  for (int i = 0; i < 18; i++) {
    waveform[i].begin(WAVEFORM_SINE);
    waveform[i].amplitude(1.0);
  }

  // Initialize mixers with VCO volumes
  for (int i = 0; i < 6; i++) {
    vcoMixer[i].gain(0, vco[0].volume);
    vcoMixer[i].gain(1, vco[1].volume);
    vcoMixer[i].gain(2, vco[2].volume);
    vcoMixer[i].gain(3, 0.0);
  }

  // Initialize filters and envelopes
  for (int i = 0; i < 6; i++) {
    filter[i].frequency(vcfSettings.cutoff);
    filter[i].resonance(vcfSettings.resonance);
    
    filterEnvState[i].active = false;
    filterEnvState[i].released = false;
    filterEnvState[i].lastValue = 0.0;
    
    filterEnv[i].attack(vcfSettings.attack);
    filterEnv[i].decay(vcfSettings.decay);
    filterEnv[i].sustain(vcfSettings.sustain);
    filterEnv[i].release(vcfSettings.release);
    
    ampEnv[i].attack(vcaSettings.attack);
    ampEnv[i].decay(vcaSettings.decay);
    ampEnv[i].sustain(vcaSettings.sustain);
    ampEnv[i].release(vcaSettings.release);
  }

  for (int i = 0; i < 4; i++) {
    voiceMixer1.gain(i, 0.25);
    voiceMixer2.gain(i, 0.25);
  }

  preEffectsMix.gain(0, 0.5);
  preEffectsMix.gain(1, 0.5);

  reverb1.roomsize(reverbSettings.roomsize);
  reverb1.damping(reverbSettings.damping);
  reverbMixer.gain(0, 1.0 - reverbSettings.mix);
  reverbMixer.gain(1, reverbSettings.mix);

  // Apply master volume to final mix
  finalMix.gain(0, masterVolume);
  finalMix.gain(1, masterVolume);

  // Initialize as I2C master only
  Wire.begin();  // Master mode
  Wire.setClock(400000);  // 400kHz fast mode for quicker polling

  delay(1000);
  Serial.println("I2C Master ready - fast polling MEGA");
  
  // Print pot scaling configuration
  Serial.println("\n=== Pot Scaling Configuration ===");
  Serial.println("MUX Pots:");
  for (int i = 0; i < numPots; i++) {
    Serial.printf("  %s: %s\n", muxPotScaling[i].name, 
                  muxPotScaling[i].isLogarithmic ? "LOGARITHMIC" : "LINEAR");
  }
  Serial.println("MEGA Pots:");
  for (int i = 0; i < 16; i++) {
    Serial.printf("  %s: %s\n", megaPotScaling[i].name, 
                  megaPotScaling[i].isLogarithmic ? "LOGARITHMIC" : "LINEAR");
  }
  Serial.println("=================================");
  Serial.println("VCO Detune Range: ±5 semitones");
  Serial.println("Master Volume: Controlled by MUX_3");
  Serial.println("=================================\n");
  
  // Initial control read
  for (int i = 0; i < numSwitches; i++) {
    readMuxSwitch(firstSwitchChannel + i, i);
  }
  for (int i = 0; i < numPots; i++) {
    readMuxPot(firstPotChannel + i, i);
  }

  updateVCOParameters();
  updateVCFParameters();
  updateVCAParameters();
  updateReverbParameters();
  updateMasterVolume();

  Serial.println("Synth ready");
}

// =====================================================
// Main Loop
// =====================================================
void loop() {
  unsigned long now = millis();

  // Poll MEGA for note events and pot values
  if (now - lastI2CPoll >= I2C_POLL_INTERVAL) {
    lastI2CPoll = now;
    pollMEGA();
  }

  // Update filter envelopes
  if (now - lastFilterUpdate >= FILTER_UPDATE_INTERVAL) {
    lastFilterUpdate = now;
    updateFilterEnvelopes();
  }

  // Read local MUX controls
  if (now - lastMuxTime >= MUX_INTERVAL) {
    lastMuxTime = now;
    
    for (int i = 0; i < numSwitches; i++) {
      readMuxSwitch(firstSwitchChannel + i, i);
    }
    for (int i = 0; i < numPots; i++) {
      readMuxPot(firstPotChannel + i, i);
    }
    
    updateVCOParameters();
    updateVCFParameters();
    updateVCAParameters();
    updateReverbParameters();
    updateMasterVolume();
  }
}

// =====================================================
// I2C Polling - Request data from MEGA
// =====================================================
void pollMEGA() {
  // Request 6 bytes to get 2 events per poll (more efficient)
  int bytesReceived = Wire.requestFrom(MEGA_ADDRESS, 6);
  
  while (bytesReceived >= 3) {
    byte b0 = Wire.read();
    byte b1 = Wire.read();
    byte b2 = Wire.read();
    bytesReceived -= 3;
    
    if (b0 == 0 || b0 == 1) {
      // Note event: cmd, note, velocity
      handleNoteEvent(b0, b1, b2);
    } else if (b0 >= 100 && b0 < 116) {
      // MEGA pot: index (100-115), valueH, valueL
      int megaIndex = b0 - 100;
      int value = (b1 << 8) | b2;
      value = 1023 - value;  // Invert
      
      // Apply scaling based on configuration
      if (megaIndex < 16) {
        value = applyPotScaling(value, megaPotScaling[megaIndex].isLogarithmic);
        megaPotValues[megaIndex] = value;
      }
    } else if (b0 == 255) {
      // Empty slot marker - no more data
      // Clear remaining bytes
      while (Wire.available()) {
        Wire.read();
      }
      break;
    }
  }
}

// =====================================================
// MUX Helpers
// =====================================================
void setMuxChannel(int ch) {
  digitalWrite(S0, bitRead(ch, 0));
  digitalWrite(S1, bitRead(ch, 1));
  digitalWrite(S2, bitRead(ch, 2));
  digitalWrite(S3, bitRead(ch, 3));
  delayMicroseconds(5);
}

void readMuxSwitch(int channel, int switchIndex) {
  setMuxChannel(channel);
  int val = analogRead(SIG_PIN);
  if (val <= ignoreThreshold) return;
  
  for (int p = 0; p < totalPositions; p++) {
    int target = positions[switchIndex][p];
    if (val >= target - switchTolerance && val <= target + switchTolerance) {
      if (currentPos[switchIndex] != p + 1) {
        currentPos[switchIndex] = p + 1;
        Serial.printf("[MUX] Switch %d → pos %d (raw %d)\n",
                      switchIndex + 1, currentPos[switchIndex], val);
      }
      return;
    }
  }
}

void readMuxPot(int channel, int potIndex) {
  setMuxChannel(channel);
  int val = analogRead(SIG_PIN);
  val = 1023 - val;
  
  // Apply scaling based on configuration
  val = applyPotScaling(val, muxPotScaling[potIndex].isLogarithmic);
  
  if (abs(val - lastPotValues[potIndex]) > potTolerance) {
    lastPotValues[potIndex] = val;
  }
}

// =====================================================
// Filter Envelope Calculation
// =====================================================
float calculateFilterEnvelope(int voice, unsigned long now) {
  if (!filterEnvState[voice].active) return 0.0;
  
  if (!filterEnvState[voice].released) {
    unsigned long elapsed = now - filterEnvState[voice].noteOnTime;
    
    if (elapsed < vcfSettings.attack) {
      return (float)elapsed / vcfSettings.attack;
    } else if (elapsed < vcfSettings.attack + vcfSettings.decay) {
      unsigned long decayElapsed = elapsed - vcfSettings.attack;
      float decayProgress = (float)decayElapsed / vcfSettings.decay;
      return 1.0 - (decayProgress * (1.0 - vcfSettings.sustain));
    } else {
      return vcfSettings.sustain;
    }
  } else {
    unsigned long elapsed = now - filterEnvState[voice].noteOffTime;
    
    if (elapsed < vcfSettings.release) {
      float releaseProgress = (float)elapsed / vcfSettings.release;
      return filterEnvState[voice].lastValue * (1.0 - releaseProgress);
    } else {
      filterEnvState[voice].active = false;
      return 0.0;
    }
  }
}

void updateFilterEnvelopes() {
  unsigned long now = millis();
  
  for (int v = 0; v < 6; v++) {
    if (filterEnvState[v].active || vcfSettings.envAmount > 0.01) {
      float envValue = calculateFilterEnvelope(v, now);
      float octaveShift = envValue * vcfSettings.envAmount * 4.0;
      float modulatedCutoff = vcfSettings.cutoff * powf(2.0, octaveShift);
      modulatedCutoff = constrain(modulatedCutoff, 20.0, 12000.0);
      filter[v].frequency(modulatedCutoff);
    }
  }
}

// =====================================================
// Parameter Updates
// =====================================================
void updateVCOParameters() {
  bool changed = false;
  
  // VCO 1 - No volume control (uses master volume)
  int vco1Wave = getControlValue(VCO1_WAVEFORM, true);
  int vco1Det = getControlValue(VCO1_DETUNE, false);
  int vco1Oct = getControlValue(VCO1_OCTAVE, true);
  
  int newWave1 = mapWaveform(vco1Wave);
  if (newWave1 != vco[0].waveform) {
    vco[0].waveform = newWave1;
    changed = true;
  }
  
  // Detune range: ±5 semitones (was ±1)
  float newDet1 = ((vco1Det - 512) / 512.0) * detuneAmt;  // Range: -5 to +5 semitones
  if (abs(newDet1 - vco[0].detune) > 0.01) {
    vco[0].detune = newDet1;
    changed = true;
  }
  
  if (vco1Oct != vco[0].octave && vco1Oct >= 1 && vco1Oct <= 4) {
    vco[0].octave = vco1Oct;
    changed = true;
  }
  
  // VCO 2
  int vco2Wave = getControlValue(VCO2_WAVEFORM, true);
  int vco2Vol = getControlValue(VCO2_VOLUME, false);
  int vco2Det = getControlValue(VCO2_DETUNE, false);
  int vco2Oct = getControlValue(VCO2_OCTAVE, true);
  
  int newWave2 = mapWaveform(vco2Wave);
  if (newWave2 != vco[1].waveform) {
    vco[1].waveform = newWave2;
    changed = true;
  }
  
  float newVol2 = vco2Vol / 1023.0;
  if (abs(newVol2 - vco[1].volume) > 0.01) {
    vco[1].volume = newVol2;
    for (int v = 0; v < 6; v++) {
      vcoMixer[v].gain(1, newVol2);
    }
    changed = true;
  }
  
  // Detune range: ±5 semitones (was ±1)
  float newDet2 = ((vco2Det - 512) / 512.0) * detuneAmt;  // Range: -5 to +5 semitones
  if (abs(newDet2 - vco[1].detune) > 0.01) {
    vco[1].detune = newDet2;
    changed = true;
  }
  
  if (vco2Oct != vco[1].octave && vco2Oct >= 1 && vco2Oct <= 4) {
    vco[1].octave = vco2Oct;
    changed = true;
  }
  
  // VCO 3
  int vco3Wave = getControlValue(VCO3_WAVEFORM, true);
  int vco3Vol = getControlValue(VCO3_VOLUME, false);
  int vco3Det = getControlValue(VCO3_DETUNE, false);
  int vco3Oct = getControlValue(VCO3_OCTAVE, true);
  
  int newWave3 = mapWaveform(vco3Wave);
  if (newWave3 != vco[2].waveform) {
    vco[2].waveform = newWave3;
    changed = true;
  }
  
  float newVol3 = vco3Vol / 1023.0;
  if (abs(newVol3 - vco[2].volume) > 0.01) {
    vco[2].volume = newVol3;
    for (int v = 0; v < 6; v++) {
      vcoMixer[v].gain(2, newVol3);
    }
    changed = true;
  }
  
  // Detune range: ±5 semitones (was ±1)
  float newDet3 = ((vco3Det - 512) / 512.0) * detuneAmt;  // Range: -5 to +5 semitones
  if (abs(newDet3 - vco[2].detune) > 0.01) {
    vco[2].detune = newDet3;
    changed = true;
  }
  
  if (vco3Oct != vco[2].octave && vco3Oct >= 1 && vco3Oct <= 4) {
    vco[2].octave = vco3Oct;
    changed = true;
  }
  
  if (changed) {
    updateActiveVoices();
  }
}

void updateMasterVolume() {
  int masterVal = getControlValue(MASTER_VOLUME, false);
  float newMaster = masterVal / 1023.0;
  
  if (abs(newMaster - masterVolume) > 0.01) {
    masterVolume = newMaster;
    finalMix.gain(0, masterVolume);
    finalMix.gain(1, masterVolume);
    Serial.printf("[MASTER] Volume: %.2f\n", masterVolume);
  }
}

void updateVCFParameters() {
  bool changed = false;
  
  int cutoffVal = getControlValue(VCF_CUTOFF, false);
  float newCutoff = 20.0 * powf(500.0, cutoffVal / 1023.0);
  if (abs(newCutoff - vcfSettings.cutoff) > 10.0) {
    vcfSettings.cutoff = newCutoff;
    changed = true;
  }
  
  int resVal = getControlValue(VCF_RESONANCE, false);
  float newRes = 0.7 + (resVal / 1023.0) * 4.3;
  if (abs(newRes - vcfSettings.resonance) > 0.05) {
    vcfSettings.resonance = newRes;
    for (int i = 0; i < 6; i++) {
      filter[i].resonance(newRes);
    }
    changed = true;
  }
  
  int envAmtVal = getControlValue(VCF_ENV_AMOUNT, false);
  float newEnvAmt = envAmtVal / 1023.0;
  if (abs(newEnvAmt - vcfSettings.envAmount) > 0.01) {
    vcfSettings.envAmount = newEnvAmt;
    changed = true;
  }
  
  int attackVal = getControlValue(VCF_ATTACK, false);
  float newAttack = (attackVal / 1023.0) * 2000.0;
  if (abs(newAttack - vcfSettings.attack) > 5.0) {
    vcfSettings.attack = newAttack;
    for (int i = 0; i < 6; i++) {
      filterEnv[i].attack(newAttack);
    }
    changed = true;
  }
  
  int decayVal = getControlValue(VCF_DECAY, false);
  float newDecay = (decayVal / 1023.0) * 2000.0;
  if (abs(newDecay - vcfSettings.decay) > 5.0) {
    vcfSettings.decay = newDecay;
    for (int i = 0; i < 6; i++) {
      filterEnv[i].decay(newDecay);
    }
    changed = true;
  }
  
  int sustainVal = getControlValue(VCF_SUSTAIN, false);
  float newSustain = sustainVal / 1023.0;
  if (abs(newSustain - vcfSettings.sustain) > 0.01) {
    vcfSettings.sustain = newSustain;
    for (int i = 0; i < 6; i++) {
      filterEnv[i].sustain(newSustain);
    }
    changed = true;
  }
  
  int releaseVal = getControlValue(VCF_RELEASE, false);
  float newRelease = (releaseVal / 1023.0) * 3000.0;
  if (abs(newRelease - vcfSettings.release) > 5.0) {
    vcfSettings.release = newRelease;
    for (int i = 0; i < 6; i++) {
      filterEnv[i].release(newRelease);
    }
    changed = true;
  }
}

void updateVCAParameters() {
  bool changed = false;
  
  int attackVal = getControlValue(VCA_ATTACK, false);
  float newAttack = (attackVal / 1023.0) * 2000.0;
  if (abs(newAttack - vcaSettings.attack) > 5.0) {
    vcaSettings.attack = newAttack;
    for (int i = 0; i < 6; i++) {
      ampEnv[i].attack(newAttack);
    }
    changed = true;
  }
  
  int decayVal = getControlValue(VCA_DECAY, false);
  float newDecay = (decayVal / 1023.0) * 2000.0;
  if (abs(newDecay - vcaSettings.decay) > 5.0) {
    vcaSettings.decay = newDecay;
    for (int i = 0; i < 6; i++) {
      ampEnv[i].decay(newDecay);
    }
    changed = true;
  }
  
  int sustainVal = getControlValue(VCA_SUSTAIN, false);
  float newSustain = sustainVal / 1023.0;
  if (abs(newSustain - vcaSettings.sustain) > 0.01) {
    vcaSettings.sustain = newSustain;
    for (int i = 0; i < 6; i++) {
      ampEnv[i].sustain(newSustain);
    }
    changed = true;
  }
  
  int releaseVal = getControlValue(VCA_RELEASE, false);
  float newRelease = (releaseVal / 1023.0) * 3000.0;
  if (abs(newRelease - vcaSettings.release) > 5.0) {
    vcaSettings.release = newRelease;
    for (int i = 0; i < 6; i++) {
      ampEnv[i].release(newRelease);
    }
    changed = true;
  }
}

void updateReverbParameters() {
  bool changed = false;
  
  // Reverb room size: 0.0 to 1.0
  int roomVal = getControlValue(REVERB_ROOMSIZE, false);
  float newRoom = roomVal / 1023.0;
  if (abs(newRoom - reverbSettings.roomsize) > 0.01) {
    reverbSettings.roomsize = newRoom;
    reverb1.roomsize(newRoom);
    changed = true;
  }
  
  // Reverb damping: 0.0 to 1.0
  int dampVal = getControlValue(REVERB_DAMPING, false);
  float newDamp = dampVal / 1023.0;
  if (abs(newDamp - reverbSettings.damping) > 0.01) {
    reverbSettings.damping = newDamp;
    reverb1.damping(newDamp);
    changed = true;
  }
  
  // Reverb mix: 0.0 to 1.0
  int mixVal = getControlValue(REVERB_MIX, false);
  float newMix = mixVal / 1023.0;
  if (abs(newMix - reverbSettings.mix) > 0.01) {
    reverbSettings.mix = newMix;
    reverbMixer.gain(0, 1.0 - newMix);  // Dry
    reverbMixer.gain(1, newMix);         // Wet
    changed = true;
  }
  
  if (changed) {
    Serial.println("[REVERB] Parameters updated");
  }
}

// =====================================================
// Control Value Getter
// =====================================================
int getControlValue(const char* control, bool isSwitch) {
  if (strncmp(control, "MUX_", 4) == 0) {
    int channel = atoi(control + 4);
    if (isSwitch) {
      int switchIndex = channel - firstSwitchChannel;
      if (switchIndex >= 0 && switchIndex < numSwitches) {
        return currentPos[switchIndex];
      }
    } else {
      int potIndex = channel - firstPotChannel;
      if (potIndex >= 0 && potIndex < numPots) {
        return lastPotValues[potIndex];
      }
    }
  } else if (strncmp(control, "MEGA_", 5) == 0) {
    int index = atoi(control + 5);
    if (index >= 0 && index < 16) {
      return megaPotValues[index];
    }
  }
  return 0;
}

// =====================================================
// Waveform Mapper
// =====================================================
int mapWaveform(int position) {
  switch (position) {
    case 1: return WAVEFORM_SINE;
    case 2: return WAVEFORM_SAWTOOTH;
    case 3: return WAVEFORM_SQUARE;
    case 4: return WAVEFORM_TRIANGLE;
    default: return WAVEFORM_SINE;
  }
}

// =====================================================
// Synth Voice Handling
// =====================================================
void handleNoteEvent(byte cmd, byte note, byte vel) {
  float baseFreq = 440.0 * powf(2.0, (note - 69) / 12.0);
  
  if (cmd == 1) {  // NoteOn
    Serial.println("NOTE RECEIVED");
    
    // First, try to find a completely free voice (not playing or releasing)
    int freeVoice = -1;
    for (int v = 0; v < 6; v++) {
      if (activeFreq[v] == 0.0 && !ampEnv[v].isActive()) {
        freeVoice = v;
        break;
      }
    }
    
    // If no completely free voice, find one that's not actively held (can be releasing)
    if (freeVoice == -1) {
      for (int v = 0; v < 6; v++) {
        if (activeFreq[v] == 0.0) {
          freeVoice = v;
          break;
        }
      }
    }
    
    // If still no free voice, steal the oldest voice
    if (freeVoice == -1) {
      freeVoice = 0;  // Simple voice stealing - take voice 0
    }
    
    if (freeVoice != -1) {
      int v = freeVoice;
      
      // Set frequency immediately
      activeFreq[v] = baseFreq;
      
      // Set all 3 VCOs for this voice
      for (int vco_num = 0; vco_num < 3; vco_num++) {
        int wfIndex = v * 3 + vco_num;
        
        // Calculate frequency with octave shift and detune
        int octaveShift = vco[vco_num].octave - 3;  // Convert 1-4 to -2,-1,0,+1
        float octaveMultiplier = powf(2.0, octaveShift);
        // Detune is now in semitones (-5 to +5), convert to frequency multiplier
        float detunedFreq = activeFreq[v] * octaveMultiplier * powf(2.0, vco[vco_num].detune / 12.0);
        
        waveform[wfIndex].begin(vco[vco_num].waveform);
        waveform[wfIndex].frequency(detunedFreq);
        waveform[wfIndex].amplitude(1.0);  // Full amplitude, VCA controls volume
      }
      
      // Trigger filter and amplitude envelopes for this voice
      filterEnvState[v].active = true;
      filterEnvState[v].noteOnTime = millis();
      filterEnvState[v].released = false;
      filterEnvState[v].lastValue = 0.0;
      
      filterEnv[v].noteOn();
      ampEnv[v].noteOn();
    }
  } else {  // NoteOff
    for (int v = 0; v < 6; v++) {
      if (fabs(activeFreq[v] - baseFreq) < 1.0) {
        activeFreq[v] = 0.0;
        
        // Trigger release phase of envelopes
        unsigned long now = millis();
        filterEnvState[v].noteOffTime = now;
        filterEnvState[v].released = true;
        filterEnvState[v].lastValue = calculateFilterEnvelope(v, now);
        
        filterEnv[v].noteOff();
        ampEnv[v].noteOff();
        
        break;  // Only release one voice per note-off
      }
    }
  }
}

// =====================================================
// Update Active Voices (for detune/octave changes)
// =====================================================
void updateActiveVoices() {
  for (int v = 0; v < 6; v++) {
    if (activeFreq[v] > 0.0) {
      for (int vco_num = 0; vco_num < 3; vco_num++) {
        int wfIndex = v * 3 + vco_num;
        
        // Calculate frequency with octave shift and detune
        int octaveShift = vco[vco_num].octave - 3;  // Convert 1-4 to -2,-1,0,+1
        float octaveMultiplier = powf(2.0, octaveShift);
        // Detune is now in semitones (-5 to +5), convert to frequency multiplier
        float detunedFreq = activeFreq[v] * octaveMultiplier * powf(2.0, vco[vco_num].detune / 12.0);
        
        waveform[wfIndex].frequency(detunedFreq);
        waveform[wfIndex].begin(vco[vco_num].waveform);
      }
    }
  }
}
