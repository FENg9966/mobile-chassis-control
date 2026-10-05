
#ifndef MAIN_BOARD_H_
#define MAIN_BOARD_H_

/* LCD SPI引脚 (SPI2_HOST，避开Octal PSRAM占用的GPIO26~32) */
#define LCD_HOST            SPI2_HOST
#define LCD_PIN_SCK         12
#define LCD_PIN_MOSI        11
#define LCD_PIN_MISO        13
#define LCD_PIN_CS          10
#define LCD_PIN_DC          9
#define LCD_PIN_RST         14
#define LCD_PIN_BL          21

/* LCD背光点亮电平 */
#define LCD_BK_LIGHT_ON_LEVEL   1
#define LCD_BK_LIGHT_OFF_LEVEL  0

/* LCD参数 */
#define LCD_H_RES           320
#define LCD_V_RES           480
#define LCD_PIXEL_CLOCK_HZ  (20 * 1000 * 1000)

/* FT6336U 电容触摸 (I2C) */
#define I2C_HOST            I2C_NUM_0
#define I2C_PIN_SCL         40
#define I2C_PIN_SDA         41
#define TP_PIN_RST          42
#define TP_PIN_INT          39
#define FT6336_I2C_ADDR     0x38

#endif
