#include "commands_handler.h"
#include "MediaStorage.h"
#include "Arduino.h"  

extern std::vector<String> myBinFiles;
// Image-list scan function implemented by the main sketch.
extern void findAllImgInternal(const char* dirName, std::vector<String>& filesVector);
extern bool startEyeAnimation(const String& colorArgument);
extern void stopCurrentAnimation();

// Converts a textual command to its command type.
CommandType parseCommand(const String& cmdStr) {
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (cmdStr.equalsIgnoreCase(COMMAND_DICTIONARY[i].cmdString)) {
            return COMMAND_DICTIONARY[i].type;
        }
    }
    return CommandType::CMD_UNKNOWN;
}

// A same-sized file can still be a different image; compare its contents.
static bool filesAreEqual(File& source, File& destination) {
    if (!source || !destination || source.size() != destination.size()) return false;
    uint8_t sourceBuffer[512], destinationBuffer[512];
    while (source.available()) {
        size_t count = source.read(sourceBuffer, sizeof(sourceBuffer));
        if (destination.read(destinationBuffer, count) != count ||
            memcmp(sourceBuffer, destinationBuffer, count) != 0) return false;
        yield();
    }
    return !destination.available();
}

static bool copyOpenedFile(File& source, const String& destinationPath) {
    if (INTERNAL_FS.exists(destinationPath)) INTERNAL_FS.remove(destinationPath);
    File destination = INTERNAL_FS.open(destinationPath, FILE_WRITE);
    if (!source || !destination) {
        if (destination) destination.close();
        return false;
    }
    uint8_t buffer[1024];
    size_t count;
    while ((count = source.read(buffer, sizeof(buffer))) > 0) {
        if (destination.write(buffer, count) != count) {
            destination.close();
            return false;
        }
        yield();
    }
    destination.close();
    return true;
}

static void syncDirectory(const char* sdPath, const char* internalPath) {
    if (SD_MMC.cardType() == CARD_NONE || !SD_MMC.exists(sdPath)) return;
    if (!INTERNAL_FS.exists(internalPath) && !INTERNAL_FS.mkdir(internalPath)) return;
    File directory = SD_MMC.open(sdPath);
    if (!directory || !directory.isDirectory()) return;
    File source = directory.openNextFile();
    while (source) {
        if (!source.isDirectory()) {
            String sourceName = source.name();
            int slash = sourceName.lastIndexOf('/');
            String destinationPath = String(internalPath) + "/" + (slash >= 0 ? sourceName.substring(slash + 1) : sourceName);
            bool same = false;
            if (INTERNAL_FS.exists(destinationPath)) {
                File destination = INTERNAL_FS.open(destinationPath, FILE_READ);
                same = filesAreEqual(source, destination);
                if (destination) destination.close();
                source.seek(0);
            }
            if (!same) copyOpenedFile(source, destinationPath);
        }
        source.close();
        source = directory.openNextFile();
    }
    directory.close();
}

/** Copies the first animation directory from SD/MMC to LittleFS at startup. */
void copyAnimationFromSD() {
    syncDirectory("/anim1", "/anim1");
}

/** Copies the second animation directory from SD/MMC to LittleFS at startup. */
void copyAnimation2FromSD() {
    syncDirectory("/anim2", "/anim2");
}

void initCommandsHandler() {
    ////Serial.println("\n--- Инициализация системы хранения данных ---");
    
    // Initialize internal storage.
    if (!INTERNAL_FS.begin(false)) { 
        ////Serial.println("[ИНФО] LittleFS не найдена, форматируем...");
        INTERNAL_FS.format();
        INTERNAL_FS.begin();
    }
    
    // Initialize the SD/MMC card.
    SD_Init();
    
    // Check the initialization result.
    if (SD_MMC.cardType() == CARD_NONE) {
        ////Serial.println("[ОШИБКА] SD-карта не инициализирована.");
    } else {
        ////Serial.println("[УСПЕХ] SD-карта инициализирована.");
        // Copy animations first.
        copyAnimationFromSD();
        copyAnimation2FromSD();
        // Then check /logos for new carousel files.
        checkNewFilesOnSD();
    }
}

