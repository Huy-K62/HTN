#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "driver/i2s.h"

/* event for handler "bt_av_hdl_stack_up
 su kien cho trinh xu ly bt_av_hdl_stack_up
*/
enum {
    BT_APP_EVT_STACK_UP = 0,
};

/* handler for bluetooth stack enabled events 
 trinh xu ly cho cac su kien ho tro ngan xep bluetooth
*/
static void bt_av_hdl_stack_evt (uint16_t event, void *p_param);


void app_main (void)
{
    /* 
     * Khởi tạo NVS - nó được sử dụng để lưu trữ dữ liệu hiệu chuẩn PHY  
     * Dữ liệu khởi tạo PHY được sử dụng để hiệu chuẩn RF
     * Non-volatile storage: NVS hoạt động tốt nhất để lưu trữ nhiều giá trị nhỏ
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
     */

    /*
    * esp_err_t la kieu so nguyen co dau, su dung de tra ve ma loi
    * neu không có lỗi được chỉ ra bằng mã ESP_OK
    * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/error-handling.html
    */
    esp_err_t err = nvs_flash_init (); //khởi tạo phân vùng NVS mặc định được mã hóa

    /* 
    * ESP_ERR_NVS_NEW_VERSION_FOUND: Phân vùng NVS chứa dữ liệu ở định dạng mới
      và phiên bản mã này không thể nhận dạng được
    * ESP_ERR_NVS_NO_FREE_PAGES: Phân vùng NVS không chứa bất kỳ trang trống nào. 
      Điều này có thể xảy ra nếu phân vùng NVS bị cắt ngắn. Xóa toàn bộ phân vùng và gọi lại nvs_flash_init.
    */
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        /*
        nvs_flash_erase: Xóa phân vùng NVS mặc định.
        Xóa tất cả nội dung của phân vùng NVS mặc định (một phân vùng có nhãn “nvs”).
        Nếu phân vùng được khởi tạo, chức năng này trước tiên sẽ khử khởi tạo nó. 
        Sau đó, phân vùng phải được khởi tạo lại để sử dụng
        */
        ESP_ERROR_CHECK (nvs_flash_erase ()); 
        err = nvs_flash_init ();
    }

    /*
     * Macro ESP_ERROR_CHECK () được sử dụng để kiểm tra mã lỗi 
     * và kết thúc chương trình trong trường hợp mã không phải là ESP_OK
     * In mã lỗi, vị trí lỗi và câu lệnh không thành công vào đầu ra nối tiếp
     */
    ESP_ERROR_CHECK (err);


    /*
     * Cấu hình một master i2s
     * là giao thức truyền thông đồng bộ được sử dụng trong truyền audio giữa các thiết bị audio số 
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
     */
    i2s_config_t i2s_config = {
#ifdef CONFIG_EXAMPLE_A2DP_SINK_OUTPUT_INTERNAL_DAC
    /*
    I2S_MODE_DAC_BUILT_IN: Xuất dữ liệu I2S sang DAC tích hợp, 
    bất kể định dạng dữ liệu là 16bit hay 32 bit, mô-đun DAC sẽ chỉ lấy 8bit từ MSB
    */
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
#else
        /*
        Chế độ I2S
        Mode master , Mode TX 
        */
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
#endif
        .sample_rate = 44100, // tốc độ lấy mẫu (HZ), ti le mau
        .bits_per_sample = 16, // Độ rộng bit I2S trên mỗi mẫu o day la 16 bit
        /*
        Loại định dạng kênh I2S.
        Kênh trái và phải được tách biệt
        */        
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // 2-channels
        /*
        Định dạng chuẩn giao tiếp I2S. I2S_COMM_FORMAT_STAND_MSB = 0X02
        Giao tiếp I2S tiêu chuẩn căn chỉnh MSB, khởi chạy dữ liệu ở BCK đầu tiên
        */       
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .dma_buf_count = 6, // sử dụng 6 bộ đệm DMA
        .dma_buf_len = 60, // do dai moi bo dem
        .intr_alloc_flags = 0,          //Default interrupt priority
        .tx_desc_auto_clear = true      //Auto clear tx descriptor on underflow
    };

    /*
    Cài đặt và khởi động trình điều khiển I2S. 
    i2s_driver_install trả về ESP_OK, điều đó có nghĩa là I2S đã bắt đầu
    */
    i2s_driver_install (0, &i2s_config, 0, NULL);
