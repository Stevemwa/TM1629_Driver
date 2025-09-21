/*
 * TM1629_dere.c
 *
 * Bit-banged TM1629 driver for STM32F4 (Nucleo-F401RE).
 * Uses 2 bytes per digit (15 segments + dp).
 */
#include "TM1629_dere.h"

// This handle must be defined in main.c
extern TIM_HandleTypeDef htim2;

/* --- Private Function Prototypes --- */
static void delay_us(uint32_t us);
static void DIO_AsOutput(TM1629_HandleTypeDef *h);
static void DIO_AsInput(TM1629_HandleTypeDef *h);
static void tm_write_byte(TM1629_HandleTypeDef *h, uint8_t b);
static void send_command(TM1629_HandleTypeDef *h, uint8_t cmd);
static void TM1629_UpdateAll(TM1629_HandleTypeDef *h, const uint16_t *buffer);

/* --- Timing Helper --- */
static void delay_us(uint32_t us) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}

/* --- GPIO Direction Helpers --- */
static void DIO_AsOutput(TM1629_HandleTypeDef *h) {
	GPIO_InitTypeDef gpio = {0};
	gpio.Pin = h->DIO_Pin;
	gpio.Mode = GPIO_MODE_OUTPUT_PP; // Using Push-Pull is also fine
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(h->DIO_Port, &gpio);
}

static void DIO_AsInput(TM1629_HandleTypeDef *h) {
	GPIO_InitTypeDef gpio = {0};
	gpio.Pin = h->DIO_Pin;
	gpio.Mode = GPIO_MODE_INPUT;
	gpio.Pull = GPIO_PULLUP; // Pull-up is good for reading
	HAL_GPIO_Init(h->DIO_Port, &gpio);
}

/* --- Pin Control Macros --- */
#define STB_H(h) HAL_GPIO_WritePin(h->STB_Port, h->STB_Pin, GPIO_PIN_SET)
#define STB_L(h) HAL_GPIO_WritePin(h->STB_Port, h->STB_Pin, GPIO_PIN_RESET)
#define CLK_H(h) HAL_GPIO_WritePin(h->CLK_Port, h->CLK_Pin, GPIO_PIN_SET)
#define CLK_L(h) HAL_GPIO_WritePin(h->CLK_Port, h->CLK_Pin, GPIO_PIN_RESET)
#define DIO_H(h) HAL_GPIO_WritePin(h->DIO_Port, h->DIO_Pin, GPIO_PIN_SET)
#define DIO_L(h) HAL_GPIO_WritePin(h->DIO_Port, h->DIO_Pin, GPIO_PIN_RESET)

/* --- Low-level transfer (LSB-first) --- */
static void tm_write_byte(TM1629_HandleTypeDef *h, uint8_t b) {
	for (uint8_t i = 0; i < 8; ++i) {
		CLK_L(h);
		if (b & 0x01)
			DIO_H(h);
		else
			DIO_L(h);
		delay_us(1);
		CLK_H(h);
		delay_us(1);
		b >>= 1;
	}
}

/* --- TM1629 Command Constants --- */
#define CMD_DATA_WRITE_AUTO_INC 0x40
#define CMD_ADDR_BASE           0xC0
#define CMD_DISPLAY_CTRL_BASE   0x80

/* --- 15-Segment Bit Definitions (Standard Layout) --- */
/* Your physical display wiring MUST match this mapping. */
#define SEG_A   (1u<<0)
#define SEG_B   (1u<<1)
#define SEG_C   (1u<<2)
#define SEG_D   (1u<<3)
#define SEG_E   (1u<<4)
#define SEG_F   (1u<<5)
#define SEG_G1  (1u<<6)  // Top horizontal bar
#define SEG_G2  (1u<<7)  // Bottom horizontal bar
#define SEG_H   (1u<<8)
#define SEG_J   (1u<<9)
#define SEG_K   (1u<<10)
#define SEG_L   (1u<<11)
#define SEG_M   (1u<<12)
#define SEG_N   (1u<<13)
// Segments 14 and 15 are often unused or for DP
#define SEG_DP  (1u<<15)
#define SEG_RGB  (1u<<15)


