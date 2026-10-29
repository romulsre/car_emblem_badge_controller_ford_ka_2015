#include "ble_handler.h"
#include "commands_handler.h"
#include "MediaStorage.h" // Image rendering and storage utilities
#include <Update.h>

// Image list is defined by the main sketch.
extern std::vector<String> myBinFiles; 
// Animation state is defined by the main sketch.
extern bool isPlayingAnimation;
extern int currentAnimationNum;

static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;
static BLECharacteristic* pOtaStatusCharacteristic = nullptr;
static bool deviceConnected = false;
static bool oldDeviceConnected = false;
static String bleRxBuffer = "";
static bool bleDataReceived = false;
static bool otaInProgress = false;
static bool otaRebootPending = false;
static size_t otaExpectedSize = 0;
static size_t otaWrittenSize = 0;
static size_t otaLastReportedSize = 0;
static uint32_t otaRebootAt = 0;

static void sendOtaStatus(const char* status) {
    if (deviceConnected && pOtaStatusCharacteristic != nullptr) {
        pOtaStatusCharacteristic->setValue(status);
        pOtaStatusCharacteristic->notify();
    }
}

static void abortOta(const char* status) {
    if (otaInProgress) Update.abort();
    otaInProgress = false;
    otaExpectedSize = 0;
    otaWrittenSize = 0;
    otaLastReportedSize = 0;
    sendOtaStatus(status);
}

// Callbacks track phone connections and disconnections.
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        ////Serial.println("[BLE] Телефон подключился!");
    };

    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        if (otaInProgress) {
            Update.abort();
            otaInProgress = false;
        }
        ////Serial.println("[BLE] Телефон отключился.");
    }
};

// OTA control: "START <password> <firmware_size>", "END", or "ABORT".
class OtaControlCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        String command = pCharacteristic->getValue().c_str();
        command.trim();

        if (command.equalsIgnoreCase("ABORT")) {
            abortOta("ABORTED");
            return;
        }
        if (command.equalsIgnoreCase("END")) {
            if (!otaInProgress) {
                sendOtaStatus("ERR not started");
            } else if (otaWrittenSize != otaExpectedSize) {
                abortOta("ERR size mismatch");
            } else if (!Update.end()) {
                abortOta("ERR finalize");
            } else {
                otaInProgress = false;
                otaRebootPending = true;
                otaRebootAt = millis() + 800;
                sendOtaStatus("OK rebooting");
            }
            return;
        }
        if (!command.startsWith("START ")) {
            sendOtaStatus("ERR use START <password> <size>, END or ABORT");
            return;
        }

        String arguments = command.substring(6);
        arguments.trim();
        int separator = arguments.indexOf(' ');
        if (separator < 0) {
            sendOtaStatus("ERR password required");
            return;
        }
        String password = arguments.substring(0, separator);
        String sizeText = arguments.substring(separator + 1);
        sizeText.trim();
        if (password != OTA_PASSWORD) {
            sendOtaStatus("ERR unauthorized");
            return;
        }
        char* end = nullptr;
        unsigned long size = strtoul(sizeText.c_str(), &end, 10);
        if (!sizeText.length() || *end != '\0' || size == 0) {
            sendOtaStatus("ERR invalid size");
            return;
        }

        if (otaInProgress) Update.abort();
        otaInProgress = Update.begin(size, U_FLASH);
        if (!otaInProgress) {
            sendOtaStatus("ERR begin");
            return;
        }
        otaExpectedSize = size;
        otaWrittenSize = 0;
        otaLastReportedSize = 0;
        sendOtaStatus("READY");
    }
};

