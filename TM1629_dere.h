#ifndef TM1629_DERE_H
#define TM1629_DERE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

// Handle structure for the TM1629 driver
typedef struct {
    GPIO_TypeDef *STB_Port;
    uint16_t STB_Pin;
    GPIO_TypeDef *CLK_Port;
    uint16_t CLK_Pin;
    GPIO_TypeDef *DIO_Port;
    uint16_t DIO_Pin;
    uint8_t  nPositions; // number of characters/digits
} TM1629_HandleTypeDef;

/**
 * @brief Initializes the TM1629 driver and display.
 * @param h Pointer to a TM1629_HandleTypeDef structure.
 */
void TM1629_Init(TM1629_HandleTypeDef *h);

/**
 * @brief Sets the display brightness and turns it on.
 * @param h Pointer to a TM1629_HandleTypeDef structure.
 * @param brightness Brightness level from 0 (off) to 7 (max).
 */
void TM1629_DisplayOn(TM1629_HandleTypeDef *h, uint8_t brightness);

/**
 * @brief Clears all segments on the display.
 * @param h Pointer to a TM1629_HandleTypeDef structure.
 */
void TM1629_Clear(TM1629_HandleTypeDef *h);

/**
 * @brief Displays a string of text on the display.
 * @param h Pointer to a TM1629_HandleTypeDef structure.
 * @param text The null-terminated string to display.
 */
void TM1629_DisplayText(TM1629_HandleTypeDef *h, const char *text);

/**
 * @brief Lights up all segments on the display for testing.
 * @param h Pointer to a TM1629_HandleTypeDef structure.
 */
void TM1629_AllOn(TM1629_HandleTypeDef *h);


void TM1629_SetRGB(TM1629_HandleTypeDef *htm1629, uint8_t color);
/**
 * @brief Turns off all segments (same as clear).
 * @param h Pointer to a TM1629_HandleTypeDef structure.
 */
void TM1629_AllOff(TM1629_HandleTypeDef *h);

#endif // TM1629_DERE_H
