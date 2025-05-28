#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

const int BUZZER_PIN = 9;
const int RSSI_THRESHOLD = -80;
bool isCurrentlyAlarming = false;
BLEServer* pServer = NULL;
BLECharacteristic* pAlarmCharacteristic = NULL;
bool deviceConnected = false;

void startBuzzing() {
  if (!isCurrentlyAlarming) {
    digitalWrite(BUZZER_PIN, HIGH);
    isCurrentlyAlarming = true;
  }
}
void stopBuzzing() {
  if (isCurrentlyAlarming) {
    digitalWrite(BUZZER_PIN, LOW);
    isCurrentlyAlarming = false;
  }
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServerInstance) {
      deviceConnected = true;
      pServer->getAdvertising()->stop(); // Stop advertising once connected
    }

    void onDisconnect(BLEServer* pServerInstance) {
      deviceConnected = false;
      stopBuzzing(); // Stop buzzing if client disconnects
      delay(500); // give BLE stack time to process
      pServer->getAdvertising()->start(); // Restart advertising
    }
};

class AlarmCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value_from_client = pCharacteristic->getValue(); 
        if (value_from_client.length() > 0) {
            if (value_from_client == "1") { // Client signals "out of range"
                startBuzzing();
            } else if (value_from_client == "0") { // Client signals "in range"
                stopBuzzing();
            }
        }
    }
};

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off
  BLEDevice::init("ESP32_Proximity_Server"); // Give your BLE server a name
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService("544d2995-da8b-4759-ba1d-7f165532ce72");

  pAlarmCharacteristic = pService->createCharacteristic("6876e650-c946-47e3-93a4-8a21762c321e",BLECharacteristic::PROPERTY_READ |BLECharacteristic::PROPERTY_WRITE |BLECharacteristic::PROPERTY_NOTIFY);
  pAlarmCharacteristic->setCallbacks(new AlarmCharacteristicCallbacks());
  pAlarmCharacteristic->addDescriptor(new BLE2902()); // For notifications
  pAlarmCharacteristic->setValue("0"); // Initial value

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("544d2995-da8b-4759-ba1d-7f165532ce72");
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // i think this helps iphones? 
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

void loop() {
  if (!deviceConnected && isCurrentlyAlarming) { // disables buzzing if it disonnect ajust to add periodic beeper if you want
      stopBuzzing();
  }
  delay(100); 
}