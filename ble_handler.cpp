#include "ble_handler.h"
#include "commands_handler.h"
#include "IMG_SD.h" // Твоя библиотека отрисовки
#include <Update.h>

// Даем знать компилятору, что вектор картинок лежит в основном .ino файле
extern std::vector<String> myBinFiles; 
// Даем знать компилятору о флаге запуска анимации из main.ino
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

// Колбэки для отслеживания подключения/отключения телефона
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

// Управление OTA: "START <password> <firmware_size>", "END" или "ABORT".
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

// Принимает сырые байты .bin. Не преобразовывать их в String: прошивка может
// содержать нулевые байты.
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

// Колбэк для приема данных от телефона (RX)
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        ////Serial.println("\n[BLE] --- ВХОДЯЩИЙ ПАКЕТ ---");
        
        // 1. Получаем данные сразу в Arduino String
        String rxValue = pCharacteristic->getValue();
        //Serial.printf("Длина пакета: %d байт\n", rxValue.length());
        
        // 2. Выводим каждый байт в HEX для анализа скрытых символов
        //Serial.print("Байты (HEX): ");
        for (size_t i = 0; i < rxValue.length(); i++) {
            //Serial.printf("0x%02X ", (uint8_t)rxValue[i]);
        }
        ////Serial.println();

        // 3. Выводим как текст
        //Serial.print("Как текст: \"");
        //Serial.print(rxValue);
        ////Serial.println("\"");
        
        if (rxValue.length() > 0) {
            bleRxBuffer += rxValue;
            
            // Убираем мусор и пробелы
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

    // Создаем BLE устройство с твоим именем
    BLEDevice::init("LogoBadge");

    // Создаем BLE Сервер
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Создаем сервис Nordic UART
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Создаем характеристику для ОТПРАВКИ данных на телефон (TX)
    pTxCharacteristic = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_TX,
                          BLECharacteristic::PROPERTY_NOTIFY
                        );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // Создаем характеристику для ПРИЕМА данных с телефона (RX)
    // Разрешаем обычную запись и запись без подтверждения (WRITE + WRITE_NO_RESPONSE)
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

    // Стартуем сервис и рекламу (advertising)
    pService->start();
    pOtaService->start();
    pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
    pServer->getAdvertising()->start();
    
    ////Serial.println("[УСПЕХ] BLE запущен. Имя устройства: \"LogoBadge\"");
}

void sendBLEMessage(const String& message) {
    if (deviceConnected && pTxCharacteristic != nullptr) {
        pTxCharacteristic->setValue(message.c_str());
        pTxCharacteristic->notify(); // Отправляем пуш-уведомление на телефон
    }
}

void handleBLECommands() {
    if (otaRebootPending && static_cast<int32_t>(millis() - otaRebootAt) >= 0) {
        ESP.restart();
    }
    // Автоматический рестарт рекламы при обрыве связи
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); 
        pServer->getAdvertising()->start();
        ////Serial.println("[BLE] Перезапуск рекламы (ожидание подключения)...");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    // Если команда еще не зафиксирована — выходим
    if (!bleDataReceived) return;
    
    // Как только зашли сюда, сразу копируем команду в локальную переменную и освобождаем буфер
    String currentCmd = bleRxBuffer;
    bleRxBuffer = ""; 
    bleDataReceived = false; 

    // Use the same parser and command dictionary as the Serial interface.
    sendBLEMessage(executeCommand(currentCmd));
    return;

    // Выводим в Serial для дебага — теперь ты точно увидишь, ЧТО прислал телефон
    //Serial.print("\n[BLE РЕГИСТР] Распознано: \"");
    //Serial.print(currentCmd);
    ////Serial.println("\"");

    // Парсим команду и аргумент
    int spaceIndex = currentCmd.indexOf(' ');
    String cmdPart = (spaceIndex == -1) ? currentCmd : currentCmd.substring(0, spaceIndex);
    String argPart = (spaceIndex == -1) ? "" : currentCmd.substring(spaceIndex + 1);
    cmdPart.trim();
    argPart.trim();

    // --- КОМАНДА: LIST ---
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
    // --- КОМАНДА: SHOW ---
    else if (cmdPart.equalsIgnoreCase("show")) {
        if (argPart.length() == 0) {
            sendBLEMessage("0\n"); 
            ////Serial.println("[BLE ОШИБКА] Команда show без аргумента");
            return;
        }

        // ПРОВЕРКА: Если запросили ПЕРВУЮ анимацию
        if (argPart.equalsIgnoreCase("anim1")) {
            Clear_Screen_Black(); 
            currentAnimationNum = 1;   // Выбираем первую анимацию
            isPlayingAnimation = true; 
            sendBLEMessage("1\n");
            return;
        }

        // ПРОВЕРКА: Если запросили ВТОРУЮ анимацию
        if (argPart.equalsIgnoreCase("anim2")) {
            Clear_Screen_Black(); 
            currentAnimationNum = 2;   // Выбираем вторую анимацию
            isPlayingAnimation = true; 
            sendBLEMessage("1\n");
            return;
        }

        // Если запросили обычный файл, то останавливаем любую анимацию
        isPlayingAnimation = false;
        currentAnimationNum = 0;

        if (myBinFiles.empty()) {
            sendBLEMessage("0\n");
            ////Serial.println("[BLE ОШИБКА] Карусель пуста");
            return;
        }

        int idx = argPart.toInt();
        int realIdx = idx - 1; // Человеческий отсчет с 1

        if (realIdx >= 0 && (size_t)realIdx < myBinFiles.size()) {
            String targetFile = myBinFiles[realIdx];
            //Serial.printf("[BLE ВЫВОД] Отрисовка: %s... ", targetFile.c_str());

            // размеры экрана
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
    // --- НЕИЗВЕСТНАЯ КОМАНДА ---
    else {
        sendBLEMessage("Unknown. Use 'list' or 'show X'\n");
    }
}