String executeCommand(const String& rawInput) {
    String input = rawInput;
    input.trim();
    int space = input.indexOf(' ');
    String command = space < 0 ? input : input.substring(0, space);
    String argument = space < 0 ? "" : input.substring(space + 1);
    command.trim();
    argument.trim();
    if (command.equalsIgnoreCase("list")) command = "ls_int"; // legacy BLE name

    switch (parseCommand(command)) {
        case CommandType::CMD_HELP: {
            String response = "Commands:\n";
            for (size_t i = 0; i < COMMAND_COUNT; ++i) response += String(COMMAND_DICTIONARY[i].cmdString) + "\n";
            return response;
        }
        case CommandType::CMD_LIST_SD: {
            if (SD_MMC.cardType() == CARD_NONE) return "ERROR: SD card unavailable\n";
            std::vector<String> files;
            findAllImg("/logos", files);
            String response = "SD /logos:\n";
            for (const String& file : files) response += file + "\n";
            return response;
        }
        case CommandType::CMD_LIST_INTERNAL: {
            String response = "LittleFS:\n";
            for (size_t i = 0; i < myBinFiles.size(); ++i) response += String(i + 1) + ": " + myBinFiles[i] + "\n";
            return response;
        }
        case CommandType::CMD_COPY_FILE:
            if (argument.length() == 0) return "ERROR: use copy <filename>\n";
            if (argument.startsWith("/")) argument = argument.substring(1);
            return copyFileToInternal(argument.c_str()) ? "OK\n" : "ERROR: copy failed\n";
        case CommandType::CMD_DELETE_FILE:
            if (argument.length() == 0) return "ERROR: use delete <filename>\n";
            if (argument.startsWith("/")) argument = argument.substring(1);
            return deleteInternalFile(argument.c_str()) ? "OK\n" : "ERROR: delete failed\n";
        case CommandType::CMD_SYNC_ALL:
            performSync();
            return "Sync complete\n";
        case CommandType::CMD_MEM_INFO:
            return "LittleFS: " + String(INTERNAL_FS.usedBytes()) + "/" + String(INTERNAL_FS.totalBytes()) + " bytes\n";
        case CommandType::CMD_SHOW_IMAGE: {
            int separator = argument.indexOf(' ');
            String animationName = separator == -1 ? argument : argument.substring(0, separator);
            String animationArgument = separator == -1 ? "" : argument.substring(separator + 1);
            animationArgument.trim();

            if (animationName.equalsIgnoreCase("anim3")) {
                return startEyeAnimation(animationArgument) ? "OK\n" : "ERROR: use show anim3 <#RRGGBB|RGB565>\n";
            }

            if (animationName.equalsIgnoreCase("anim1") || animationName.equalsIgnoreCase("anim2")) {
                extern bool isPlayingAnimation;
                extern int currentAnimationNum;
                stopCurrentAnimation();
                Clear_Screen_Black();
                currentAnimationNum = animationName.equalsIgnoreCase("anim1") ? 1 : 2;
                isPlayingAnimation = true;
                return "OK\n";
            }
            if (argument.length() == 0 || myBinFiles.empty()) return "ERROR: no image selected\n";
            String target;
            bool numeric = true;
            for (size_t i = 0; i < argument.length(); ++i) if (!isDigit(argument[i])) numeric = false;
            if (numeric) {
                int index = argument.toInt() - 1;
                if (index < 0 || static_cast<size_t>(index) >= myBinFiles.size()) return "ERROR: invalid image index\n";
                target = myBinFiles[index];
            } else {
                if (!argument.startsWith("/")) argument = "/" + argument;
                for (const String& path : myBinFiles) if (path.equalsIgnoreCase(argument)) { target = path; break; }
                if (!target.length()) return "ERROR: file not found\n";
            }
            stopCurrentAnimation();
            return Draw_BIN_From_Internal(target.c_str(), 0, 0, 360, 360) ? "OK\n" : "ERROR: draw failed\n";
        }
        case CommandType::CMD_RST: {
            delay(300);
            ESP.restart();
        }
        case CommandType::CMD_CLR_SCR: {
            Clear_Screen_Black();
            return "OK\n";
        }
        case CommandType::CMD_GET_SN: {
            uint64_t chipId = ESP.getEfuseMac();
            char sn[32];
            // Format: SN-V1-A1B2C3D4E5F6.
            snprintf(sn, sizeof(sn), "SN-V1-%04X%08X", (uint16_t)(chipId >> 32), (uint32_t)chipId);
            return "SN:\n" + String(sn) + "\n";
        }
        default:
            return "ERROR: unknown command. Use help\n";
    }
}