/* --- Corrected 16-bit Font Table --- */
// IMPORTANT: This table assumes a standard segment mapping.
// If characters are still scrambled, you must find which SEG_x corresponds
// to which physical segment on your display and adjust this table.
static const uint16_t seg15_table[128] = {
    [' '] = 0x0000,
    ['-'] = SEG_G1 | SEG_G2,
    ['_'] = SEG_D,
    ['.'] = SEG_DP,
	['/'] = SEG_K | SEG_N  ,
	[':'] = SEG_D,


    // ========== FIXED DIGITS (using SEG_x macros) ==========
    ['0'] = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_K | SEG_N, // Correct 0
    ['1'] = SEG_B | SEG_C,                                            // Correct 1
    ['2'] = SEG_A | SEG_B | SEG_G1 | SEG_G2 | SEG_E | SEG_D,            // Correct 2
    ['3'] = SEG_A | SEG_B | SEG_G1 | SEG_G2 | SEG_C | SEG_D,            // Correct 3
    ['4'] = SEG_F | SEG_G1 | SEG_G2 | SEG_B | SEG_C,                    // Correct 4
    ['5'] = SEG_A | SEG_F | SEG_G1 | SEG_G2 | SEG_C | SEG_D,            // Correct 5
    ['6'] = SEG_A | SEG_F | SEG_E | SEG_D | SEG_C | SEG_G1 | SEG_G2,    // Correct 6
    ['7'] = SEG_A | SEG_B | SEG_C,                                    // Correct 7
    ['8'] = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G1 | SEG_G2, // Correct 8
    ['9'] = SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G1 | SEG_G2,    // Correct 9

    // Uppercase letters (standard 15-seg approximations)
    ['A'] = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G1 | SEG_G2,
    ['B'] = SEG_A | SEG_B | SEG_C | SEG_D | SEG_H | SEG_M | SEG_G2,
    ['C'] = SEG_A | SEG_D | SEG_E | SEG_F,
    ['D'] = SEG_A | SEG_B | SEG_C | SEG_D | SEG_H |SEG_M,
    ['E'] = SEG_A | SEG_D | SEG_E | SEG_F | SEG_G1|SEG_G2,
    ['F'] = SEG_A | SEG_E | SEG_F | SEG_G1,
    ['G'] = SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G2,
    ['H'] = SEG_B | SEG_C | SEG_E | SEG_F | SEG_G1 | SEG_G2,
    ['I'] =  SEG_H | SEG_M,
    ['J'] = SEG_B | SEG_C | SEG_D | SEG_E,
    ['K'] = SEG_E | SEG_F | SEG_G1 | SEG_K,
    ['L'] = SEG_D | SEG_E | SEG_F,
    ['M'] = SEG_B | SEG_C | SEG_E | SEG_F | SEG_K | SEG_J | SEG_M, // Added top bar (SEG_A) for a different style
    ['N'] = SEG_B | SEG_C | SEG_E | SEG_F | SEG_J | SEG_L,
    ['O'] = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    ['P'] = SEG_A | SEG_B | SEG_E | SEG_F | SEG_G1,
    ['Q'] = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_M,
    ['R'] = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G1 |SEG_G2,
    ['S'] = SEG_A | SEG_F | SEG_G1 | SEG_G2 | SEG_C | SEG_D,
    ['T'] = SEG_A | SEG_H | SEG_M, // Changed central vertical bar to two diagonals
    ['U'] = SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    ['V'] = SEG_E | SEG_F | SEG_J,
    ['W'] = SEG_B | SEG_C | SEG_E | SEG_F | SEG_N | SEG_L,
    ['X'] = SEG_H | SEG_J | SEG_K | SEG_M,
    ['Y'] = SEG_B | SEG_C | SEG_D |SEG_F | SEG_G1 | SEG_G2,
    ['Z'] = SEG_A | SEG_D | SEG_J,
};

/* --- Low-level Helpers --- */
static void send_command(TM1629_HandleTypeDef *h, uint8_t cmd) {
	STB_L(h);
	tm_write_byte(h, cmd);
	STB_H(h);
}

/* --- Public API Functions --- */

