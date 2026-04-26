/*
 * ESP32-C5 工厂料架定位终端示例
 *
 * 这个程序完成三个核心动作：
 * 1. 启动 Wi-Fi STA；
 * 2. 周期性扫描周边可见的 Wi-Fi AP；
 * 3. 将扫描结果打印到串口，并上传到上位系统。
 *
 * 说明：
 * - 这里的定位不是在终端本地计算，而是由上位系统根据 AP 坐标和 RSSI 做定位；
 * - 终端负责采集“Wi-Fi 指纹”，也就是 BSSID、RSSI、信道等信息；
 * - 当前代码是工程骨架，适合作为后续接入 NVS 配置、HTTPS、掉线缓存的基础版本。
 * - 当前版本会固定连接指定 AP，并将扫描结果 POST 到上位系统。
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define MAX_SCANNED_APS 16
#define JSON_PAYLOAD_BUFFER_SIZE 4096
#define HTTP_RESPONSE_BUFFER_SIZE 512
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "factory_locator";
static EventGroupHandle_t s_wifi_event_group;

/* 终端运行时配置。
 * 当前先写死在代码里，后续建议迁移到 NVS 或产测参数区。 */
typedef struct {
    const char *device_id;
    const char *backhaul_ssid;
    const char *backhaul_password;
    const char *server_url;
    uint32_t scan_period_ms;
} locator_config_t;

static const locator_config_t LOCATOR_CONFIG = {
    .device_id = "rack-tag-001",
    .backhaul_ssid = "yang",
    .backhaul_password = "yang123456",
    .server_url = "http://192.168.1.3:8080/api/v1/locate",
    .scan_period_ms = 5000,
};

/* Wi-Fi / IP 事件回调。
 * 当前阶段使用固定的 AP 参数联网，并在掉线后自动重连。 */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA started, connecting to SSID=%s", LOCATOR_CONFIG.backhaul_ssid);
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from SSID=%s, retrying", LOCATOR_CONFIG.backhaul_ssid);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected to %s, IP: " IPSTR, LOCATOR_CONFIG.backhaul_ssid, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* 初始化 STA 模式。
 * 当前阶段连接固定回传 AP，同时保留扫描和串口打印能力。 */
static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* 固定本次调试要连接的 AP：SSID=yang，密码=yang123456。 */
    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.sta.ssid,
            LOCATOR_CONFIG.backhaul_ssid,
            sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password,
            LOCATOR_CONFIG.backhaul_password,
            sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    /* 先全信道搜索，再按信号强度优先接入目标 AP。 */
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi station started with fixed backhaul SSID=%s", LOCATOR_CONFIG.backhaul_ssid);
}

/* 将扫描结果组织成上位系统需要的 JSON 字符串。
 * 调试阶段虽然不上传，但保持和上位系统一致的数据结构，便于后续直接恢复 HTTP 上报。 */
static void build_payload(char *buffer,
                          size_t buffer_size,
                          const wifi_ap_record_t *records,
                          uint16_t count)
{
    uint8_t sta_mac[6] = {0};
    char mac_string[18] = {0};
    int64_t ts_ms = esp_timer_get_time() / 1000;
    int written = 0;

    /* 注意：新版本 IDF 不再通过 esp_system.h 间接引入 esp_mac.h，
     * 所以本文件已经显式包含了 esp_mac.h。 */
    ESP_ERROR_CHECK(esp_read_mac(sta_mac, ESP_MAC_WIFI_STA));
    snprintf(mac_string,
             sizeof(mac_string),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             sta_mac[0], sta_mac[1], sta_mac[2],
             sta_mac[3], sta_mac[4], sta_mac[5]);

    written = snprintf(buffer,
                       buffer_size,
                       "{\"device_id\":\"%s\",\"device_mac\":\"%s\",\"timestamp_ms\":%" PRId64 ",\"scan_results\":[",
                       LOCATOR_CONFIG.device_id,
                       mac_string,
                       ts_ms);

    if (written < 0 || (size_t)written >= buffer_size) {
        if (buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }

    for (uint16_t i = 0; i < count; ++i) {
        char bssid[18] = {0};
        int item_written = 0;

        /* BSSID 统一转成字符串形式，便于服务端直接查表匹配。 */
        snprintf(bssid,
                 sizeof(bssid),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 records[i].bssid[0], records[i].bssid[1], records[i].bssid[2],
                 records[i].bssid[3], records[i].bssid[4], records[i].bssid[5]);

        item_written = snprintf(buffer + written,
                                buffer_size - (size_t)written,
                                "%s{\"bssid\":\"%s\",\"ssid\":\"%s\",\"rssi\":%d,"
                                "\"primary_channel\":%u,\"secondary_channel\":%u,"
                                "\"is_backhaul\":%s}",
                                (i == 0) ? "" : ",",
                                bssid,
                                (const char *)records[i].ssid,
                                records[i].rssi,
                                records[i].primary,
                                records[i].second,
                                strcmp((const char *)records[i].ssid, LOCATOR_CONFIG.backhaul_ssid) == 0 ? "true" : "false");
        if (item_written < 0 || (size_t)item_written >= (buffer_size - (size_t)written)) {
            buffer[0] = '\0';
            return;
        }
        written += item_written;
    }

    if ((size_t)written < buffer_size - 2U) {
        snprintf(buffer + written, buffer_size - (size_t)written, "]}");
    } else if (buffer_size > 0) {
        buffer[0] = '\0';
    }
}

/* 根据信道号判断频段 */
static const char* get_band_name(uint8_t channel)
{
    if (channel >= 1 && channel <= 14) {
        return "2.4GHz";
    } else if (channel >= 36 && channel <= 165) {
        return "5GHz";
    } else {
        return "Unknown";
    }
}

/* 把每轮扫描结果逐条打印到串口，便于调试阶段观察现场 AP 分布。
 * 这里既打印结构化摘要，也保留后面的完整 JSON。 */
static void print_scan_summary(const wifi_ap_record_t *records, uint16_t count)
{
    ESP_LOGI(TAG, "Scan completed, AP count=%u", count);
    for (uint16_t i = 0; i < count; ++i) {
        const char *band = get_band_name(records[i].primary);
        ESP_LOGI(TAG,
                 "[%u] SSID=%s BSSID=%02X:%02X:%02X:%02X:%02X:%02X RSSI=%d CH=%u (%s)",
                 i,
                 (const char *)records[i].ssid,
                 records[i].bssid[0], records[i].bssid[1], records[i].bssid[2],
                 records[i].bssid[3], records[i].bssid[4], records[i].bssid[5],
                 records[i].rssi,
                 records[i].primary,
                 band);
    }
}

/* 上传扫描结果到上位系统。
 * 当前使用 Spring Boot 的 /api/v1/locate 接口。 */
static esp_err_t http_post_scan_payload(const char *payload)
{
    char response_buffer[HTTP_RESPONSE_BUFFER_SIZE] = {0};
    esp_http_client_config_t http_cfg = {
        .url = LOCATOR_CONFIG.server_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .user_data = response_buffer,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_fetch_headers(client);
        int read_len = esp_http_client_read_response(client, response_buffer, sizeof(response_buffer) - 1);
        if (read_len < 0) {
            read_len = 0;
        }
        response_buffer[read_len] = '\0';

        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG,
                     "Upload success, status=%d, content_length=%d, url=%s, body=%s",
                     status_code,
                     content_length,
                     LOCATOR_CONFIG.server_url,
                     response_buffer);
        } else {
            ESP_LOGE(TAG,
                     "Server returned error, status=%d, content_length=%d, url=%s, body=%s",
                     status_code,
                     content_length,
                     LOCATOR_CONFIG.server_url,
                     response_buffer);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG,
                 "Upload failed: %s, url=%s",
                 esp_err_to_name(err),
                 LOCATOR_CONFIG.server_url);
    }

    esp_http_client_cleanup(client);
    return err;
}

