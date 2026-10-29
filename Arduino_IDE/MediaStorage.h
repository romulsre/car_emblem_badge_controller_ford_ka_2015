#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

#include "TCA9554PWR.h"

// SD/MMC wiring and card utilities.
#define SD_CLK_PIN 14
#define SD_CMD_PIN 17
#define SD_D0_PIN  16

extern uint16_t SDCard_Size;
extern uint16_t Flash_Size;

void SD_Init();
void Flash_test();
bool File_Search(const char* directory, const char* fileName);
uint16_t Folder_retrieval(const char* directory, const char* fileExtension,
                          char fileName[][100], uint16_t maxFiles);

// Image discovery and rendering from SD/MMC or LittleFS.
void findAllImg(const char* directory, std::vector<String>& imageList);
bool Draw_BMP_From_SD(const char* path, uint16_t x, uint16_t y);
bool Draw_BIN_From_SD(const char* path, uint16_t x, uint16_t y,
                      uint16_t imageWidth, uint16_t imageHeight);
bool Draw_BIN_From_Internal(const char* path, uint16_t x, uint16_t y,
                            uint16_t imageWidth, uint16_t imageHeight);
void Clear_Screen_Black();
