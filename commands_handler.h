#ifndef COMMANDS_HANDLER_H
#define COMMANDS_HANDLER_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <LittleFS.h>

// Internal storage filesystem.
#define INTERNAL_FS LittleFS

enum class CommandType {
    CMD_UNKNOWN = 0,
    CMD_HELP,
    CMD_LIST_SD,
    CMD_LIST_INTERNAL,
    CMD_COPY_FILE,
    CMD_DELETE_FILE,
    CMD_SYNC_ALL,
    CMD_SHOW_IMAGE,
    CMD_MEM_INFO,
    CMD_RST,
    CMD_CLR_SCR,
    CMD_GET_SN
};

struct CommandMapping {
    const char* cmdString;
    CommandType type;
    const char* description;
};

// Available command catalogue.
const CommandMapping COMMAND_DICTIONARY[] = {
    {"help",      CommandType::CMD_HELP,          "List all available commands"},
    {"ls_sd",     CommandType::CMD_LIST_SD,       "List files on the SD card"},
    {"ls_int",    CommandType::CMD_LIST_INTERNAL, "List files in internal storage (LittleFS)"},
    {"copy",      CommandType::CMD_COPY_FILE,     "Copy a file from SD to internal storage: copy <filename>"},
    {"delete",    CommandType::CMD_DELETE_FILE,   "Delete a file from internal storage: delete <filename>"},
    {"sync",      CommandType::CMD_SYNC_ALL,      "Synchronize new files from SD to internal storage"},
    {"show",      CommandType::CMD_SHOW_IMAGE,    "Show an image, anim1/anim2, or eye: show anim3 <#RRGGBB|RGB565>"},
    {"mem",       CommandType::CMD_MEM_INFO,      "Show storage usage for SD and internal storage"},
    {"reset",     CommandType::CMD_RST,           "Restart the controller"},
    {"clear",     CommandType::CMD_CLR_SCR,       "Clear the display"},
    {"get_sn",    CommandType::CMD_GET_SN,        "Get the device serial number"}
};

const size_t COMMAND_COUNT = sizeof(COMMAND_DICTIONARY) / sizeof(COMMAND_DICTIONARY[0]);

// Copy animations from SD/MMC to LittleFS during startup.
void copyAnimationFromSD();
void copyAnimation2FromSD();

// Check SD/MMC for updates without copying them.
void checkNewFilesOnSD();

// Synchronize files when the "sync" command is received.
void performSync();

// Copy one file.
bool copyFileToInternal(const char* filename);

// Delete a file from internal storage.
bool deleteInternalFile(const char* filename);

// Print storage usage.
void printMemoryInfo();

// List a directory recursively.
void listFiles(FS &fs, const char* dirName, int numSpaces = 0);

// Initialize and poll the serial interface.
void initCommandsHandler();
void handleSerialCommands();
String executeCommand(const String& input);

#endif // COMMANDS_HANDLER_H