#ifdef CONFIG_EXAMPLE_A2DP_SINK_OUTPUT_INTERNAL_DAC
    /*
    I2S hỗ trợ DMA nghĩa là nó có thể truyền dữ liệu mẫu liên tục 
    mà không yêu cầu mỗi mẫu phải được đọc hoặc viết bởi CPU.
    DMA (Direct Memory Access) là cơ chế chuyển dữ liệu trực tiếp từ I/O vào RAM mà không cần thông qua CPU.
    Giúp dữ liệu được truyền đi nhanh hơn đặc biệt là các dữ liệu lớn và có tính liên tục.
    */
    i2s_set_dac_mode (I2S_DAC_CHANNEL_BOTH_EN);//Đặt chế độ dac I2S, chỉ muốn khởi tạo một trong 2 kênh DAC 
    i2s_set_pin (0, NULL);//chức năng này sẽ khởi tạo cả hai kênh DAC tích hợp theo mặc định GPIO 25 và GPIO26
#else
    i2s_pin_config_t pin_config = {
        .bck_io_num = CONFIG_EXAMPLE_I2S_BCK_PIN, //BCK vào ra pin
        .ws_io_num = CONFIG_EXAMPLE_I2S_LRCK_PIN, //WS vào mã pin
        .data_out_num = CONFIG_EXAMPLE_I2S_DATA_PIN, //Ghim ra DATA
        .data_in_num = -1     //Data trong pin                        //Not used
    };

    i2s_set_pin (0, &pin_config); //đặt cấu hình chân DAC nhu da config