// Receives raw .bin bytes. Do not convert them to String: firmware may contain
// null bytes.
class OtaDataCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        if (!otaInProgress) {
            sendOtaStatus("ERR not started");
            return;
        }
        String packet = pCharacteristic->getValue();
        size_t packetSize = packet.length();
        if (packetSize == 0) return;
        if (packetSize > otaExpectedSize - otaWrittenSize) {
            abortOta("ERR too much data");
            return;
        }
        uint8_t* packetBytes = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet.c_str()));
        if (Update.write(packetBytes, packetSize) != packetSize) {
            abortOta("ERR write");
            return;
        }
        otaWrittenSize += packetSize;
        if (otaWrittenSize - otaLastReportedSize >= 4096 || otaWrittenSize == otaExpectedSize) {
            otaLastReportedSize = otaWrittenSize;
            String progress = "PROGRESS " + String(otaWrittenSize) + "/" + String(otaExpectedSize);
            sendOtaStatus(progress.c_str());
        }
    }
};

// Callback for data received from the phone (RX).
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        ////Serial.println("\n[BLE] --- ВХОДЯЩИЙ ПАКЕТ ---");
        
// Read the data directly into an Arduino String.
        String rxValue = pCharacteristic->getValue();
        //Serial.printf("Длина пакета: %d байт\n", rxValue.length());
        
// Print each byte as HEX when debugging hidden characters.
        //Serial.print("Байты (HEX): ");
        for (size_t i = 0; i < rxValue.length(); i++) {
            //Serial.printf("0x%02X ", (uint8_t)rxValue[i]);
        }
        ////Serial.println();

// Print the data as text.
        //Serial.print("Как текст: \"");
        //Serial.print(rxValue);
        ////Serial.println("\"");
        
        if (rxValue.length() > 0) {
            bleRxBuffer += rxValue;
            
// Remove whitespace and invalid input.
            bleRxBuffer.replace("\r", "");
            bleRxBuffer.replace("\n", "");
            bleRxBuffer.trim();
            
            if (bleRxBuffer.length() > 0) {
                bleDataReceived = true;
                //Serial.printf("Буфер готов к обработке: \"%s\"\n", bleRxBuffer.c_str());
            }
        }
        ////Serial.println("[BLE] -----------------------\n");
    }
};

void initBLEHandler() {
    ////Serial.println("\n--- Инициализация Bluetooth (BLE) ---");

// Create the BLE device with its advertised name.
    BLEDevice::init("LogoBadge");

// Create the BLE server.
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

// Create the Nordic UART service.
    BLEService *pService = pServer->createService(SERVICE_UUID);

// Create the characteristic that sends data to the phone (TX).
    pTxCharacteristic = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_TX,
                          BLECharacteristic::PROPERTY_NOTIFY
                        );
    pTxCharacteristic->addDescriptor(new BLE2902());

