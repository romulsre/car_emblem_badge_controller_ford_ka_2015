#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <vector>

// Стандартные UUID для Nordic UART Service (NUS)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Отдельный сервис обновления прошивки. Бинарные блоки передаются только в
// OTA_DATA, поэтому они не попадают в текстовый обработчик команд NUS.
#define OTA_SERVICE_UUID        "D4A00000-8A7B-4B9B-9D6A-5B2B4E5F0001"
#define OTA_CONTROL_UUID        "D4A00000-8A7B-4B9B-9D6A-5B2B4E5F0002"
#define OTA_DATA_UUID           "D4A00000-8A7B-4B9B-9D6A-5B2B4E5F0003"
#define OTA_STATUS_UUID         "D4A00000-8A7B-4B9B-9D6A-5B2B4E5F0004"
#define OTA_PASSWORD            "0000"

// BLE OTA protocol:
//   1. Write ASCII "START 0000 <size_in_bytes>" to OTA_CONTROL_UUID.
//   2. Write the exported firmware .bin as raw chunks to OTA_DATA_UUID.
//   3. Write ASCII "END" to OTA_CONTROL_UUID and wait for "OK rebooting"
//      on OTA_STATUS_UUID. "ABORT" cancels an unfinished update.

// Инициализация BLE
void initBLEHandler();

// Проверка входящих команд по BLE (вызывается в loop)
void handleBLECommands();

// Функция отправки ответа обратно клиенту (1, 0 или список файлов)
void sendBLEMessage(const String& message);
