/*
    WS2812B FlipperZero driver
    Copyright (C) 2022  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "WS2812B.h"
#include <furi_hal.h>
#include <storage/storage.h>

/* Настройки */
#define RGB_BACKLIGHT_LEDS 3 //Количество светодиодов на плате подсветки
#define RGB_BACKLIGHT_LED_PIN &led_pin //Порт подключения светодиодов
#define RGB_BACKLIGHT_SETTINGS_VERSION 5
#define RGB_BACKLIGHT_SETTINGS_FILE_NAME ".rgb_backlight.settings"
#define RGB_BACKLIGHT_SETTINGS_PATH EXT_PATH(RGB_BACKLIGHT_SETTINGS_FILE_NAME)

#define TAG "RGB Backlight"

#ifdef FURI_DEBUG
#define DEBUG_PIN &gpio_ext_pa7
#define DEBUG_INIT() \
    furi_hal_gpio_init(DEBUG_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh)
#define DEBUG_SET_HIGH() furi_hal_gpio_write(DEBUG_PIN, true)
#define DEBUG_SET_LOW() furi_hal_gpio_write(DEBUG_PIN, false)
#else
#define DEBUG_INIT()
#define DEBUG_SET_HIGH()
#define DEBUG_SET_LOW()
#endif
static uint8_t RGB_BACKLIGHT_ledbuffer[RGB_BACKLIGHT_LEDS][3];
static const GpioPin led_pin = {.port = GPIOA, .pin = LL_GPIO_PIN_8};

static RGBBacklightSettings rgb_settings = {
    .version = RGB_BACKLIGHT_SETTINGS_VERSION,
    .display_color_index = 0,
    .settings_is_loaded = false};

#define COLOR_COUNT (sizeof(colors) / sizeof(WS2812B_Color))

const WS2812B_Color colors[] = {
    {"Orange", 255, 79, 0},
    {"Yellow", 255, 170, 0},
    {"Spring", 167, 255, 0},
    {"Lime", 0, 255, 0},
    {"Aqua", 0, 255, 127},
    {"Cyan", 0, 210, 210},
    {"Azure", 0, 127, 255},
    {"Blue", 0, 0, 255},
    {"Purple", 127, 0, 255},
    {"Magenta", 210, 0, 210},
    {"Pink", 255, 0, 127},
    {"Red", 255, 0, 0},
    {"White", 140, 140, 140},
};