void TM1629_Init(TM1629_HandleTypeDef *h) {
    // Ensure pins are in a known state
	STB_H(h);
	CLK_H(h);
	DIO_AsOutput(h);
	DIO_H(h);
	delay_us(10);

    // ========== CLEANED UP INITIALIZATION SEQUENCE ==========
    // 1. Set display control: Turn ON, set brightness (e.g., 4/7)
	TM1629_DisplayOn(h, 4);

    // 2. Set data command: Write mode, auto-increment address
    send_command(h, CMD_DATA_WRITE_AUTO_INC);

    // 3. Clear the display memory
	TM1629_Clear(h);
}

void TM1629_DisplayOn(TM1629_HandleTypeDef *h, uint8_t brightness) {
	if (brightness > 7) brightness = 7;
	uint8_t cmd = CMD_DISPLAY_CTRL_BASE | 0x08 | brightness; // 0x08 bit turns display ON
	send_command(h, cmd);
}

void TM1629_Clear(TM1629_HandleTypeDef *h) {
	STB_L(h);
	tm_write_byte(h, CMD_ADDR_BASE); // Start at address 0
	for (uint8_t i = 0; i < (h->nPositions * 2); ++i) {
		tm_write_byte(h, 0x00); // Write 16 zero-bytes (8 positions * 2 bytes/pos)
	}
	STB_H(h);
}

static void TM1629_UpdateAll(TM1629_HandleTypeDef *h, const uint16_t *buffer) {
	STB_L(h);
	tm_write_byte(h, CMD_ADDR_BASE); // Start at address 0
	for (uint8_t i = 0; i < h->nPositions; ++i) {
		uint16_t v = buffer[i];
		tm_write_byte(h, (uint8_t)(v & 0xFF));      // Low byte
		tm_write_byte(h, (uint8_t)((v >> 8) & 0xFF)); // High byte
	}
	STB_H(h);
}

void TM1629_DisplayText(TM1629_HandleTypeDef *h, const char *text) {
	uint16_t buf[16] = {0}; // Buffer is safe for up to 16 digits
	for (uint8_t i = 0; i < h->nPositions; ++i) {
		char c = text[i];
		if (c == '\0') {
            // Fill rest of the buffer with spaces if string is short
            for(uint8_t j = i; j < h->nPositions; ++j) {
                buf[j] = seg15_table[' '];
            }
			break;
		}
        // Look up character in the font table
		if ((uint8_t)c < 128)
			buf[i] = seg15_table[(uint8_t)c];
		else
			buf[i] = 0; // Display blank for unknown characters
	}
    // Set the data command again in case it was changed
    send_command(h, CMD_DATA_WRITE_AUTO_INC);
	TM1629_UpdateAll(h, buf);
}

/* --- Quick Test Helpers --- */
void TM1629_AllOn(TM1629_HandleTypeDef *h) {
    send_command(h, CMD_DATA_WRITE_AUTO_INC);
	STB_L(h);
	tm_write_byte(h, CMD_ADDR_BASE);
	for (int i = 0; i < (h->nPositions * 2); ++i) {
		tm_write_byte(h, 0xFF);
	}
	STB_H(h);
    TM1629_DisplayOn(h, 7); // Turn on max brightness to be sure
}

void TM1629_SetRGB(TM1629_HandleTypeDef *htm1629, uint8_t color) {
    uint16_t buf[16] = {0}; // buffer for all positions (max 8 positions * 2 bytes)

    // Segment 16 / bit 15 controls the RGB LED
    switch (color) {
        case 1: // Green -> Grid 1
            buf[0] = SEG_RGB; // Use bit 15 (SEG_DP / SEG16)
            break;
        case 2: // Red -> Grid 2
            buf[1] = SEG_RGB;
            break;
        case 3: // Blue -> Grid 3
            buf[2] = SEG_RGB;
            break;
        default:
            // Turn off all RGB LEDs
            for(int i=0;i<8;i++) buf[i]=0;
            break;
    }

    // Write buffer directly to the display
    TM1629_UpdateAll(htm1629, buf);
}

void TM1629_AllOff(TM1629_HandleTypeDef *h) {
	TM1629_Clear(h);
}


