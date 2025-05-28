#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h> // use lower power?  no serial for battery  \ battery led? (cad for assignment)

// --- Variables ---
const int BUZZER_PIN = 9;
bool isCurrentlyAlarming = false;
const int RSSI_ALARM_ON_THRESHOLD = -65;  
const int RSSI_ALARM_OFF_THRESHOLD = -50;
float smoothedRSSIValue = 0.0;         
const float EMA_ALPHA = 0.3;           
bool firstEMAreading = true;           
const int SMA_SAMPLE_COUNT = 10;        

static BLEUUID serviceUUID("544d2995-da8b-4759-ba1d-7f165532ce72");
static BLEUUID charUUIDAlarm("6876e650-c946-47e3-93a4-8a21762c321e");
static boolean doConnect = false;
static boolean connected = false;
static BLEAdvertisedDevice* myDevice;
static BLERemoteCharacteristic* pRemoteAlarmCharacteristic;
BLEClient*  pClient = NULL;

void startBuzzing() {
  if (!isCurrentlyAlarming) {
    Serial.println("CLIENT: Starting Buzzer");
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

bool writeAlarmStateToServer(bool alarmOn) {
    if (!connected || pRemoteAlarmCharacteristic == nullptr) {
        return false;
    }
    if (pRemoteAlarmCharacteristic->canWrite()) {
        if (alarmOn) {
            pRemoteAlarmCharacteristic->writeValue("1", false); 
        } else {
            pRemoteAlarmCharacteristic->writeValue("0", false);
        }
        return true;
    } else {
        return false;
    }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    firstEMAreading = true; // Reset EMA
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    doConnect = false; // rescan
    stopBuzzing(); // this stops buzzing if it disconnects maybe change?
  }
};

bool connectToServer() {
    pClient  = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    if (!pClient->connect(myDevice)) { //check if the connection has failed
        delete pClient;
        pClient = nullptr;
        return false;
    }
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {//check find bluetooth service
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
      return false;
    }
    pRemoteAlarmCharacteristic = pRemoteService->getCharacteristic(charUUIDAlarm);
    if (pRemoteAlarmCharacteristic == nullptr) { //check find uuid
      Serial.println(charUUIDAlarm.toString().c_str());
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
      return false;
    }
    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      if (myDevice != nullptr) { 
          delete myDevice;
          myDevice = nullptr;
      }
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true; 
    }
  }
};

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off
  BLEDevice::init(""); 
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); 
  pBLEScan->setInterval(1349); 
  pBLEScan->setWindow(449);   
}

void loop() {
  if (doConnect == true) { //flag for attempting to initilize connection
    if (connectToServer()) {
      doConnect = false; 
    } else { // if it fails to connect
      doConnect = false; 
      if (myDevice != nullptr) { 
          delete myDevice;
          myDevice = nullptr;
      }
    }
  }

  if (connected && pClient != nullptr && pClient->isConnected()) {
    float sumOfRawRSSI = 0;
    int validReadingsCount = 0;

    for (int i = 0; i < SMA_SAMPLE_COUNT; i++) {
        int rawRSSI = pClient->getRssi(); 
        if (rawRSSI != 0 && rawRSSI < 0) {
            sumOfRawRSSI += rawRSSI;
            validReadingsCount++;
        }
        // delay(5); 
    }

    if (validReadingsCount > 0) {
        float currentAverageRSSI = sumOfRawRSSI / validReadingsCount;

        if (firstEMAreading) {
            smoothedRSSIValue = currentAverageRSSI; // Initialize with the first valid average
            firstEMAreading = false;
        } else {
            smoothedRSSIValue = (EMA_ALPHA * currentAverageRSSI) + ((1.0 - EMA_ALPHA) * smoothedRSSIValue);
        }

        if (!isCurrentlyAlarming && smoothedRSSIValue < RSSI_ALARM_ON_THRESHOLD) {
            startBuzzing();
            writeAlarmStateToServer(true); 
        } else if (isCurrentlyAlarming && smoothedRSSIValue > RSSI_ALARM_OFF_THRESHOLD) {
            stopBuzzing();
            writeAlarmStateToServer(false); 
        }
    } else {
        Serial.println("CLIENT: No valid RSSI readings for current average.");
        // out of range / lost / other side dead?
    }

  } else if (!doConnect) { 
    BLEDevice::getScan()->start(5, false);
  }

  delay(1000); 
}