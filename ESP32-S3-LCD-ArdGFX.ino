#include <Arduino.h>
#include <vector>

// Подключаем наши новые файлы управления памятью и командами
#include "commands_handler.h"

// Стандартные библиотеки для периферии платы
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "Display_ST77916.h"
#include "SD_Card.h"
#include "IMG_SD.h"
//#include "Touch_CST816.h"
#include "ble_handler.h"
#include "eye_animation.h"

// Переменные для управления анимацией
bool isPlayingAnimation = false;
int currentAnimationNum = 0; // 0 - нет, 1 - anim1, 2 - anim2
int animCurrentFrame = 1;           // Номер текущего кадра (1-6)
unsigned long lastFrameTime = 0;    // Таймер последнего кадра
const unsigned long frameDelay = 80; // Задержка между кадрами (80мс ~ 12.5 FPS)

// Размеры BIN картинок фиксированные (360x360)
#define IMG_WIDTH  360 
#define IMG_HEIGHT 360

// Динамический массив для путей к файлам во ВНУТРЕННЕЙ памяти
std::vector<String> myBinFiles;

static bool isEyeAnimationPlaying = false;

static bool parseEyeColor(const String& colorArgument, uint16_t& color) {
  String value = colorArgument;
  value.trim();
  if (value.startsWith("#")) {
    if (value.length() != 7) return false;
    String hex = value.substring(1);
    char *end = nullptr;
    unsigned long rgb = strtoul(hex.c_str(), &end, 16);
    if (*end != '\0' || rgb > 0xFFFFFFUL) return false;
    color = ((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F);
    return true;
  }
  if (!value.length()) return false;
  char *end = nullptr;
  unsigned long rgb565 = strtoul(value.c_str(), &end, 0);
  if (*end != '\0') {
    end = nullptr;
    rgb565 = strtoul(value.c_str(), &end, 16);  // also accept bare values such as F800
  }
  if (*end != '\0' || rgb565 > 0xFFFFUL) return false;
  color = static_cast<uint16_t>(rgb565);
  return true;
}

void stopCurrentAnimation() {
  isPlayingAnimation = false;
  currentAnimationNum = 0;
  animCurrentFrame = 1;
  if (isEyeAnimationPlaying) {
    eye_animation_end();
    isEyeAnimationPlaying = false;
  }
}

bool startEyeAnimation(const String& colorArgument) {
  uint16_t color;
  if (!parseEyeColor(colorArgument, color)) return false;
  stopCurrentAnimation();
  if (!panel_handle || !eye_animation_begin(panel_handle, color)) return false;
  isEyeAnimationPlaying = true;
  return true;
}

// Индекс текущей картинки в массиве
size_t currentImgIdx = 0;

/**
 * @brief Функция сканирования внутренней памяти (LittleFS)
 */
void findAllImgInternal(const char* dirName, std::vector<String>& filesVector) {
  filesVector.clear();
  File root = INTERNAL_FS.open(dirName);
  if (!root || !root.isDirectory()) {
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = file.name();
      if (name.endsWith(".bin") || name.endsWith(".BIN") || name.endsWith(".bmp")) {
        if (!name.startsWith("/logos")) {
          name = "/" + name;
        }
        filesVector.push_back(name);
      }
    }
    file = root.openNextFile();
  }
}

/**
 * @brief Неблокирующий проигрыватель анимации
 */
