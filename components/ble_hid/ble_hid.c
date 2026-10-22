/*
 * BLE HID keyboard + consumer-control (media) device for the reTerminal Sticky.
 *
 * Wraps the ESP-IDF `esp_hid` device API into a small, reusable component that
 * advertises as a BLE keyboard and can send keyboard reports, tap hotkeys, type
 * text, and send media keys.
 */

#include "ble_hid.h"

#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_hid_common.h"
#include "esp_hidd.h"
#include "esp_hidd_gatts.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "ble_hid";

static esp_hidd_dev_t *s_hid_dev = NULL;
static bool s_advertising = false;

#define BLE_HID_DEVICE_NAME "StickyDeck"

/* ------------------------------------------------------------------ */
/* Report maps                                                         */
/* ------------------------------------------------------------------ */

/* Standard HID keyboard: report ID 1, 8-byte input report. */
static const uint8_t keyboard_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,        //   Usage Minimum (0xE0)
    0x29, 0xE7,        //   Usage Maximum (0xE7)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Const,Var,Abs)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x91, 0x02,        //   Output (Data,Var,Abs,Non-volatile)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x03,        //   Output (Const,Var,Abs,Non-volatile)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,        //   Usage Minimum (0x00)
    0x29, 0x65,        //   Usage Maximum (0x65)
    0x81, 0x00,        //   Input (Data,Array,Abs)
    0xC0,              // End Collection
};

/* Consumer control (media): report ID 3, 2-byte input report.
 * This is the proven report map from the ESP-IDF esp_hid_device example. */
static const uint8_t media_report_map[] = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x09, 0x02,        //   Usage (Numeric Key Pad)
    0xA1, 0x02,        //   Collection (Logical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x0A,        //     Usage Maximum (0x0A)
    0x15, 0x01,        //     Logical Minimum (1)
    0x25, 0x0A,        //     Logical Maximum (10)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x00,        //     Input (Data,Array,Abs)
    0xC0,              //   End Collection
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x09, 0x86,        //   Usage (Channel)
    0x15, 0xFF,        //   Logical Minimum (-1)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x02,        //   Report Size (2)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x46,        //   Input (Data,Var,Rel)
    0x09, 0xE9,        //   Usage (Volume Increment)
    0x09, 0xEA,        //   Usage (Volume Decrement)
    0x15, 0x00,        //   Logical Minimum (0)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x09, 0xE2,        //   Usage (Mute)
    0x09, 0x30,        //   Usage (Power)
    0x09, 0x83,        //   Usage (Recall Last)
    0x09, 0x81,        //   Usage (Assign Selection)
    0x09, 0xB0,        //   Usage (Play)
    0x09, 0xB1,        //   Usage (Pause)
    0x09, 0xB2,        //   Usage (Record)
    0x09, 0xB3,        //   Usage (Fast Forward)
    0x09, 0xB4,        //   Usage (Rewind)
    0x09, 0xB5,        //   Usage (Scan Next Track)
    0x09, 0xB6,        //   Usage (Scan Previous Track)
    0x09, 0xB7,        //   Usage (Stop)
    0x15, 0x01,        //   Logical Minimum (1)
    0x25, 0x0C,        //   Logical Maximum (12)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data,Array,Abs)
    0x09, 0x80,        //   Usage (Selection)
    0xA1, 0x02,        //   Collection (Logical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x03,        //     Usage Maximum (0x03)
    0x15, 0x01,        //     Logical Minimum (1)
    0x25, 0x03,        //     Logical Maximum (3)
    0x75, 0x02,        //     Report Size (2)
    0x81, 0x00,        //     Input (Data,Array,Abs)
    0xC0,              //   End Collection
    0x81, 0x03,        //   Input (Const,Var,Abs)
    0xC0,              // End Collection
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    {.data = keyboard_report_map, .len = sizeof(keyboard_report_map)},
    {.data = media_report_map, .len = sizeof(media_report_map)},
};

static esp_hid_device_config_t s_hid_config = {
    .vendor_id = 0x2886, /* Seeed Studio */
    .product_id = 0x0001,
    .version = 0x0100,
    .device_name = BLE_HID_DEVICE_NAME,
    .manufacturer_name = "Seeed Studio",
    .serial_number = "STICKY0001",
    .report_maps = s_report_maps,
    .report_maps_len = 2,
};

/* Map index / report id for the two report maps. */
#define KB_MAP_INDEX 0
#define KB_REPORT_ID 1
#define MEDIA_MAP_INDEX 1
#define MEDIA_REPORT_ID 3

/* ------------------------------------------------------------------ */
/* BLE GAP                                                             */
/* ------------------------------------------------------------------ */

static bool s_adv_data_ready = false;
static bool s_adv_start_pending = false;

