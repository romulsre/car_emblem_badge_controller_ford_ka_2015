#ifndef IMG_SD_H
#define IMG_SD_H

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>

/**
 * @brief Ищет все BMP-файлы в указанной директории и добавляет их пути в вектор.
 * * @param directory Путь к папке (например, "/")
 * @param ImgList Ссылка на вектор строк, куда будут записаны пути к файлам
 */
void findAllImg(const char *directory, std::vector<String> &imgList);

/**
 * @brief Декодирует и выводит 24-битный BMP-файл с SD-карты на дисплей.
 * * @param path Полный путь к BMP-файлу
 * @param x Начальная координата X на экране
 * @param y Начальная координата Y на экране
 * @return true Если файл успешно отрисован
 * @return false Если произошла ошибка (файл не найден, неверный формат и т.д.)
 */
bool Draw_BMP_From_SD(const char *path, uint16_t x, uint16_t y);
bool Draw_BIN_From_SD(const char *path, uint16_t x, uint16_t y, uint16_t imgW, uint16_t imgH);

bool Draw_BIN_From_Internal(const char *path, uint16_t x, uint16_t y, uint16_t imgW, uint16_t imgH);

void Clear_Screen_Black();

#endif // IMG_SD_H