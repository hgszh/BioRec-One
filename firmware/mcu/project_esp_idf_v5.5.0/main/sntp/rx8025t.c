#include "rx8025t.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "rx8025t";

static i2c_master_dev_handle_t i2c_dev_handle;
static esp_err_t i2c_master_init(void) {
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = RX8025T_SDA,
        .scl_io_num = RX8025T_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        return ret;
    }
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RX8025T_ADDR,
        .scl_speed_hz = 400000,
    };
    ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &i2c_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static esp_err_t rx8025t_register_read(uint8_t reg_addr, uint8_t *data, size_t len) {
    if (data == NULL || len == 0) return ESP_ERR_INVALID_ARG;
    return i2c_master_transmit_receive(i2c_dev_handle, &reg_addr, 1, data, len, 100);
}

static esp_err_t rx8025t_register_write(uint8_t reg_addr, const uint8_t *data, size_t len) {
    if (data == NULL || len == 0) return ESP_ERR_INVALID_ARG;
    uint8_t buf[1 + len];
    buf[0] = reg_addr;
    memcpy(&buf[1], data, len);
    return i2c_master_transmit(i2c_dev_handle, buf, sizeof(buf), 100);
}

esp_err_t rx8025t_init(void) {
    esp_err_t ret;
    uint8_t flag_reg = 0;
    uint8_t ctrl_reg = RX8025T_DEFAULT_CTRL;
    uint8_t ext_reg = RX8025T_DEFAULT_EXT;
    ret = i2c_master_init();
    if (ret != ESP_OK) return ret;
    ret = rx8025t_register_read(RX8025T_REG_FLAG, &flag_reg, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read FLAG register");
        return ret;
    }
    if (flag_reg & RX8025T_FLAG_VLF) {
        ESP_LOGW(TAG, "VLF=1, RTC lost power. Reinitializing...");
        flag_reg = 0x00;
        ret = rx8025t_register_write(RX8025T_REG_FLAG, &flag_reg, 1);
        if (ret != ESP_OK) return ret;
        ret = rx8025t_register_write(RX8025T_REG_EXT, &ext_reg, 1);
        if (ret != ESP_OK) return ret;
        ret = rx8025t_register_write(RX8025T_REG_CTRL, &ctrl_reg, 1);
        if (ret != ESP_OK) return ret;
        ESP_LOGI(TAG, "RX8025T reinitialized, please set time manually.");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "RX8025T time data valid, initialization complete.");
    return ESP_OK;
}

static inline uint8_t bin_to_bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
static inline uint8_t bcd_to_bin(uint8_t val) { return (val & 0x0F) + ((val >> 4) * 10); }

esp_err_t rx8025t_get_time(struct tm *timeinfo) {
    if (timeinfo == NULL) return ESP_ERR_INVALID_ARG;
    uint8_t data[7];
    esp_err_t ret = rx8025t_register_read(RX8025T_REG_SEC, data, sizeof(data));
    if (ret != ESP_OK) return ret;
    timeinfo->tm_sec = bcd_to_bin(data[0] & 0x7F); // bit7=CH禁止位
    timeinfo->tm_min = bcd_to_bin(data[1] & 0x7F);
    timeinfo->tm_hour = bcd_to_bin(data[2] & 0x3F);    // 24小时制
    timeinfo->tm_wday = __builtin_ctz(data[3] & 0x7F); // 星期掩码转索引(0=周日)
    timeinfo->tm_mday = bcd_to_bin(data[4] & 0x3F);
    timeinfo->tm_mon = bcd_to_bin(data[5] & 0x1F) - 1; // struct tm中月份是0~11
    timeinfo->tm_year = bcd_to_bin(data[6]) + 100;     // tm_year = 年份 - 1900 (2000→100)
    return ESP_OK;
}

esp_err_t rx8025t_set_time(const struct tm *timeinfo) {
    if (timeinfo == NULL) return ESP_ERR_INVALID_ARG;
    uint8_t data[7];
    data[0] = bin_to_bcd(timeinfo->tm_sec) & 0x7F; // 秒，确保CH=0
    data[1] = bin_to_bcd(timeinfo->tm_min);
    data[2] = bin_to_bcd(timeinfo->tm_hour);
    data[3] = 1 << (timeinfo->tm_wday % 7); // 星期掩码形式
    data[4] = bin_to_bcd(timeinfo->tm_mday);
    data[5] = bin_to_bcd(timeinfo->tm_mon + 1);
    data[6] = bin_to_bcd((timeinfo->tm_year + 1900) % 100); // 年份后两位
    return rx8025t_register_write(RX8025T_REG_SEC, data, sizeof(data));
}
