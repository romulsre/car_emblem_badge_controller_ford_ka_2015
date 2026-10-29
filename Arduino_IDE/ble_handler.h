#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <vector>

// Standard UUIDs for the Nordic UART Service (NUS).
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Dedicated firmware-update service. Binary chunks use OTA_DATA only, so they
// cannot reach the NUS text-command handler.
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

// Initialize BLE.
void initBLEHandler();

// Process incoming BLE commands; call from loop().
void handleBLECommands();

// Send a response back to the client.
void sendBLEMessage(const String& message);
