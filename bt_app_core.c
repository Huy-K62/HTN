#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/xtensa_api.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bt_app_core.h"
#include "driver/i2s.h"
#include "freertos/ringbuf.h"

static void bt_app_task_handler (void *arg);
static bool bt_app_send_msg (bt_app_msg_t *msg);
static void bt_app_work_dispatched (bt_app_msg_t *msg);

static xQueueHandle s_bt_app_task_queue = NULL;
static xTaskHandle s_bt_app_task_handle = NULL;
static xTaskHandle s_bt_i2s_task_handle = NULL;
static RingbufHandle_t s_ringbuf_i2s = NULL;;

bool bt_app_work_dispatch (bt_app_cb_t p_cback, uint16_t event, void *p_params, int param_len, bt_app_copy_cb_t p_copy_cback)
{
    ESP_LOGD (BT_APP_CORE_TAG, "%s event 0x%x, param len %d", __func__, event, param_len);

    bt_app_msg_t msg;
    memset (&msg, 0, sizeof(bt_app_msg_t));

    msg.sig = BT_APP_SIG_WORK_DISPATCH;
    msg.event = event;
    msg.cb = p_cback;

    if (param_len == 0) esp_a2d_register_callbaesp_a2d_register_callbackck
    {
        return bt_app_send_msg (&msg);
    } 
    else if (p_params && param_len > 0) 
    {
        if ((msg.param = malloc(param_len)) != NULL) 
        {
            memcpy (msg.param, p_params, param_len);
            /* check if caller has provided a copy callback to do the deep copy 
            kiểm tra xem người gọi đã cung cấp bản sao gọi lại chưa để thực hiện bản sao sâu
            */
            if (p_copy_cback) 
            {
                p_copy_cback (&msg, msg.param, p_params);
            }

            return bt_app_send_msg (&msg);
        }
    }

    return false;
}

static bool bt_app_send_msg (bt_app_msg_t *msg)
{
    if (msg == NULL) 
    {
        return false;
    }

    if (xQueueSend (s_bt_app_task_queue, msg, 10 / portTICK_RATE_MS) != pdTRUE) 
    {
        ESP_LOGE (BT_APP_CORE_TAG, "%s xQueue send failed", __func__);
        return false;
    }

    return true;
}

static void bt_app_work_dispatched (bt_app_msg_t *msg)
{
    if (msg->cb) 
    {
        msg->cb (msg->event, msg->param);
    }
}

/*
 * Hàm này lấy item từ hàng đợi của ứng dụng copy vào msg với khoảng thời gian tối đa cho phép 
 * Nếu hàng đợi có tín hiệu thì thực hiện hàm bt_app_work_dispatched ()
 * Nếu không thì báo không có tín hiệu và thoát khỏi hàm này 
 * Nếu param chứa thông số thì giải phóng param đó của msg
 */
static void bt_app_task_handler (void *arg)
{
    bt_app_msg_t msg;
    for (;;) 
    {
        if (pdTRUE == xQueueReceive (s_bt_app_task_queue, &msg, (portTickType)portMAX_DELAY)) 
        {
            ESP_LOGD (BT_APP_CORE_TAG, "%s, sig 0x%x, 0x%x", __func__, msg.sig, msg.event);

            switch (msg.sig) 
            {
                case BT_APP_SIG_WORK_DISPATCH:
                    bt_app_work_dispatched (&msg);
                    break;
                default:
                    ESP_LOGW (BT_APP_CORE_TAG, "%s, unhandled sig: %d", __func__, msg.sig);
                    break;
            } 

            if (msg.param) 
            {
                free(msg.param);
            }
        }
    }
}

void bt_app_task_start_up (void)
{
    /*
     * tạo một hàng đợi mới có độ dài = 10
     * và số byte = size của bt_app_msg_t mà mỗi mục trong hàng đợi yêu cầu 
     *
     * tạo một task cớ tên ứng dụng với 
     * thực hiện tiếp hàm bt_app_task_handler 
     * tên task là BtAppT, kích thước 3072 byte 
     * Độ ưu tiên thứ 3
     */
    s_bt_app_task_queue = xQueueCreate (10, sizeof(bt_app_msg_t));
    xTaskCreate (bt_app_task_handler, "BtAppT", 3072, NULL, configMAX_PRIORITIES - 3, &s_bt_app_task_handle);
    return;
}

void bt_app_task_shut_down (void)
{
    if (s_bt_app_task_handle) 
    {
        vTaskDelete (s_bt_app_task_handle);
        s_bt_app_task_handle = NULL;
    }
    if (s_bt_app_task_queue) 
    {
        vQueueDelete (s_bt_app_task_queue);
        s_bt_app_task_queue = NULL;
    }
}

static void bt_i2s_task_handler (void *arg)
{
    uint8_t *data = NULL;
    size_t item_size = 0;
    size_t bytes_written = 0;

    for (;;) 
    {
        data = (uint8_t *)xRingbufferReceive(s_ringbuf_i2s, &item_size, (portTickType)portMAX_DELAY);

        if (item_size != 0)
        {
            i2s_write (0, data, item_size, &bytes_written, portMAX_DELAY);
            vRingbufferReturnItem (s_ringbuf_i2s,(void *)data);
        }
    }
}

void bt_i2s_task_start_up (void)
{
    s_ringbuf_i2s = xRingbufferCreate (8 * 1024, RINGBUF_TYPE_BYTEBUF);

    if (s_ringbuf_i2s == NULL)
    {
        return;
    }

    xTaskCreate (bt_i2s_task_handler, "BtI2ST", 1024, NULL, configMAX_PRIORITIES - 3, &s_bt_i2s_task_handle);
    return;
}

void bt_i2s_task_shut_down (void)
{
    if (s_bt_i2s_task_handle) 
    {
        vTaskDelete (s_bt_i2s_task_handle);
        s_bt_i2s_task_handle = NULL;
    }

    if (s_ringbuf_i2s) 
    {
        vRingbufferDelete (s_ringbuf_i2s);
        s_ringbuf_i2s = NULL;
    }
}

size_t write_ringbuf (const uint8_t *data, size_t size)
{
    BaseType_t done = xRingbufferSend (s_ringbuf_i2s, (void *)data, size, (portTickType)portMAX_DELAY);
    if(done)
    {
        return size;
    } 
    else 
    {
        return 0;
    }
}