#endif


    /*
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/controller_vhci.html
     * kiểm tra xem bộ nhớ điều khiển đã được giải phóng ở chế độ bluetooth BLE chua
     */
    ESP_ERROR_CHECK (esp_bt_controller_mem_release (ESP_BT_MODE_BLE));

    /*
     * tạo biến bt_cfg kiểu esp_bt_controller_congif_t cho việc điều khiển bluetooth 
     */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT ();

    /*
     * kiểm tra lại bộ điều khiển bluetooth đã được tạo hay chưa và
     * thông báo nếu xảy ra vấn đề lỗi nào 
     * esp_bt_controller_init: Khởi tạo bộ điều khiển BT để cấp phát tác vụ và tài nguyên khác
     * ESP_LOGE: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
     * esp_err_to_name: tìm mã lỗi trong bảng tra cứu được tạo trước và trả về biểu diễn chuỗi của nó
     */
    if ((err = esp_bt_controller_init (&bt_cfg)) != ESP_OK) 
    {
        // Log?? Loge la muc thap nhat, bao loi
        ESP_LOGE (BT_AV_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name (err));
        return;
    }
    // Bật bộ điều khiển BT
    if ((err = esp_bt_controller_enable (ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) 
    {
        ESP_LOGE (BT_AV_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name (err));
        return;
    }
    // Khoi tao và phân bổ tài nguyên cho bluetooth, phải có trước mọi công cụ bluetooth
    if ((err = esp_bluedroid_init ()) != ESP_OK) 
    {
        ESP_LOGE (BT_AV_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name (err));
        return;
    }
    // Bat bluetooth
    if ((err = esp_bluedroid_enable()) != ESP_OK) 
    {
        ESP_LOGE (BT_AV_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name (err));
        return;
    }

    /* create application task 
     * tạo tác vụ ứng dụng
     * bt_app_task_start_up ham ben app_core
     */
    bt_app_task_start_up ();

    /* Bluetooth device name, connection mode and profile set up 
     * Tên thiết bị Bluetooth, chế độ kết nối và thiết lập cấu hình 
     * bt_app_work_dispatch ham ben app_core
     */
    bt_app_work_dispatch (bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    /* Serial Port Profile
     * Cấu hình cổng nối tiếp xác định cách thiết lập cổng nối tiếp ảo
     * và kết nối hai thiết bị hỗ trợ Bluetooth.
     * Nó được sử dụng để thay thế giao diện giao tiếp nối tiếp (như RS-232 hoặc UART) bằng Bluetooth
     */
#if (CONFIG_BT_SSP_ENABLED == true)
    /* Set default parameters for Secure Simple Pairing
     * Đặt thông số mặc định cho Ghép nối đơn giản an toàn
     */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE; // Đặt chế độ IO
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO; // mặc định iocap của smartphonr là ESP_BT_IO_CAP_IO
    
    /*
     * Đặt giá trị thông số bảo mật GAP. Ghi đè giá trị mặc định.
     * GAP: Generic Access Profile, giúp thiết bị của bạn hiển thị với thế giới bên ngoài 
     * và xác định cách hai thiết bị có thể (hoặc không thể) tương tác với nhau.
     */
    esp_bt_gap_set_security_param (param_type, &iocap, sizeof (uint8_t));
#endif

    /*
     * Set default parameters for Legacy Pairing
     * Use fixed pin code
     * Đặt thông số mặc định cho Ghép nối kế thừa
     * Sử dụng mã pin cố định
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    //Đặt loại pin và mã pin mặc định cho ghép nối kế thừa.
    esp_bt_gap_set_pin (pin_type, 4, pin_code);

}
    /*
    bt_app_gap_cb: Loại chức năng gọi lại bluetooth GAP
    event: Loại sự kiện
    param: Con trỏ tới tham số gọi lại
    https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_bt.html#
    */
void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) 
    {
        /*
        ESP_BT_GAP_AUTH_CMPL_EVT: AUTH sự kiện hoàn thành
        Sự kiện hoàn tất xác thực
        */
        case ESP_BT_GAP_AUTH_CMPL_EVT: 
        {
            /*
            ESP_BT_STATUS_SUCCESS: cấu hình thành công
            param->auth_cmpl.stat: xác thực hoàn thành trạng thái
            param->auth_cmpl.device_name: ten thiet bi
            param->auth_cmpl.bda: địa chỉ thiết bị bluetooth từ xa
            ESP_BD_ADDR_LEN: Độ dài địa chỉ Bluetooth.
            */
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) 
            {
                ESP_LOGI (BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
                //Ghi nhật ký một bộ đệm gồm các byte hex ở mức Thông tin.
                esp_log_buffer_hex (BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
            } 
            else 
            {
                ESP_LOGE (BT_AV_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
            }
            break;
        }

#if (CONFIG_BT_SSP_ENABLED == true)
    /*
     * ESP_BT_GAP_CFM_REQ_EVT: Yêu cầu xác nhận người dùng ghép nối đơn giản
     * param->cfm_req.num_val: giá trị số để so sánh
     * param->cfm_req.bda: địa chỉ thiết bị bluetooth từ xa
     */
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI (BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
        //Trả lời giá trị xác nhận cho thiết bị ngang hàng trong giai đoạn kết nối kế thừa.
        esp_bt_gap_ssp_confirm_reply (param->cfm_req.bda, true);
        break;

    /*
     * ESP_BT_GAP_KEY_NOTIF_EVT: Thông báo mã khóa ghép nối đơn giản
     * param->key_notif.passkey: giá trị số cho mục nhập khóa mật khẩu
     */
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI (BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
        break;

    /* 
     * ESP_BT_GAP_KEY_REQ_EVT: Yêu cầu mã khóa ghép nối đơn giản   
     */
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI (BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif
    /* 
     *  param->mode_chg.mode: Chế độ PM (Power mode)
     */
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI (BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d", param->mode_chg.mode);
        break;

        default: 
        {
            ESP_LOGI (BT_AV_TAG, "event: %d", event);
            break;
        }
    }

    return;
}

static void bt_av_hdl_stack_evt (uint16_t event, void *p_param)
{
    // LOGD: debug
    ESP_LOGD (BT_AV_TAG, "%s evt %d", __func__, event);
    switch (event) 
    {
        case BT_APP_EVT_STACK_UP: 
        {
            /* set up device name 
             * esp_bt_dev_set_device_name: Đặt tên thiết bị bluetooth
            */
            char *dev_name = "ESP_SPEAKER";
            esp_bt_dev_set_device_name (dev_name);

            /*
            esp_bt_gap_register_callback: đăng ký chức năng gọi lại. 
            Đặt chế độ khả năng phát hiện và khả năng kết nối cho bluetooth cũ.
            Hàm này sẽ được gọi sau khi esp_bluedroid_enable () hoàn tất thành công  
            */
            esp_bt_gap_register_callback (bt_app_gap_cb);

            /* https://baocantho.com.vn/mot-so-loai-cau-hinh-bluetooth-thong-dung-a98928.html
             esp_avrc_ct_init: Khởi tạo mô-đun bộ điều khiển AVRCP bluetooth
             AVRC không thể hoạt động độc lập,
             AVRC nên được sử dụng cùng với A2DP và AVRC nên được khởi tạo trước A2DP
             esp_avrc_ct_register_callback: Đăng ký các lệnh gọi lại của ứng dụng tới mô-đun AVRCP
             bt_app_rc_ct_cb: ham nay ben app_av.c, to chu thich ben do
            */
            esp_avrc_ct_init ();
            esp_avrc_ct_register_callback (bt_app_rc_ct_cb);

            /* khởi tạo mục tiêu AVRCP
             * esp_avrc_tg_init: Khởi tạo mô-đun đích bluetooth AVRCP 
             * esp_avrc_tg_register_callback: Đăng ký các lệnh gọi lại của ứng dụng tới mô-đun đích AVRCP.          
             * bt_app_rc_tg_cb: ham nay ben app_av.c, to chu thich ben do
             */
            assert (esp_avrc_tg_init () == ESP_OK); //assert sẽ chấm dứt chương trình (thường có thông báo trích dẫn câu khẳng định) nếu đối số của nó hóa ra là sai. Nó thường được sử dụng trong quá trình gỡ lỗi để làm cho chương trình thất bại rõ ràng hơn nếu xảy ra tình trạng không mong muốn
            esp_avrc_tg_register_callback (bt_app_rc_tg_cb);

            /*
              esp_avrc_rn_evt_cap_mask_t evt_set: event_ids được hỗ trợ được biểu diễn trong bit-mask
              esp_avrc_rn_evt_bit_mask_operation: (Gửi phản hồi Đăng ký Thông báo tới bộ điều khiển AVRCP từ xa.
              Thông báo sự kiện địa phương khả năng có thể được thiết lập bằng cách sử dụng esp_avrc_tg_set_rn_evt_cap (),
              trong biểu diễn mặt nạ bit trong evt_set. 
              Đối với hoạt động ESP_AVRC_BIT_MASK_OP_SET hoặc ESP_AVRC_BIT_MASK_OP_CLEAR,
              trả về true cho một hoạt động thành công, nếu không trả về false).
              https://github.com/espressif/esp-idf/blob/b63ec47/components/bt/host/bluedroid/api/include/api/esp_avrc_api.h
             */
            esp_avrc_rn_evt_cap_mask_t evt_set = {0};
            esp_avrc_rn_evt_bit_mask_operation (ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
            assert (esp_avrc_tg_set_rn_evt_cap (&evt_set) == ESP_OK);

            /* 
             * esp_a2d_register_callback: (Đăng ký chức năng gọi lại của ứng dụng đến mô-đun A2DP
             * Đăng ký chức năng xuất dữ liệu chìm A2DP, 
             * Hiện tại, đầu ra là luồng dữ liệu PCM được giải mã từ định dạng SBC).
             * bt_app_a2d_cb: Ham nay ben app_av, to chu thich ben do
             * esp_a2d_sink_register_data_callback: (Đăng ký chức năng xuất dữ liệu chìm A2DP; 
             * Hiện tại, đầu ra là luồng dữ liệu PCM được giải mã từ định dạng SBC
             * Lệnh gọi lại được gọi trong ngữ cảnh của tác vụ chìm A2DP 
             * có kích thước ngăn xếp có thể định cấu hình thông qua menuconfig.)
             * bt_app_a2d_data_cb: Ham nay ben app_av, to chu thich ben do
             * esp_a2d_sink_init: Khởi tạo mô-đun chìm A2DP bluetooth.
            */
            esp_a2d_register_callback (&bt_app_a2d_cb);
            esp_a2d_sink_register_data_callback (bt_app_a2d_data_cb);
            esp_a2d_sink_init ();

            /*
             * esp_bt_gap_set_scan_mode: Đặt chế độ khả năng phát hiện và khả năng kết nối cho bluetooth cũ
             * ESP_BT_CONNECTABLE: Có thể kết nối
             * ESP_BT_GENERAL_DISCOVERABLE: Có thể khám phá chung
             */
            esp_bt_gap_set_scan_mode (ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            break;
        }

        default:
            ESP_LOGE (BT_AV_TAG, "%s unhandled evt %d", __func__, event);
            break;
    }
}