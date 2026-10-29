#include <Arduino.h>
#include <vector>

// Command and storage management.
#include "commands_handler.h"

// Board peripheral drivers.
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "Display_ST77916.h"
#include "MediaStorage.h"
//#include "Touch_CST816.h"
#include "ble_handler.h"
#include "eye_animation.h"

// Animation state.
bool isPlayingAnimation = false;
int currentAnimationNum = 0; // 0: none, 1: anim1, 2: anim2
int animCurrentFrame = 1;           // Current frame number.
unsigned long lastFrameTime = 0;    // Timestamp of the previous frame.
const unsigned long frameDelay = 80; // Frame delay (about 12.5 FPS).

// Fixed BIN image dimensions.
#define IMG_WIDTH  360 
#define IMG_HEIGHT 360

// Paths of images in internal storage.
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

// Index of the current image.
size_t currentImgIdx = 0;

/**
 * @brief Scan internal storage (LittleFS).
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
 * @brief Non-blocking animation player.
 */
void runAnimationPlayer() {
    if (isEyeAnimationPlaying) {
        eye_animation_update(panel_handle, millis());
        return;
    }
    if (!isPlayingAnimation) return;

    if (millis() - lastFrameTime >= frameDelay) {
        lastFrameTime = millis();

        // Frame parameters are selected for the active animation.
        String framePath = "";
        uint16_t imgW = 350;
        uint16_t imgH = 217;
        uint16_t maxFrames = 6;

        // Configure parameters for the selected animation.
        if (currentAnimationNum == 2) {
            // Animation 2: 350x207, 10 frames.
            framePath = "/anim2/a2-" + String(animCurrentFrame) + ".bin";
            imgW = 350;
            imgH = 207;
            maxFrames = 10;
        } else {
            // Animation 1: 350x217, 6 frames.
            framePath = "/anim1/a1-" + String(animCurrentFrame) + ".bin";
            imgW = 350;
            imgH = 217;
            maxFrames = 6;
        }
        
        // Center the frame on the 360x360 circular display.
        uint16_t startX = (360 - imgW) / 2;
        uint16_t startY = (360 - imgH) / 2;

        // Draw the current frame at the center.
        Draw_BIN_From_Internal(framePath.c_str(), startX, startY, imgW, imgH);

        animCurrentFrame++;

        // Loop when the last frame has been rendered.
        if (animCurrentFrame > maxFrames) {
            animCurrentFrame = 1; 
        }
    }
}

void setup()
{
  //Serial.begin(115200);
  delay(100);
  // Initialize the I2C bus and power GPIO expander.
  I2C_Init();
  TCA9554PWR_Init(0x00);
  Set_EXIO(EXIO_PIN1, High);
  Set_EXIO(EXIO_PIN2, High);

  // Initialize the display.
  Backlight_Init();
  LCD_Init();
  Set_Backlight(100);

  // Initialize the CST816 touch controller.
  //Touch_Init();
  //delay(500);

  // Initialize storage and the command handler.
  initCommandsHandler();
  delay(200); 

  // Scan LittleFS once at startup.
  findAllImgInternal("/", myBinFiles);

  // Draw the startup image.
  if (!myBinFiles.empty()) {
    Draw_BIN_From_Internal(myBinFiles[currentImgIdx].c_str(), 0, 0, IMG_WIDTH, IMG_HEIGHT);
  }

  // Poll touch events to clear the controller buffer.
  /*
  for (int i = 0; i < 5; i++) {
    Touch_Loop();
    delay(10);
  }
  touch_data.gesture = NONE;
  */

  // Initialize BLE.
  initBLEHandler();
}

void loop()
{
  // Poll communication interfaces.
  handleSerialCommands();
  handleBLECommands();

  // Poll the touch controller.
  //Touch_Loop(); 

  // Process touch input.
  /*
  if (touch_data.gesture == SINGLE_CLICK) {
    touch_data.gesture = NONE; // Clear the gesture immediately.

    // Any touch stops an active animation and returns to static content.
    if (isPlayingAnimation) {
        isPlayingAnimation = false;
        currentAnimationNum = 0;
        animCurrentFrame = 1;
        Clear_Screen_Black(); // Clear residual animation pixels before drawing.
    }

    if (!myBinFiles.empty()) {
      currentImgIdx++;
      if (currentImgIdx >= myBinFiles.size()) {
        currentImgIdx = 0;
      }

      //Draw_BIN_From_SD(myBinFiles[currentImgIdx].c_str(), 0, 0, IMG_WIDTH, IMG_HEIGHT);
      Draw_BIN_From_Internal(myBinFiles[currentImgIdx].c_str(), 0, 0, IMG_WIDTH, IMG_HEIGHT);
      
      // Basic touch debounce.
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

  // Update the active animation.
  runAnimationPlayer(); 

  // Brief yield for ESP32 scheduler stability.
  delay(2); 
}