void handleSerialCommands() {
    if (!Serial.available()) return;
    
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
    int spaceIndex = input.indexOf(' ');
    String cmdPart = (spaceIndex == -1) ? input : input.substring(0, spaceIndex);
    String argPart = (spaceIndex == -1) ? "" : input.substring(spaceIndex + 1);
    argPart.trim();
    
    CommandType type = parseCommand(cmdPart);
    //Serial.print("\n> Получена команда: ");
    ////Serial.println(input);
    
    switch (type) {
        case CommandType::CMD_HELP: {
            Serial.println("\n--- Список доступных команд ---");
            for (size_t i = 0; i < COMMAND_COUNT; i++) {
                Serial.printf("  %-10s : %s\n", COMMAND_DICTIONARY[i].cmdString, COMMAND_DICTIONARY[i].description);
            }
            break;
        }
        case CommandType::CMD_LIST_SD:
            if (SD_MMC.cardType() == CARD_NONE) {
                ////Serial.println("[ОШИБКА] SD-карта отсутствует.");
            } else {
                ////Serial.println("\n--- Содержимое папки /logos на SD-карте ---");
                listFiles(SD_MMC, "/logos");
            }
            break;
            
        case CommandType::CMD_LIST_INTERNAL:
            ////Serial.println("\n--- Содержимое внутренней памяти (LittleFS) ---");
            listFiles(INTERNAL_FS, "/");
            break;
            
        case CommandType::CMD_COPY_FILE:
            if (argPart.length() == 0) {
                ////Serial.println("[ОШИБКА] Укажите файл. Пример: copy pic.bin");
            } else {
                if (argPart.startsWith("/")) argPart = argPart.substring(1);
                copyFileToInternal(argPart.c_str());
            }
            break;
            
        case CommandType::CMD_DELETE_FILE:
            if (argPart.length() == 0) {
                ////Serial.println("[ОШИБКА] Укажите файл. Пример: delete pic.bin");
            } else {
                if (argPart.startsWith("/")) argPart = argPart.substring(1);
                deleteInternalFile(argPart.c_str());
            }
            break;
            
        case CommandType::CMD_SYNC_ALL:
            performSync();
            break;
            
        case CommandType::CMD_MEM_INFO:
            printMemoryInfo();
            break;
        
        case CommandType::CMD_SHOW_IMAGE: {
            if (argPart.length() == 0) {
                ////Serial.println("[ОШИБКА] Укажите индекс или имя файла. Пример: show 2 или show s2.bin");
                break;
            }

            // Keep serial behaviour identical to BLE for the procedural eye.
            if (argPart.startsWith("anim3 ") || argPart.equalsIgnoreCase("anim3")) {
                Serial.print(executeCommand(input));
                break;
            }

            if (myBinFiles.empty()) {
                ////Serial.println("[ОШИБКА] Список файлов карусели пуст. Нечего отображать.");
                break;
            }

            String targetFile = "";

            // Determine whether the user supplied an index or file name.
            bool isIndex = true;
            for (size_t i = 0; i < argPart.length(); i++) {
                if (!isDigit(argPart[i])) {
                    isIndex = false;
                    break;
                }
            }

            if (isIndex) {
                int idx = argPart.toInt();
                int realIdx = idx - 1; 

                if (realIdx >= 0 && (size_t)realIdx < myBinFiles.size()) {
                    targetFile = myBinFiles[realIdx];
                } else {
                    //Serial.printf("[ОШИБКА] Неверный индекс: %d. Всего картинок в карусели: %d\n", idx, myBinFiles.size());
                    break;
                }
            } else {
                String searchName = argPart;
                if (!searchName.startsWith("/")) searchName = "/" + searchName;

                for (const String& path : myBinFiles) {
                    if (path.equalsIgnoreCase(searchName)) {
                        targetFile = path;
                        break;
                    }
                }

                if (targetFile.length() == 0) {
                    //Serial.printf("[ОШИБКА] Файл %s не найден в текущей карусели.\n", searchName.c_str());
                    break;
                }
            }

            // Stop animation so it cannot overwrite the static image.
            stopCurrentAnimation();

            //Serial.printf("[ВЫВОД] Отрисовка файла: %s... ", targetFile.c_str());
            
            uint16_t imgW = 360; 
            uint16_t imgH = 360;
            
            if (Draw_BIN_From_Internal(targetFile.c_str(), 0, 0, imgW, imgH)) {
                ////Serial.println("УСПЕХ");
            } else {
                ////Serial.println("ОШИБКА ОТРИСОВКИ");
            }
            break;
        }
            
        default:
            ////Serial.println("[ОШИБКА] Неизвестная команда. Введите 'help'.");
            break;
    }
}

