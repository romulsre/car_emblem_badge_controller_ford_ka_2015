#include "MediaStorage.h"
#include "FS.h"
#include "SD_MMC.h"
#include "Display_ST77916.h"
#include <vector>

#define IMG_BLOCK_ROWS 60
static uint16_t block[EXAMPLE_LCD_WIDTH * IMG_BLOCK_ROWS];

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t read16(File &f)
{
  uint16_t v;
  ((uint8_t *)&v)[0] = f.read();
  ((uint8_t *)&v)[1] = f.read();
  return v;
}

static uint32_t read32(File &f)
{
  uint32_t v;
  ((uint8_t *)&v)[0] = f.read();
  ((uint8_t *)&v)[1] = f.read();
  ((uint8_t *)&v)[2] = f.read();
  ((uint8_t *)&v)[3] = f.read();
  return v;
}

// Collects all supported image paths from the selected directory.
void findAllImg(const char *directory, std::vector<String> &imgList)
{
  File root = SD_MMC.open(directory);
  if (!root || !root.isDirectory()) {
    //Serial.println("Failed to open directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    String path = file.path();

    if (!file.isDirectory()) {
      String lower = path;
      lower.toLowerCase();

      if (lower.endsWith(".bin")) {//".bmp"
        imgList.push_back(path); // Add the path to the result list.
      }
    }
    file = root.openNextFile();
  }
  root.close();
}

bool Draw_BMP_From_SD(const char *path, uint16_t x, uint16_t y)
{
  if (x >= EXAMPLE_LCD_WIDTH || y >= EXAMPLE_LCD_HEIGHT) return false;
  File bmp = SD_MMC.open(path, FILE_READ);
  if (!bmp) {
    //Serial.println("BMP open failed");
    return false;
  }

  if (read16(bmp) != 0x4D42) {
    //Serial.println("Not a BMP file");
    bmp.close();
    return false;
  }

  (void)read32(bmp);
  (void)read32(bmp);

  uint32_t imageOffset = read32(bmp);
  uint32_t headerSize = read32(bmp);
  int32_t bmpWidth = (int32_t)read32(bmp);
  int32_t bmpHeight = (int32_t)read32(bmp);

  if (read16(bmp) != 1) {
    //Serial.println("Invalid BMP planes");
    bmp.close();
    return false;
  }

  uint16_t depth = read16(bmp);
  uint32_t compression = read32(bmp);

  if (headerSize < 40 || compression != 0 || depth != 24) {
    //Serial.println("Only uncompressed 24-bit BMP is supported");
    bmp.close();
    return false;
  }

  bool flip = true;
  if (bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip = false;
  }

  uint16_t drawW = min((int32_t)(EXAMPLE_LCD_WIDTH - x), bmpWidth);
  uint16_t drawH = min((int32_t)(EXAMPLE_LCD_HEIGHT - y), bmpHeight);

  uint32_t rowSize = ((bmpWidth * 3 + 3) & ~3);

  for (uint16_t blockY = 0; blockY < drawH; blockY += IMG_BLOCK_ROWS) {
    uint16_t rowsThisBlock = IMG_BLOCK_ROWS;

    if (blockY + rowsThisBlock > drawH) {
      rowsThisBlock = drawH - blockY;
    }

    for (uint16_t blockRow = 0; blockRow < rowsThisBlock; blockRow++) {
      uint16_t screenRow = blockY + blockRow;
      uint32_t bmpRow = flip ? (bmpHeight - 1 - screenRow) : screenRow;
      uint32_t pos = imageOffset + bmpRow * rowSize;

      bmp.seek(pos);

      for (uint16_t col = 0; col < drawW; col++) {
        uint8_t b = bmp.read();
        uint8_t g = bmp.read();
        uint8_t r = bmp.read();

        block[(uint32_t)blockRow * drawW + col] = rgb565(r, g, b);
      }
    }

    LCD_addWindow(
      x,
      y + blockY,
      x + drawW - 1,
      y + blockY + rowsThisBlock - 1,
      block
    );
  }

  bmp.close();
  return true;
}

// Renders a raw RGB565 image from an already-open storage file.
static bool drawBin(File& binFile, uint16_t x, uint16_t y, uint16_t imageWidth, uint16_t imageHeight) {
  if (x >= EXAMPLE_LCD_WIDTH || y >= EXAMPLE_LCD_HEIGHT) return false;

  const uint16_t drawWidth = min(EXAMPLE_LCD_WIDTH - x, imageWidth);
  const uint16_t drawHeight = min(EXAMPLE_LCD_HEIGHT - y, imageHeight);
  const size_t rowSizeInBytes = static_cast<size_t>(drawWidth) * sizeof(uint16_t);

  for (uint16_t blockY = 0; blockY < drawHeight; blockY += IMG_BLOCK_ROWS) {
    const uint16_t rowsRemaining = drawHeight - blockY;
    const uint16_t rowsThisBlock = rowsRemaining < IMG_BLOCK_ROWS ? rowsRemaining : IMG_BLOCK_ROWS;
    for (uint16_t blockRow = 0; blockRow < rowsThisBlock; ++blockRow) {
      uint16_t* row = &block[static_cast<size_t>(blockRow) * drawWidth];
      if (binFile.read(reinterpret_cast<uint8_t*>(row), rowSizeInBytes) != rowSizeInBytes) return false;
    }
    LCD_addWindow(x, y + blockY, x + drawWidth - 1,
                  y + blockY + rowsThisBlock - 1, block);
  }
  return true;
}

bool Draw_BIN_From_SD(const char* path, uint16_t x, uint16_t y, uint16_t imageWidth, uint16_t imageHeight) {
  File binFile = SD_MMC.open(path, FILE_READ);
  if (!binFile) return false;
  const bool drawn = drawBin(binFile, x, y, imageWidth, imageHeight);
  binFile.close();
  return drawn;
}

bool Draw_BIN_From_Internal(const char* path, uint16_t x, uint16_t y, uint16_t imageWidth, uint16_t imageHeight) {
  File binFile = LittleFS.open(path, FILE_READ);
  if (!binFile) {
    Serial.printf("Could not open BIN from LittleFS: %s\n", path);
    return false;
  }
  const bool drawn = drawBin(binFile, x, y, imageWidth, imageHeight);
  binFile.close();
  return drawn;
}

/**
 * @brief Quickly clears the screen to black using the shared block buffer.
 */
void Clear_Screen_Black() {
    // Fill the complete block buffer with black pixels.
    uint32_t totalPixelsInBlock = (uint32_t)EXAMPLE_LCD_WIDTH * IMG_BLOCK_ROWS;
    memset(block, 0, totalPixelsInBlock * sizeof(uint16_t));

    // Paint the screen from top to bottom in blocks.
    for (uint16_t blockY = 0; blockY < EXAMPLE_LCD_HEIGHT; blockY += IMG_BLOCK_ROWS) {
        uint16_t rowsThisBlock = IMG_BLOCK_ROWS;

        // Clamp the final block to the display boundary.
        if (blockY + rowsThisBlock > EXAMPLE_LCD_HEIGHT) {
            rowsThisBlock = EXAMPLE_LCD_HEIGHT - blockY;
        }

        // Send the black block to the display.
        LCD_addWindow(
            0,                          // x_start
            blockY,                     // y_start
            EXAMPLE_LCD_WIDTH - 1,      // x_end
            blockY + rowsThisBlock - 1, // y_end
            block                       // Pointer to the cleared buffer.
        );
    }
}