static void start_advertising(void)
{
    esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x320, /* ~500ms: leaves airtime for the Wi-Fi AP */
        .adv_int_max = 0x400, /* ~640ms */
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    esp_err_t ret = esp_ble_gap_start_advertising(&adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "start_advertising failed: %s", esp_err_to_name(ret));
    }
}

static void configure_advertising(void)
{
    /* HID service UUID, 128-bit form. */
    static const uint8_t hidd_service_uuid128[] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
    };

    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .appearance = ESP_BLE_APPEARANCE_HID_KEYBOARD,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(hidd_service_uuid128),
        .p_service_uuid = (uint8_t *)hidd_service_uuid128,
        .flag = 0x06,
    };

    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "config_adv_data failed: %s", esp_err_to_name(ret));
    }
}

static void request_advertising(void)
{
    if (s_adv_data_ready) {
        start_advertising();
    } else {
        s_adv_start_pending = true;
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        s_adv_data_ready = (param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS);
        if (!s_adv_data_ready) {
            ESP_LOGE(TAG, "adv data set failed: 0x%x", param->adv_data_cmpl.status);
        } else {
            ESP_LOGI(TAG, "adv data configured");
        }
        if (s_adv_start_pending && s_adv_data_ready) {
            s_adv_start_pending = false;
            start_advertising();
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "adv start failed: 0x%x", param->adv_start_cmpl.status);
        } else {
            s_advertising = true;
            ESP_LOGI(TAG, "advertising started");
        }
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        /* Accept pairing (Just Works). */
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* HID device events                                                   */
/* ------------------------------------------------------------------ */

static void hid_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    (void)handler_args;
    (void)base;
    (void)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID device registered, requesting advertising");
        request_advertising();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID client connected");
        s_advertising = false;
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID client disconnected, re-advertising");
        s_advertising = false;
        request_advertising();
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t ble_hid_init(void)
{
    if (s_hid_dev != NULL) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        return ret;
    }

    /* Release classic-BT memory, we are BLE-only. */
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "mem_release classic BT: %s", esp_err_to_name(ret));
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bt controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bt controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gap register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Just Works pairing + bonding, no MITM/passkey. */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, 1);
    esp_ble_gap_set_device_name(BLE_HID_DEVICE_NAME);

    configure_advertising();

    /* The esp_hid component owns the GATT HID service. */
    ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gatts register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE, hid_event_callback, &s_hid_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "hidd dev init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "BLE HID device '%s' initialized", BLE_HID_DEVICE_NAME);
    return ESP_OK;
}

bool ble_hid_connected(void)
{
    if (s_hid_dev == NULL) {
        return false;
    }
    return esp_hidd_dev_connected(s_hid_dev);
}

