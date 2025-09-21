# TM1629 Bit-Banged Driver for STM32

![STM32 Logo](https://upload.wikimedia.org/wikipedia/commons/3/3a/STM32_logo.png)

A lightweight, bit-banged **TM1629 LED driver library** for STM32 MCUs (tested on Nucleo-F401RE).  
Supports **8-character 15-segment displays** and an integrated **RGB LED**. Written in C, easy to integrate in STM32Cube projects.

---

## Features

- Control up to **8 alphanumeric characters** on a 15-segment LED display  
- Supports **floating-point value display** without `printf` float support  
- Built-in animation examples for **scrolling text** and **FW values**  
- Control an **RGB LED** connected to segment 16 (Grid 1, 2, or 3)  
- Full **clear**, **all-on**, and **brightness control** functions  
- Pure software (bit-banged) — no hardware SPI required  

---

## Installation

1. Copy the following files to your STM32Cube project:

TM1629_dere.h
TM1629_dere.c

cpp
Copy code

2. Include the header in your `main.c`:

```c
#include "TM1629_dere.h"
Ensure htim2 timer is initialized in microseconds mode for delays:

c
Copy code
HAL_TIM_Base_Start(&htim2);
Usage Example
c
Copy code
#include "TM1629_dere.h"

TM1629_HandleTypeDef htm1629 = {
    .STB_Port = GPIOB, .STB_Pin = GPIO_PIN_6,
    .CLK_Port = GPIOA, .CLK_Pin = GPIO_PIN_6,
    .DIO_Port = GPIOA, .DIO_Pin = GPIO_PIN_7,
    .nPositions = 8
};

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();

    HAL_TIM_Base_Start(&htim2);
    TM1629_Init(&htm1629);

    // Display simple text
    TM1629_DisplayText(&htm1629, "HELLO  ");
    HAL_Delay(2000);

    // Scroll float values
    fw_values_animation(&htm1629, 2.56, 11.0, 300);

    // Animate SUNCULTURE
    sunculture_animation(250);

    // Set RGB LED
    TM1629_SetRGB(&htm1629, 1); // Grid 1 = Green
    HAL_Delay(1000);
    TM1629_SetRGB(&htm1629, 2); // Grid 2 = Red
    HAL_Delay(1000);
    TM1629_SetRGB(&htm1629, 3); // Grid 3 = Blue
    HAL_Delay(1000);

    while(1)
    {
        // Loop your animations
    }
}
API Functions
Function	Description
TM1629_Init(&htm1629)	Initialize the display and set default brightness.
TM1629_DisplayText(&htm1629, "TEXT")	Display an 8-character string.
TM1629_Clear(&htm1629)	Clear the entire display.
TM1629_AllOn(&htm1629)	Turn all segments on for testing.
TM1629_AllOff(&htm1629)	Turn all segments off.
TM1629_SetRGB(&htm1629, color)	Control the RGB LED: 1=Green, 2=Red, 3=Blue.
fw_values_animation(&htm1629, val1, val2, delay_ms)	Scroll FW values like FW:2.56/11.0.
sunculture_animation(delay)	Animate "SUNCULTURE" across the display.

Notes
The RGB LED is controlled via the 16th segment (bit 15).

All animations are blocking using HAL_Delay(). Non-blocking versions can be implemented with timers.

The segment mapping may vary; adjust seg15_table[] for your display if characters appear incorrect.
