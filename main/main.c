#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "bsp/power.h"
#include "custom_certificates.h"
#include "driver/gpio.h"
#include "em_test.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "hal/lcd_types.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_types.h"
#include "portmacro.h"
#include "wifi_connection.h"
#include "wifi_remote.h"

#include <stdio.h>

// Constants
static char const TAG[] = "main";

// Global variables
static size_t                     display_h_res        = 0;
static size_t                     display_v_res        = 0;
static bsp_display_color_format_t display_color_format = 0;
static bsp_display_endianness_t   display_data_endian  = 0;
static pax_buf_t                  fb                   = {0};
static QueueHandle_t              input_event_queue    = NULL;

static void blit(void) {
    esp_err_t res = bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(&fb));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to blit to display: %d", res);
    }
}

static void display_message(char const *message) {
    if (pax_buf_get_width(&fb) > 0) {
        pax_background(&fb, 0xff000000);
        pax_draw_text(&fb, 0xffffffff, pax_font_sky_mono, 16, 0, 0, message);
        blit();
    } else {
        ESP_LOGI(TAG, "Message: %s", message);
    }
}

void app_main(void) {
    // Start the GPIO interrupt service
    gpio_install_isr_service(0);

    // Initialize the Non Volatile Storage partition
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        res = nvs_flash_erase();
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %d", res);
            return;
        }
        res = nvs_flash_init();
    }
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %d", res);
        return;
    }

    // Initialize the Board Support Package
    bsp_configuration_t const bsp_configuration = {
        .display = {
            .requested_color_format = BSP_DISPLAY_COLOR_FORMAT_24_888RGB,
            .num_fbs                = 1,
        },
    };
    res = bsp_device_initialize(&bsp_configuration);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP: %d", res);
        return;
    }

    // Get display parameters and rotation
    ESP_ERROR_CHECK(
        bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format, &display_data_endian)
    );

    // Convert ESP-IDF color format into PAX buffer type
    pax_buf_type_t format = PAX_BUF_24_888RGB;
    switch (display_color_format) {
        case BSP_DISPLAY_COLOR_FORMAT_1_PAL: format = PAX_BUF_1_PAL; break;
        case BSP_DISPLAY_COLOR_FORMAT_2_PAL: format = PAX_BUF_2_PAL; break;
        case BSP_DISPLAY_COLOR_FORMAT_4_PAL: format = PAX_BUF_4_PAL; break;
        case BSP_DISPLAY_COLOR_FORMAT_8_PAL: format = PAX_BUF_8_PAL; break;
        case BSP_DISPLAY_COLOR_FORMAT_16_PAL: format = PAX_BUF_16_PAL; break;
        case BSP_DISPLAY_COLOR_FORMAT_1_GREY: format = PAX_BUF_1_GREY; break;
        case BSP_DISPLAY_COLOR_FORMAT_2_GREY: format = PAX_BUF_2_GREY; break;
        case BSP_DISPLAY_COLOR_FORMAT_4_GREY: format = PAX_BUF_4_GREY; break;
        case BSP_DISPLAY_COLOR_FORMAT_8_GREY: format = PAX_BUF_8_GREY; break;
        case BSP_DISPLAY_COLOR_FORMAT_8_332RGB: format = PAX_BUF_8_332RGB; break;
        case BSP_DISPLAY_COLOR_FORMAT_16_565RGB: format = PAX_BUF_16_565RGB; break;
        case BSP_DISPLAY_COLOR_FORMAT_4_1111ARGB: format = PAX_BUF_4_1111ARGB; break;
        case BSP_DISPLAY_COLOR_FORMAT_8_2222ARGB: format = PAX_BUF_8_2222ARGB; break;
        case BSP_DISPLAY_COLOR_FORMAT_16_4444ARGB: format = PAX_BUF_16_4444ARGB; break;
        case BSP_DISPLAY_COLOR_FORMAT_24_888RGB: format = PAX_BUF_24_888RGB; break;
        case BSP_DISPLAY_COLOR_FORMAT_32_8888ARGB: format = PAX_BUF_32_8888ARGB; break;
        case BSP_DISPLAY_COLOR_FORMAT_18_666RGB:
        default: ESP_LOGW(TAG, "BSP requests color format not supported by PAX (%u)", format); break;
    }

    // Convert BSP display rotation format into PAX orientation type
    bsp_display_rotation_t display_rotation = bsp_display_get_default_rotation();
    pax_orientation_t      orientation      = PAX_O_UPRIGHT;
    switch (display_rotation) {
        case BSP_DISPLAY_ROTATION_90: orientation = PAX_O_ROT_CCW; break;
        case BSP_DISPLAY_ROTATION_180: orientation = PAX_O_ROT_HALF; break;
        case BSP_DISPLAY_ROTATION_270: orientation = PAX_O_ROT_CW; break;
        case BSP_DISPLAY_ROTATION_0:
        default: orientation = PAX_O_UPRIGHT; break;
    }

    // Initialize graphics stack
    printf(
        "Initializing framebuffer with w=%d h=%d format=%d endian=%d orientation=%d\n",
        display_h_res,
        display_v_res,
        format,
        display_data_endian,
        orientation
    );
    pax_buf_init(&fb, NULL, display_h_res, display_v_res, format);
    pax_buf_reversed(&fb, display_data_endian == BSP_DISPLAY_ENDIAN_BIG);
    pax_buf_set_orientation(&fb, orientation);

    // Get input event queue from BSP
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    em_run_tests();
    display_message("Welcome! Press any key to trigger an event.");

    while (1) {
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case INPUT_EVENT_TYPE_NAVIGATION:
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F1) {
                        bsp_device_restart_to_launcher();
                    }
                    break;

                case INPUT_EVENT_TYPE_ACTION: break;

                case INPUT_EVENT_TYPE_SCANCODE: break;

                default: break;
            }
        }
    }
}