// Compare files automatically without copying them.
void checkNewFilesOnSD() {
    if (SD_MMC.cardType() == CARD_NONE) return;
    
    std::vector<String> sdFiles;
    findAllImg("/logos", sdFiles); 
    
    if (sdFiles.empty()) {
        ////Serial.println("[АВТО] В папке /logos на SD-карте нет .bin файлов.");
        return;
    }
    
    int newFilesCount = 0;
    ////Serial.println("\n[АВТО] Проверка наличия новых файлов на SD...");
    
    for (const String& sdPath : sdFiles) {
        int lastSlash = sdPath.lastIndexOf('/');
        String name = (lastSlash == -1) ? sdPath : sdPath.substring(lastSlash + 1);
        
        String internalPath = "/" + name; 
        
        if (!INTERNAL_FS.exists(internalPath)) {
            //Serial.printf("  + Обнаружен новый файл на SD: %s\n", sdPath.c_str());
            newFilesCount++;
        }
    }
    
    if (newFilesCount > 0) {
        //Serial.printf("[АВТО] Найдено новых файлов для переноса: %d. Отправьте 'sync'.\n", newFilesCount);
    } else {
        ////Serial.println("[АВТО] Все файлы на SD уже скопированы в корень LittleFS.");
    }
}

// Actively synchronize files for the "sync" command.
void performSync() {
    if (SD_MMC.cardType() == CARD_NONE) {
        ////Serial.println("[СИНХРОНИЗАЦИЯ] Ошибка: SD-карта отсутствует.");
        return;
    }
    
    std::vector<String> sdFiles;
    findAllImg("/logos", sdFiles); 
    
    if (sdFiles.empty()) {
        Serial.println("[СИНХРОНИЗАЦИЯ] На SD-карте в /logos нечего копировать.");
        return;
    }
    
    Serial.printf("\n[СИНХРОНИЗАЦИЯ] Найдено файлов на SD: %d. Запуск...\n", sdFiles.size());
    int syncedCount = 0;
    int errorCount = 0;
    
    for (const String& sdPath : sdFiles) {
        int lastSlash = sdPath.lastIndexOf('/');
        String name = (lastSlash == -1) ? sdPath : sdPath.substring(lastSlash + 1);
        String internalPath = "/" + name;
        
        bool needCopy = false;
        if (!INTERNAL_FS.exists(internalPath)) {
            needCopy = true; 
        } else {
            File srcFile = SD_MMC.open(sdPath);
            File intFile = INTERNAL_FS.open(internalPath, FILE_READ);
            if (srcFile && intFile) {
                if (srcFile.size() != intFile.size()) needCopy = true;
            }
            if (srcFile) srcFile.close();
            if (intFile) intFile.close();
        }
        
        if (needCopy) {
            //Serial.printf("[СИНХРОНИЗАЦИЯ] %s -> корень LittleFS... ", name.c_str());
            File src = SD_MMC.open(sdPath, FILE_READ);
            File dst = INTERNAL_FS.open(internalPath, FILE_WRITE);
            
            if (src && dst) {
                uint8_t buf[512];
                size_t len;
                while ((len = src.read(buf, sizeof(buf))) > 0) {
                    dst.write(buf, len);
                    yield(); 
                }
                dst.close(); src.close();
                ////Serial.println("УСПЕХ");
                syncedCount++;
            } else {
                if (src) src.close();
                if (dst) dst.close();
                ////Serial.println("ОШИБКА");
                errorCount++;
            }
        }
    }
    
    // Refresh the carousel image list after synchronization.
    findAllImgInternal("/", myBinFiles);
    
    //Serial.printf("[СИНХРОНИЗАЦИЯ] Завершено. Успешно: %d, Ошибок: %d. Список карусели обновлен.\n", syncedCount, errorCount);
}