static void _port_init(void) {
    DEBUG_INIT();
    furi_hal_gpio_write(RGB_BACKLIGHT_LED_PIN, false);
    furi_hal_gpio_init(
        RGB_BACKLIGHT_LED_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
}

void WS2812B_send(void) {
    _port_init();
    furi_kernel_lock();
    uint32_t end;
    /* Последовательная отправка цветов светодиодов */
    for(uint8_t lednumber = 0; lednumber < RGB_BACKLIGHT_LEDS; lednumber++) {
        //Последовательная отправка цветов светодиода
        for(uint8_t color = 0; color < 3; color++) {
            //Последовательная отправка битов цвета
            uint8_t i = 0b10000000;
            while(i != 0) {
                if(RGB_BACKLIGHT_ledbuffer[lednumber][color] & (i)) {
                    furi_hal_gpio_write(RGB_BACKLIGHT_LED_PIN, true);
                    DEBUG_SET_HIGH();
                    end = DWT->CYCCNT + 30;
                    //T1H 600 us (615 us)
                    while(DWT->CYCCNT < end) {
                    }
                    furi_hal_gpio_write(RGB_BACKLIGHT_LED_PIN, false);
                    DEBUG_SET_LOW();
                    end = DWT->CYCCNT + 26;
                    //T1L  600 us (587 us)
                    while(DWT->CYCCNT < end) {
                    }
                } else {
                    furi_hal_gpio_write(RGB_BACKLIGHT_LED_PIN, true);
                    DEBUG_SET_HIGH();
                    end = DWT->CYCCNT + 11;
                    //T0H 300 ns (312 ns)
                    while(DWT->CYCCNT < end) {
                    }
                    furi_hal_gpio_write(RGB_BACKLIGHT_LED_PIN, false);
                    DEBUG_SET_LOW();
                    end = DWT->CYCCNT + 43;
                    //T0L 900 ns (890 ns)
                    while(DWT->CYCCNT < end) {
                    }
                }
                i >>= 1;
            }
        }
    }
    furi_kernel_unlock();
}

uint8_t rgb_backlight_get_color_count(void) {
    return COLOR_COUNT;
}

const char* rgb_backlight_get_color_text(uint8_t index) {
    return colors[index].name;
}

static void rgb_backlight_load_settings(void) {
    FuriHalRtcBootMode bm = furi_hal_rtc_get_boot_mode();
    if(bm == FuriHalRtcBootModeDfu) {
        rgb_settings.settings_is_loaded = true;
        return;
    }

    RGBBacklightSettings settings;
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    const size_t settings_size = sizeof(RGBBacklightSettings);

    FURI_LOG_I(TAG, "loading settings from \"%s\"", RGB_BACKLIGHT_SETTINGS_PATH);
    bool fs_result =
        storage_file_open(file, RGB_BACKLIGHT_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING);

    if(fs_result) {
        uint16_t bytes_count = storage_file_read(file, &settings, settings_size);

        if(bytes_count != settings_size) {
            fs_result = false;
        }
    }

    if(fs_result) {
        FURI_LOG_I(TAG, "load success");
        if(settings.version != RGB_BACKLIGHT_SETTINGS_VERSION) {
            FURI_LOG_E(
                TAG,
                "version(%d != %d) mismatch",
                settings.version,
                RGB_BACKLIGHT_SETTINGS_VERSION);
        } else {
            memcpy(&rgb_settings, &settings, settings_size);
        }
    } else {
        FURI_LOG_E(TAG, "load failed, %s", storage_file_get_error_desc(file));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    rgb_settings.settings_is_loaded = true;
};

void rgb_backlight_save_settings(void) {
    RGBBacklightSettings settings;
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    const size_t settings_size = sizeof(RGBBacklightSettings);

    FURI_LOG_I(TAG, "saving settings to \"%s\"", RGB_BACKLIGHT_SETTINGS_PATH);

    memcpy(&settings, &rgb_settings, settings_size);

    bool fs_result =
        storage_file_open(file, RGB_BACKLIGHT_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(fs_result) {
        uint16_t bytes_count = storage_file_write(file, &settings, settings_size);

        if(bytes_count != settings_size) {
            fs_result = false;
        }
    }

    if(fs_result) {
        FURI_LOG_I(TAG, "save success");
    } else {
        FURI_LOG_E(TAG, "save failed, %s", storage_file_get_error_desc(file));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
};

RGBBacklightSettings* rgb_backlight_get_settings(void) {
    if(!rgb_settings.settings_is_loaded) {
        rgb_backlight_load_settings();
    }
    return &rgb_settings;
}

void rgb_backlight_set_color(uint8_t color_index) {
    if(color_index > (rgb_backlight_get_color_count() - 1)) color_index = 0;
    rgb_settings.display_color_index = color_index;
}

void rgb_backlight_update(uint8_t brightness) {
    if(!rgb_settings.settings_is_loaded) {
        rgb_backlight_load_settings();
    }

    static uint8_t last_color_index = 255;
    static uint8_t last_brightness = 123;

    if(last_brightness == brightness && last_color_index == rgb_settings.display_color_index)
        return;

    last_brightness = brightness;
    last_color_index = rgb_settings.display_color_index;

    for(uint8_t i = 0; i < RGB_BACKLIGHT_LEDS; i++) {
        RGB_BACKLIGHT_ledbuffer[i][0] =
            colors[rgb_settings.display_color_index].green * (brightness / 255.0f);
        RGB_BACKLIGHT_ledbuffer[i][1] =
            colors[rgb_settings.display_color_index].red * (brightness / 255.0f);
        RGB_BACKLIGHT_ledbuffer[i][2] =
            colors[rgb_settings.display_color_index].blue * (brightness / 255.0f);
    }

    WS2812B_send();
}