void runAnimationPlayer() {
    if (isEyeAnimationPlaying) {
        eye_animation_update(panel_handle, millis());
        return;
    }
    if (!isPlayingAnimation) return;

    if (millis() - lastFrameTime >= frameDelay) {
        lastFrameTime = millis();

        // Переменные параметров кадра (будут настроены под конкретную анимацию)
        String framePath = "";
        uint16_t imgW = 350;
        uint16_t imgH = 217;
        uint16_t maxFrames = 6;

        // Настраиваем параметры в зависимости от выбранной анимации
        if (currentAnimationNum == 2) {
            // Анимация №2: 350x207, 10 кадров
            framePath = "/anim2/a2-" + String(animCurrentFrame) + ".bin";
            imgW = 350;
            imgH = 207;
            maxFrames = 10;
        } else {
            // Анимация №1: 350x217, 6 кадров
            framePath = "/anim1/a1-" + String(animCurrentFrame) + ".bin";
            imgW = 350;
            imgH = 217;
            maxFrames = 6;
        }
        
        // Смещение для вывода ровно по центру круглого экрана (360x360):
        // Формула: start = (360 - размер) / 2
        uint16_t startX = (360 - imgW) / 2; // Для 350 это будет 5
        uint16_t startY = (360 - imgH) / 2; // Для 217 -> ~71, для 207 -> ~76

        // Рисуем текущий кадр строго по центру
        Draw_BIN_From_Internal(framePath.c_str(), startX, startY, imgW, imgH);

        animCurrentFrame++;

        // Сброс и зацикливание на основе максимального количества кадров текущей анимации
        if (animCurrentFrame > maxFrames) {
            animCurrentFrame = 1; 
        }
    }
}

void setup()
{
  //Serial.begin(115200);
  delay(100);
  // Инициализация I2C шины и расширителя портов питания
  I2C_Init();
  TCA9554PWR_Init(0x00);
  Set_EXIO(EXIO_PIN1, High);
  Set_EXIO(EXIO_PIN2, High);

  // 1. Инициализация дисплея
  Backlight_Init();
  LCD_Init();
  Set_Backlight(100);

  // 2. Инициализация тач-скрина CST816
  //Touch_Init();
  //delay(500);

  // 3. Инициализация файловой системы и обработчика команд
  initCommandsHandler();
  delay(200); 

  // 4. Сканируем файлы в LittleFS (делаем это ОДИН раз при запуске)
  findAllImgInternal("/", myBinFiles);

  // 5. Выводим стартовую картинку
  if (!myBinFiles.empty()) {
    Draw_BIN_From_Internal(myBinFiles[currentImgIdx].c_str(), 0, 0, IMG_WIDTH, IMG_HEIGHT);
  }

  // Холостой опрос тача для очистки буфера чипа
  /*
  for (int i = 0; i < 5; i++) {
    Touch_Loop();
    delay(10);
  }
  touch_data.gesture = NONE;
  */

  // Инициализация BLE
  initBLEHandler();
}

void loop()
{
  // 1. Опрашиваем интерфейсы связи
  handleSerialCommands();
  handleBLECommands();

  // 2. Опрашиваем тач-скрин
  //Touch_Loop(); 

  // 3. Обработка нажатия на экран
  /*
  if (touch_data.gesture == SINGLE_CLICK) {
    touch_data.gesture = NONE; // Сбрасываем жест сразу же

    // Если крутится анимация, любой тап по экрану останавливает её и возвращает статику
    if (isPlayingAnimation) {
        isPlayingAnimation = false;
        currentAnimationNum = 0;
        animCurrentFrame = 1;
        Clear_Screen_Black(); // Стираем остатки анимации перед выводом картинки
    }

    if (!myBinFiles.empty()) {
      currentImgIdx++;
      if (currentImgIdx >= myBinFiles.size()) {
        currentImgIdx = 0;
      }

      //Draw_BIN_From_SD(myBinFiles[currentImgIdx].c_str(), 0, 0, IMG_WIDTH, IMG_HEIGHT);
      Draw_BIN_From_Internal(myBinFiles[currentImgIdx].c_str(), 0, 0, IMG_WIDTH, IMG_HEIGHT);
      
      // Защита от дребезга контактов тача
      delay(50); 
      int timeout = 0;
      while (touch_data.points > 0 && timeout < 50) { 
        Touch_Loop(); 
        delay(10);    
        timeout++;    
      }
      touch_data.gesture = NONE; 
    }
  }
  */

  // 4. Обновляем анимацию (если она активна)
  runAnimationPlayer(); 

  // Микро-пауза для стабильности планировщика ESP32 (не блокирует поток)
  delay(2); 
}
