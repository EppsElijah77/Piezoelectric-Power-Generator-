#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>

// --- BLE UUIDs ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// --- RGB LED ---
#define RGB_PIN   8
#define RGB_COUNT 1
Adafruit_NeoPixel rgb(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

#define COLOR_RED    rgb.Color(255, 0, 0)
#define COLOR_GREEN  rgb.Color(0, 255, 0)
#define COLOR_BLUE   rgb.Color(0, 0, 255)

// --- LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- BLE Objects ---
BLEServer* pServer = nullptr;
BLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;

// --- Analog Voltage Pin ---
const int analogPin = 1;
const float vref = 3.3;
const float dividerRatio = 11.0;

// --- Moving Average ---
const int numSamples = 10;
int readings[numSamples];
int sampleIndex = 0;
long total = 0;

// --- Step Detection ---
const float stepThreshold_mV = 40.0;
const float resetThreshold_mV = 30.0;

bool stepActive = false;
float peakVoltage_mV = 0.0;
int footstepCount = 0;

// --- Timing ---
unsigned long lastUpdate = 0;
unsigned long lastBlueFlash = 0;
bool blueFlashOn = false;

// ---------------------------------------------------
// BLE Server Callbacks
// ---------------------------------------------------
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("BLE Client Connected");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("BLE Client Disconnected");
    pServer->getAdvertising()->start();
  }
};

// ---------------------------------------------------
// BLE RX Callback
// ---------------------------------------------------
class MyRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.print("BLE RX: ");
      Serial.println(rxValue);

      if (rxValue == "reset") {
        footstepCount = 0;
        peakVoltage_mV = 0.0;
        stepActive = false;
        Serial.println("Footstep count reset.");
      }
    }
  }
};

// ---------------------------------------------------
// Read ADC with oversampling
// ---------------------------------------------------
int readADC() {
  long sum = 0;
  const int samples = 20;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(analogPin);
  }

  return sum / samples;
}

// ---------------------------------------------------
// Send BLE message
// ---------------------------------------------------
void blePrint(const char* message) {
  if (deviceConnected && pTxCharacteristic != nullptr) {
    rgb.setPixelColor(0, COLOR_BLUE);
    rgb.show();

    lastBlueFlash = millis();
    blueFlashOn = true;

    pTxCharacteristic->setValue((uint8_t*)message, strlen(message));
    pTxCharacteristic->notify();
  }
}

// ---------------------------------------------------
// Setup
// ---------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(analogPin, ADC_11db);

  for (int i = 0; i < numSamples; i++) {
    readings[i] = 0;
  }

  rgb.begin();
  rgb.setBrightness(50);
  rgb.clear();
  rgb.show();

  Wire.begin(6, 7);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("FOOTSTEP POWER");
  lcd.setCursor(0, 1);
  lcd.print("BLE STARTING");

  rgb.setPixelColor(0, COLOR_RED);
  rgb.show();

  delay(2000);
  lcd.clear();

  BLEDevice::init("ESP32-C6 FootStep");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyRxCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("BLE + peak voltage footstep detection started...");
}

// ---------------------------------------------------
// Loop
// ---------------------------------------------------
void loop() {
  unsigned long now = millis();

  // --- RGB Status ---
  if (blueFlashOn && now - lastBlueFlash >= 80) {
    blueFlashOn = false;
  }

  if (!blueFlashOn) {
    if (deviceConnected) {
      rgb.setPixelColor(0, COLOR_GREEN);
    } else {
      rgb.setPixelColor(0, COLOR_RED);
    }
    rgb.show();
  }

  // --- Run every 50 ms ---
  if (now - lastUpdate >= 50) {
    lastUpdate = now;

    int rawADC = readADC();

    total -= readings[sampleIndex];
    readings[sampleIndex] = rawADC;
    total += readings[sampleIndex];

    sampleIndex = (sampleIndex + 1) % numSamples;

    int avgADC = total / numSamples;

    float adcVoltage_mV = (avgADC / 4095.0) * vref * 1000.0;
    float preDivider_mV = adcVoltage_mV * dividerRatio;

    // Step starts
    if (!stepActive && preDivider_mV > stepThreshold_mV) {
      stepActive = true;
      peakVoltage_mV = preDivider_mV;
    }

    // Track peak
    if (stepActive && preDivider_mV > peakVoltage_mV) {
      peakVoltage_mV = preDivider_mV;
    }

    // Step ends
    if (stepActive && preDivider_mV < resetThreshold_mV) {
      footstepCount++;

      Serial.print("FOOTSTEP DETECTED | Count: ");
      Serial.print(footstepCount);
      Serial.print(" | Peak Voltage: ");
      Serial.print(peakVoltage_mV, 1);
      Serial.println(" mV");

      char bleBuf[80];
      snprintf(
        bleBuf,
        sizeof(bleBuf),
        "Footstep: %d | Peak: %.1f mV\n",
        footstepCount,
        peakVoltage_mV
      );

      blePrint(bleBuf);

      stepActive = false;
      peakVoltage_mV = 0.0;
    }

    // --- LCD Update ---
    lcd.setCursor(0, 0);
    lcd.printf("STEPS:%-9d", footstepCount);

    lcd.setCursor(0, 1);
    lcd.printf("V:%-8.1fmV", preDivider_mV);
  }
}
