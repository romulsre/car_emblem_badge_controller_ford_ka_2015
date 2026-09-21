#ifndef COMMANDS_HANDLER_H
#define COMMANDS_HANDLER_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <LittleFS.h>

// Определение используемой файловой системы во внутренней памяти
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

// Словарь доступных команд
const CommandMapping COMMAND_DICTIONARY[] = {
    {"help",      CommandType::CMD_HELP,          "Вывод списка всех доступных команд"},
    {"ls_sd",     CommandType::CMD_LIST_SD,       "Вывести список файлов на SD-карте"},
    {"ls_int",    CommandType::CMD_LIST_INTERNAL, "Вывести список файлов во внутренней памяти (LittleFS)"},
    {"copy",      CommandType::CMD_COPY_FILE,     "Копировать файл с SD во внутреннюю память. Формат: copy <filename>"},
    {"delete",    CommandType::CMD_DELETE_FILE,   "Удалить файл из внутренней памяти. Формат: delete <filename>"},
    {"sync",      CommandType::CMD_SYNC_ALL,      "Синхронизировать все новые файлы с SD во внутреннюю память"},
    {"show",      CommandType::CMD_SHOW_IMAGE,    "Показать картинку, anim1/anim2 или глаз: show anim3 <#RRGGBB|RGB565>"},
    {"mem",       CommandType::CMD_MEM_INFO,      "Показать информацию о состоянии памяти (SD и Internal)"},
    {"reset",     CommandType::CMD_RST,           "Перезагрузка контроллера"},
    {"clear",     CommandType::CMD_CLR_SCR,       "Полная очистка экрана"},
    {"get_sn",    CommandType::CMD_GET_SN,        "Получить серийный номер прибора"}
};

const size_t COMMAND_COUNT = sizeof(COMMAND_DICTIONARY) / sizeof(COMMAND_DICTIONARY[0]);

// Автоматическое копирование анимации /anim1 с SD-карты в LittleFS при старте
void copyAnimationFromSD();
void copyAnimation2FromSD();

// Автоматическая проверка наличия обновлений на SD (сверяет, но НЕ копирует сама)
void checkNewFilesOnSD();

// Синхронизация по команде ("sync")
void performSync();

// Копирование одиночного файла
bool copyFileToInternal(const char* filename);

// Удаление файла из внутренней памяти
bool deleteInternalFile(const char* filename);

// Статус заполнения памяти
void printMemoryInfo();

// Универсальный листинг директорий
void listFiles(FS &fs, const char* dirName, int numSpaces = 0);

// Инициализация и поллинг Serial-порта
void initCommandsHandler();
void handleSerialCommands();
String executeCommand(const String& input);

#endif // COMMANDS_HANDLER_H