esp_err_t ble_hid_set_battery(uint8_t level)
{
    if (s_hid_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_hidd_dev_battery_set(s_hid_dev, level);
}

const char *ble_hid_device_name(void)
{
    return BLE_HID_DEVICE_NAME;
}

esp_err_t ble_hid_send_keyboard_report(const uint8_t report[8])
{
    if (s_hid_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t buffer[8];
    memcpy(buffer, report, sizeof(buffer));
    return esp_hidd_dev_input_set(s_hid_dev, KB_MAP_INDEX, KB_REPORT_ID, buffer, sizeof(buffer));
}

esp_err_t ble_hid_tap_keys(uint8_t modifiers, const uint8_t *keycodes, size_t count)
{
    uint8_t report[8] = {0};
    report[0] = modifiers;
    size_t n = count > 6 ? 6 : count;
    for (size_t i = 0; i < n; i++) {
        report[2 + i] = keycodes[i];
    }
    esp_err_t ret = ble_hid_send_keyboard_report(report);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(12));
    uint8_t release[8] = {0};
    return ble_hid_send_keyboard_report(release);
}

esp_err_t ble_hid_tap_key(uint8_t modifiers, uint8_t keycode)
{
    return ble_hid_tap_keys(modifiers, &keycode, 1);
}

/* ------------------------------------------------------------------ */
/* ASCII -> (modifier, HID keycode)  — US keyboard layout              */
/* ------------------------------------------------------------------ */

static bool ascii_to_key(char c, uint8_t *modifier, uint8_t *keycode)
{
    *modifier = 0;
    *keycode = 0;

    if (c >= 'a' && c <= 'z') {
        *keycode = 0x04 + (uint8_t)(c - 'a');
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        *modifier = BLE_HID_MOD_SHIFT;
        *keycode = 0x04 + (uint8_t)(c - 'A');
        return true;
    }
    if (c >= '1' && c <= '9') {
        *keycode = 0x1E + (uint8_t)(c - '1');
        return true;
    }
    if (c == '0') {
        *keycode = 0x27;
        return true;
    }

    switch (c) {
    case ' ':  *keycode = 0x2C; return true;
    case '\n': *keycode = 0x28; return true;
    case '\t': *keycode = 0x2B; return true;
    case '-':  *keycode = 0x2D; return true;
    case '=':  *keycode = 0x2E; return true;
    case '[':  *keycode = 0x2F; return true;
    case ']':  *keycode = 0x30; return true;
    case '\\': *keycode = 0x31; return true;
    case ';':  *keycode = 0x33; return true;
    case '\'': *keycode = 0x34; return true;
    case '`':  *keycode = 0x35; return true;
    case ',':  *keycode = 0x36; return true;
    case '.':  *keycode = 0x37; return true;
    case '/':  *keycode = 0x38; return true;

    case '_':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x2D; return true;
    case '+':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x2E; return true;
    case '{':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x2F; return true;
    case '}':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x30; return true;
    case '|':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x31; return true;
    case ':':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x33; return true;
    case '"':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x34; return true;
    case '~':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x35; return true;
    case '<':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x36; return true;
    case '>':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x37; return true;
    case '?':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x38; return true;
    case '!':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x1E; return true;
    case '@':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x1F; return true;
    case '#':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x20; return true;
    case '$':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x21; return true;
    case '%':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x22; return true;
    case '^':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x23; return true;
    case '&':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x24; return true;
    case '*':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x25; return true;
    case '(':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x26; return true;
    case ')':  *modifier = BLE_HID_MOD_SHIFT; *keycode = 0x27; return true;
    default:
        return false;
    }
}

esp_err_t ble_hid_type_text(const char *text)
{
    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (const char *p = text; *p != '\0'; p++) {
        uint8_t modifier = 0;
        uint8_t keycode = 0;
        if (!ascii_to_key(*p, &modifier, &keycode)) {
            continue; /* Skip characters outside the supported set. */
        }
        uint8_t report[8] = {0};
        report[0] = modifier;
        report[2] = keycode;
        esp_err_t ret = ble_hid_send_keyboard_report(report);
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(6));
        uint8_t release[8] = {0};
        ret = ble_hid_send_keyboard_report(release);
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(6));
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Media keys                                                          */
/* ------------------------------------------------------------------ */

/* Consumer-control report bit helpers (from the ESP-IDF esp_hid_device example). */
#define HID_CC_RPT_MUTE           1
#define HID_CC_RPT_POWER          2
#define HID_CC_RPT_LAST           3
#define HID_CC_RPT_ASSIGN_SEL     4
#define HID_CC_RPT_PLAY           5
#define HID_CC_RPT_PAUSE          6
#define HID_CC_RPT_RECORD         7
#define HID_CC_RPT_FAST_FWD       8
#define HID_CC_RPT_REWIND         9
#define HID_CC_RPT_SCAN_NEXT_TRK  10
#define HID_CC_RPT_SCAN_PREV_TRK  11
#define HID_CC_RPT_STOP           12

#define HID_CC_RPT_VOLUME_BITS    0x3F
#define HID_CC_RPT_BUTTON_BITS    0xF0
#define HID_CC_RPT_SET_VOLUME_UP(s)   (s)[0] &= HID_CC_RPT_VOLUME_BITS; (s)[0] |= 0x40
#define HID_CC_RPT_SET_VOLUME_DOWN(s) (s)[0] &= HID_CC_RPT_VOLUME_BITS; (s)[0] |= 0x80
#define HID_CC_RPT_SET_BUTTON(s, x)   (s)[1] &= HID_CC_RPT_BUTTON_BITS; (s)[1] |= (x)

static esp_err_t media_send(uint16_t usage, bool pressed)
{
    if (s_hid_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buffer[2] = {0, 0};
    if (pressed) {
        switch (usage) {
        case BLE_HID_MEDIA_VOLUME_UP:
            HID_CC_RPT_SET_VOLUME_UP(buffer);
            break;
        case BLE_HID_MEDIA_VOLUME_DOWN:
            HID_CC_RPT_SET_VOLUME_DOWN(buffer);
            break;
        case BLE_HID_MEDIA_MUTE:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_MUTE);
            break;
        case BLE_HID_MEDIA_PLAY:
        case BLE_HID_MEDIA_PLAY_PAUSE: /* no dedicated play/pause slot in map */
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PLAY);
            break;
        case BLE_HID_MEDIA_PAUSE:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_PAUSE);
            break;
        case BLE_HID_MEDIA_NEXT:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_NEXT_TRK);
            break;
        case BLE_HID_MEDIA_PREV:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_SCAN_PREV_TRK);
            break;
        case BLE_HID_MEDIA_STOP:
            HID_CC_RPT_SET_BUTTON(buffer, HID_CC_RPT_STOP);
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
        }
    }
    return esp_hidd_dev_input_set(s_hid_dev, MEDIA_MAP_INDEX, MEDIA_REPORT_ID, buffer, sizeof(buffer));
}

esp_err_t ble_hid_tap_media(uint16_t usage)
{
    esp_err_t ret = media_send(usage, true);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(40));
    return media_send(usage, false);
}
