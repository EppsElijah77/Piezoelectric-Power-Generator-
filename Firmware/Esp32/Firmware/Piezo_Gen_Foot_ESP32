#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>

// --- BLE UUIDs (standard BLE UART profile) ---
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// --- Onboard RGB LED ---
#define RGB_PIN   8
#define RGB_COUNT 1
Adafruit_NeoPixel rgb(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

// --- Colors ---
#define COLOR_RED    rgb.Color(255, 0,   0)   // Not connected
#define COLOR_GREEN  rgb.Color(0,   255, 0)   // Connected
#define COLOR_BLUE   rgb.Color(0,   0,   255) // Data sending
#define COLOR_OFF    rgb.Color(0,   0,   0)   // Off

// --- LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- BLE Objects ---
BLEServer*         pServer           = nullptr;
BLECharacteristic* pTxCharacteristic = nullptr;
bool               deviceConnected   = false;

// --- Pin Definitions (ESP32-C6 safe pins) ---
const int voltagePin = 3;  // ADC capable GPIO
const int stepPin    = 4;  // Digital input GPIO

// --- Variables ---
int           stepCount     = 0;
float         totalVoltage  = 0.0;
int           lastStepState = LOW;
unsigned long lastUpdate    = 0;
unsigned long lastStepTime  = 0;
unsigned long lastBlueFlash = 0;
bool          blueFlashOn   = false;
const unsigned long debounceDelay = 50;

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
    pServer->getAdvertising()->start(); // Restart advertising
  }
};

// ---------------------------------------------------
// BLE RX Callback — data received from phone/Pi
// ---------------------------------------------------
class MyRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      Serial.print("BLE RX: ");
      Serial.println(rxValue.c_str());
    }
  }
};

// ---------------------------------------------------
// Helper — send string over BLE TX with blue flash
// ---------------------------------------------------
void blePrint(const char* message) {
  if (deviceConnected && pTxCharacteristic != nullptr) {
    // Flash blue briefly to show data is being sent
    rgb.setPixelColor(0, COLOR_BLUE);
    rgb.show();
    lastBlueFlash = millis();
    blueFlashOn   = true;

    pTxCharacteristic->setValue((uint8_t*)message, strlen(message));
    pTxCharacteristic->notify();
  }
}

// ---------------------------------------------------
// SETUP
// ---------------------------------------------------
void setup() {
  Serial.begin(115200);

  // RGB LED
  rgb.begin();
  rgb.setBrightness(50); // Keep low — 0 to 255
  rgb.clear();
  rgb.show();

  // Step input
  pinMode(stepPin, INPUT);

  // LCD
  Wire.begin(6, 7);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("FOOT STEP POWER");
  lcd.setCursor(0, 1);
  lcd.print("   GENERATOR   ");

  // Red while booting / not yet connected
  rgb.setPixelColor(0, COLOR_RED);
  rgb.show();

  delay(3000);
  lcd.clear();

  // --- BLE Setup ---
  BLEDevice::init("ESP32-C6 FootStep");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  // TX — ESP32 sends to phone/Pi
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // RX — phone/Pi sends to ESP32
  BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyRxCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("BLE started — waiting for connection...");
}

// ---------------------------------------------------
// LOOP
// ---------------------------------------------------
void loop() {
  unsigned long now = millis();

  // --- RGB LED Status ---
  if (blueFlashOn && (now - lastBlueFlash >= 80)) {
    // Return to green after brief blue data flash
    blueFlashOn = false;
    rgb.setPixelColor(0, COLOR_GREEN);
    rgb.show();
  }
  if (!blueFlashOn) {
    if (deviceConnected) {
      rgb.setPixelColor(0, COLOR_GREEN); // Green = connected
    } else {
      rgb.setPixelColor(0, COLOR_RED);   // Red = not connected
    }
    rgb.show();
  }

  // --- Step Detection with debounce (non-blocking) ---
  int currentStepState = digitalRead(stepPin);
  if (currentStepState == HIGH && lastStepState == LOW) {
    if (now - lastStepTime >= debounceDelay) {
      stepCount++;
      lastStepTime = now;
      Serial.printf("Step detected! Total: %d\n", stepCount);
    }
  }
  lastStepState = currentStepState;

  // --- Timed Updates every 100ms ---
  if (now - lastUpdate >= 100) {
    lastUpdate = now;

    // Averaged ADC read to reduce noise
    int sum = 0;
    for (int i = 0; i < 10; i++) sum += analogRead(voltagePin);
    float pinVoltage = (sum / 10.0) * 3.3 / 4095.0;
    totalVoltage = pinVoltage * 11.0;

    // --- LCD Update ---
    char buf0[17];
    char buf1[17];
    sprintf(buf0, "STEP COUNT: %-4d", stepCount);
    sprintf(buf1, "VOLTAGE:%-6.2fV",  totalVoltage);

    lcd.setCursor(0, 0);
    lcd.print(buf0);
    lcd.setCursor(0, 1);
    lcd.print(buf1);

    // --- BLE Send ---
    char bleBuf[64];
    snprintf(bleBuf, sizeof(bleBuf),
             "Steps: %d | Voltage: %.2fV\n", stepCount, totalVoltage);
    blePrint(bleBuf);
  }
}