// Copy one file manually.
bool copyFileToInternal(const char* filename) {
    if (SD_MMC.cardType() == CARD_NONE) return false;
    
    String sdPath = "/logos/" + String(filename);
    String internalPath = "/" + String(filename);
    
    if (!SD_MMC.exists(sdPath)) {
        //Serial.printf("[КОПИРОВАНИЕ] Файл %s не найден в /logos на SD.\n", filename);
        return false;
    }
    
    File src = SD_MMC.open(sdPath, FILE_READ);
    File dst = INTERNAL_FS.open(internalPath, FILE_WRITE);
    if (!src || !dst) {
        if (src) src.close();
        if (dst) dst.close();
        ////Serial.println("[КОПИРОВАНИЕ] Ошибка открытия файлов.");
        return false;
    }
    
    //Serial.printf("[КОПИРОВАНИЕ] %s (%d байт) -> LittleFS... ", filename, src.size());
    uint8_t buf[512];
    size_t len;
    while ((len = src.read(buf, sizeof(buf))) > 0) {
        dst.write(buf, len);
        yield();
    }
    dst.close(); src.close();
    ////Serial.println("ГОТОВО");
    
    // Refresh the carousel list after a manual copy.
    findAllImgInternal("/", myBinFiles);
    
    return true;
}

bool deleteInternalFile(const char* filename) {
    String internalPath = "/" + String(filename);
    if (!INTERNAL_FS.exists(internalPath)) {
        //Serial.printf("[УДАЛЕНИЕ] Файл %s не найден в LittleFS.\n", filename);
        return false;
    }
    
    if (INTERNAL_FS.remove(internalPath)) {
        //Serial.printf("[УДАЛЕНИЕ] Файл %s успешно удален.\n", filename);
        
        // Refresh the carousel list after deletion.
        findAllImgInternal("/", myBinFiles);
        
        return true;
    } else {
        //Serial.printf("[УДАЛЕНИЕ] Ошибка при удалении файла %s.\n", filename);
        return false;
    }
}

void printMemoryInfo() {
    ////Serial.println("\n--- Статус памяти ---");
    size_t total = INTERNAL_FS.totalBytes();
    size_t used = INTERNAL_FS.usedBytes();
    //Serial.printf("  LittleFS Всего:    %.2f МБ\n", total / 1024.0 / 1024.0);
    //Serial.printf("  LittleFS Занято:   %.2f МБ\n", used / 1024.0 / 1024.0);
    //Serial.printf("  LittleFS Свободно: %.2f МБ\n", (total - used) / 1024.0 / 1024.0);
}

void listFiles(FS &fs, const char* dirName, int numSpaces) {
    File root = fs.open(dirName);
    if (!root || !root.isDirectory()) return;
    
    File file = root.openNextFile();
    while (file) {
        for (int i = 0; i < numSpaces; i++) //Serial.print(" ");
        if (file.isDirectory()) {
            //Serial.printf("[%s]\n", file.name());
            listFiles(fs, file.path(), numSpaces + 2);
        } else {
            //Serial.printf("  - %s  (%d байт)\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
    root.close();
}