/* 周期扫描并打印。
 * 当前阶段会把扫描结果打印到串口，同时上传到上位系统。 */
static void scan_and_upload_task(void *arg)
{
    /* 全频段主动扫描。
     * 未来如果已知工厂 AP 只部署在特定信道，也可以在这里做信道收缩以提高速度。 */
    wifi_scan_config_t scan_cfg = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    wifi_ap_record_t records[MAX_SCANNED_APS];
    char payload_text[JSON_PAYLOAD_BUFFER_SIZE];

    while (true) {
        /* 先确认已经连上固定回传 AP，再开始扫描。 */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT,
                                               pdFALSE,
                                               pdTRUE,
                                               pdMS_TO_TICKS(10000));
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGW(TAG, "Backhaul SSID=%s not connected yet, postpone scan", LOCATOR_CONFIG.backhaul_ssid);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* 每轮扫描前先清空上一次的缓存数据。 */
        memset(records, 0, sizeof(records));
        uint16_t ap_count = MAX_SCANNED_APS;

        ESP_LOGI(TAG, "Starting AP scan while connected to %s", LOCATOR_CONFIG.backhaul_ssid);
        esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(LOCATOR_CONFIG.scan_period_ms));
            continue;
        }

        /* 从驱动中读出扫描到的 AP 列表。
         * 如果实际 AP 数量超过 MAX_SCANNED_APS，则只保留前 N 个。 */
        err = esp_wifi_scan_get_ap_records(&ap_count, records);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Read scan results failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(LOCATOR_CONFIG.scan_period_ms));
            continue;
        }

        print_scan_summary(records, ap_count);

        /* 生成 JSON，先打印到串口，再上传到上位系统。 */
        memset(payload_text, 0, sizeof(payload_text));
        build_payload(payload_text, sizeof(payload_text), records, ap_count);
        if (payload_text[0] != '\0') {
            ESP_LOGI(TAG, "Generated payload: %s", payload_text);
            http_post_scan_payload(payload_text);
        } else {
            ESP_LOGW(TAG, "Payload buffer is not large enough, JSON output skipped");
        }

        /* 控制扫描节奏，避免过于频繁地占用射频资源。 */
        vTaskDelay(pdMS_TO_TICKS(LOCATOR_CONFIG.scan_period_ms));
    }
}

void app_main(void)
{
    /* 初始化 NVS。
     * Wi-Fi 驱动会依赖 NVS，因此这是绝大多数联网类 ESP-IDF 工程的标准初始化步骤。 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS 分区异常时先擦除再重建。 */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 启动 Wi-Fi 联网与扫描能力。 */
    wifi_init_sta();

    /* 创建主业务任务。
     * 8 KB 栈空间足够容纳 Wi-Fi 扫描、JSON 组包和串口打印这几个动作。 */
    xTaskCreate(scan_and_upload_task,
                "scan_and_upload_task",
                8192,
                NULL,
                5,
                NULL);
}