// Create the characteristic that receives data from the phone (RX).
// Allow normal and unacknowledged writes.
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                             CHARACTERISTIC_UUID_RX,
                                             BLECharacteristic::PROPERTY_WRITE | 
                                             BLECharacteristic::PROPERTY_WRITE_NR
                                           );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    BLEService *pOtaService = pServer->createService(OTA_SERVICE_UUID);
    BLECharacteristic *pOtaControl = pOtaService->createCharacteristic(
        OTA_CONTROL_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    BLECharacteristic *pOtaData = pOtaService->createCharacteristic(
        OTA_DATA_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    pOtaStatusCharacteristic = pOtaService->createCharacteristic(
        OTA_STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pOtaStatusCharacteristic->addDescriptor(new BLE2902());
    pOtaStatusCharacteristic->setValue("IDLE");
    pOtaControl->setCallbacks(new OtaControlCallbacks());
    pOtaData->setCallbacks(new OtaDataCallbacks());

// Start the service and advertising.
    pService->start();
    pOtaService->start();
    pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
    pServer->getAdvertising()->start();
    
    ////Serial.println("[УСПЕХ] BLE запущен. Имя устройства: \"LogoBadge\"");
}

void sendBLEMessage(const String& message) {
    if (deviceConnected && pTxCharacteristic != nullptr) {
        pTxCharacteristic->setValue(message.c_str());
  pTxCharacteristic->notify(); // Send a notification to the phone.
    }
}

void handleBLECommands() {
    if (otaRebootPending && static_cast<int32_t>(millis() - otaRebootAt) >= 0) {
        ESP.restart();
    }
// Restart advertising after a disconnect.
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); 
        pServer->getAdvertising()->start();
        ////Serial.println("[BLE] Перезапуск рекламы (ожидание подключения)...");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

// Exit until a complete command is available.
    if (!bleDataReceived) return;
    
// Copy the command locally and release the receive buffer immediately.
    String currentCmd = bleRxBuffer;
    bleRxBuffer = ""; 
    bleDataReceived = false; 

    // Use the same parser and command dictionary as the Serial interface.
    sendBLEMessage(executeCommand(currentCmd));
    return;

// Print the recognized command to Serial for debugging.
    //Serial.print("\n[BLE РЕГИСТР] Распознано: \"");
    //Serial.print(currentCmd);
    ////Serial.println("\"");

// Parse the command and argument.
    int spaceIndex = currentCmd.indexOf(' ');
    String cmdPart = (spaceIndex == -1) ? currentCmd : currentCmd.substring(0, spaceIndex);
    String argPart = (spaceIndex == -1) ? "" : currentCmd.substring(spaceIndex + 1);
    cmdPart.trim();
    argPart.trim();

// --- LIST COMMAND ---
    if (cmdPart.equalsIgnoreCase("list")) {
        if (myBinFiles.empty()) {
            sendBLEMessage("LittleFS empty\n");
            return;
        }

        String listResponse = "--- Files ---\n";
        for (size_t i = 0; i < myBinFiles.size(); i++) {
            listResponse += String(i + 1) + ": " + myBinFiles[i] + "\n";
        }
        sendBLEMessage(listResponse);
    } 
// --- SHOW COMMAND ---
    else if (cmdPart.equalsIgnoreCase("show")) {
        if (argPart.length() == 0) {
            sendBLEMessage("0\n"); 
            ////Serial.println("[BLE ОШИБКА] Команда show без аргумента");
            return;
        }

// Check whether the first animation was requested.
        if (argPart.equalsIgnoreCase("anim1")) {
            Clear_Screen_Black(); 
  currentAnimationNum = 1;   // Select the first animation.
            isPlayingAnimation = true; 
            sendBLEMessage("1\n");
            return;
        }

// Check whether the second animation was requested.
        if (argPart.equalsIgnoreCase("anim2")) {
            Clear_Screen_Black(); 
  currentAnimationNum = 2;   // Select the second animation.
            isPlayingAnimation = true; 
            sendBLEMessage("1\n");
            return;
        }

// Stop any animation before drawing a regular file.
        isPlayingAnimation = false;
        currentAnimationNum = 0;

        if (myBinFiles.empty()) {
            sendBLEMessage("0\n");
            ////Serial.println("[BLE ОШИБКА] Карусель пуста");
            return;
        }

        int idx = argPart.toInt();
  int realIdx = idx - 1; // User-facing indexes start at 1.

        if (realIdx >= 0 && (size_t)realIdx < myBinFiles.size()) {
            String targetFile = myBinFiles[realIdx];
            //Serial.printf("[BLE ВЫВОД] Отрисовка: %s... ", targetFile.c_str());

// Display dimensions.
            uint16_t imgW = 360; 
            uint16_t imgH = 360;

            if (Draw_BIN_From_Internal(targetFile.c_str(), 0, 0, imgW, imgH)) {
                ////Serial.println("УСПЕХ");
                sendBLEMessage("1\n"); 
            } else {
                ////Serial.println("ОШИБКА");
                sendBLEMessage("0\n"); 
            }
        } else {
            //Serial.printf("[BLE ОШИБКА] Неверный индекс: %d\n", idx);
            sendBLEMessage("0\n"); 
        }
    } 
// --- UNKNOWN COMMAND ---
    else {
        sendBLEMessage("Unknown. Use 'list' or 'show X'\n");
    }
}
