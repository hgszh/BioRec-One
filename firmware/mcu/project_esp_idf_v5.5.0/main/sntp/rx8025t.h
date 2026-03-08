#ifndef __RX8025T_H__
#define __RX8025T_H__

#include "esp_err.h"
#include <time.h>

#define RX8025T_SCL 7
#define RX8025T_SDA 6

/* clang-format off */
#define RX8025T_ADDR            0x32
/*-------------------------------------------
 * RX8025T寄存器地址定义
 *-------------------------------------------*/
#define RX8025T_REG_SEC         0x00
#define RX8025T_REG_MIN         0x01
#define RX8025T_REG_HOUR        0x02
#define RX8025T_REG_WEEK        0x03
#define RX8025T_REG_DAY         0x04
#define RX8025T_REG_MONTH       0x05
#define RX8025T_REG_YEAR        0x06
#define RX8025T_REG_RAM         0x07
#define RX8025T_REG_EXT         0x0D
#define RX8025T_REG_FLAG        0x0E
#define RX8025T_REG_CTRL        0x0F
/*-------------------------------------------
 * FLAG寄存器(Rx8025T_REG_FLAG)位定义
 *-------------------------------------------*/
#define RX8025T_FLAG_UF         (1 << 5)
#define RX8025T_FLAG_TF         (1 << 4)
#define RX8025T_FLAG_AF         (1 << 3)
#define RX8025T_FLAG_VLF        (1 << 1)
#define RX8025T_FLAG_VDET       (1 << 0)
/*-------------------------------------------
 * CONTROL寄存器(Rx8025T_REG_CTRL)位定义
 *-------------------------------------------*/
#define RX8025T_CTRL_CSEL1      (1 << 7)
#define RX8025T_CTRL_CSEL0      (1 << 6)
#define RX8025T_CTRL_UIE        (1 << 5)
#define RX8025T_CTRL_TIE        (1 << 4)
#define RX8025T_CTRL_AIE        (1 << 3)
#define RX8025T_CTRL_RESET      (1 << 0)
/*-------------------------------------------
 * EXTENSION寄存器(Rx8025T_REG_EXT)位定义
 *-------------------------------------------*/
#define RX8025T_EXT_TEST        (1 << 7)
#define RX8025T_EXT_WADA        (1 << 6)
#define RX8025T_EXT_USEL        (1 << 5)
#define RX8025T_EXT_TE          (1 << 4)
#define RX8025T_EXT_FSEL1       (1 << 3)
#define RX8025T_EXT_FSEL0       (1 << 2)
#define RX8025T_EXT_TSEL1       (1 << 1)
#define RX8025T_EXT_TSEL0       (1 << 0)
/*-------------------------------------------
 * 默认配置
 *-------------------------------------------*/
#define RX8025T_DEFAULT_CTRL    (RX8025T_CTRL_CSEL0)  
#define RX8025T_DEFAULT_EXT     (0x00)
/* clang-format on */

esp_err_t rx8025t_init(void);
esp_err_t rx8025t_get_time(struct tm *timeinfo);
esp_err_t rx8025t_set_time(const struct tm *timeinfo);

#endif
