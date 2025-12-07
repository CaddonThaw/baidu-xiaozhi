#include "xiaozhi_ai.h"
#include <ArduinoWebsockets.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UrlEncode.h>
#include "driver/i2s.h"

using namespace websockets;

// ===================== 百度相关参数及内部状态 =====================

// 从百度 OAuth 获取的 access_token
static String xiaozhi_baidu_access_token = " ";

// 百度实时语音识别 WebSocket URL
static String xiaozhi_baidu_input_url = "ws://vop.baidu.com/realtime_asr?sn=L1-E2-E3-";

// WebSocket 客户端实例
static WebsocketsClient xiaozhi_ws_client;

// 识别开始/结束 JSON 报文
static String xiaozhi_baidu_start_json;
static String xiaozhi_baidu_end_json = "{\"type\": \"FINISH\"}";

// I2S 采集缓冲区
static const int xiaozhi_i2s_buffer_size = 5120;
static char xiaozhi_i2s_buffer[xiaozhi_i2s_buffer_size];

// 上一次向百度发送音频的时间戳（毫秒）
static uint32_t xiaozhi_last_send_ms = 0;

// WebSocket 状态：0 空闲，1 已连接并推流
static uint8_t xiaozhi_ws_state = 0;

// 当前一句话的识别文本（最新返回）
static String xiaozhi_current_text;

// 当前完整问题文本（一轮会话的最终句子）
static String xiaozhi_question_text;

// 当前待播放的 TTS 音频 URL
static String xiaozhi_answer_host;

// AI 完整回答文本缓存
static String xiaozhi_answer_text;

// 标记是否已经有一句完整问题可供读取
static bool xiaozhi_have_question = false;

// 标记是否已经生成可播放的 TTS URL
static bool xiaozhi_have_answer = false;

// ===================== 内部工具函数声明 =====================

// 初始化 I2S 采集
static void xiaozhi_init_i2s();
// 刷新百度 access_token
static void xiaozhi_get_baidu_token();
// WebSocket 收到文本消息时的回调
static void xiaozhi_ws_on_message(WebsocketsMessage message);
// WebSocket 连接/关闭等事件回调
static void xiaozhi_ws_on_event(WebsocketsEvent event, String data);
// 开始一次语音识别会话
static void xiaozhi_start_asr();
// 结束当前语音识别会话
static void xiaozhi_stop_asr();
// 轮询 WebSocket 并按节奏推送音频
static void xiaozhi_update_ws();
// 从 I2S 读取一帧音频
static int  xiaozhi_read_i2s(char *data, int len);
// URL 编码包装
static String xiaozhi_url_encode(const String &s);
// 文本转 TTS URL
static void xiaozhi_tts_from_text(const String &text);

// ===================== 对外接口实现 =====================

void xiaozhi_init()
{
    // 只做一次 I2S 初始化和状态清零，避免反复初始化导致崩溃
    xiaozhi_init_i2s();

    xiaozhi_ws_state       = 0;
    xiaozhi_current_text   = "";
    xiaozhi_question_text  = "";
    xiaozhi_answer_host    = "";
    xiaozhi_have_question  = false;
    xiaozhi_have_answer    = false;
    xiaozhi_last_send_ms   = 0;
}

void xiaozhi_loop()
{
    // WebSocket 轮询 & 音频上报
    xiaozhi_update_ws();
}

bool xiaozhi_listen()
{
    if (!xiaozhi_ws_state)
    {
        // 如果没有在识别，则尝试启动一次
        xiaozhi_start_asr();
    }

    // 一旦有完整句子，就返回 true
    return xiaozhi_have_question;
}

String xiaozhi_question()
{
    String q = xiaozhi_question_text;
    // 读一次后仍然保留内容，由上层控制何时丢弃
    xiaozhi_have_question = false;
    return q;
}

bool xiaozhi_speak()
{
    return xiaozhi_have_answer;
}

String xiaozhi_answer(bool mode)
{
    if (!mode)
    {
        return xiaozhi_answer_text;
    }   
    else
    {
        String url = xiaozhi_answer_host;
        // 被读取一次后清空标记，等待下一轮
        xiaozhi_have_answer = false;
        return url;
    }
}

// ===================== 内部实现 =====================

static void xiaozhi_init_i2s()
{
    static bool inited = false; // I2S 是否已完成初始化
    if (inited) return;

    i2s_config_t i2s_config =
    {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = 0,
        .dma_buf_count = 16,
        .dma_buf_len = 60,
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

    const i2s_pin_config_t pin_config =
    {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    i2s_set_pin(I2S_PORT, &pin_config);

    inited = true;
}

static int xiaozhi_read_i2s(char *data, int len)
{
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, (char *)data, len, &bytesRead, portMAX_DELAY);
    return (int)bytesRead;
}

static void xiaozhi_get_baidu_token()
{
    String url = "https://aip.baidubce.com/oauth/2.0/token?";
    HTTPClient http;
    http.begin(url + "client_id=" + BAIDU_APP_KEY + "&client_secret=" + BAIDU_SECRET_KEY + "&grant_type=client_credentials");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");

    int httpCode = http.POST("");
    if (httpCode > 0)
    {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        xiaozhi_baidu_access_token = doc["access_token"].as<String>();
    }
}

static void xiaozhi_start_asr()
{
    xiaozhi_get_baidu_token();

    // 构建 START JSON（使用宏中的 APPID / APPKEY）
    xiaozhi_baidu_start_json = "{\"type\": \"START\",\"data\": {\"appid\":" + String(BAIDU_APP_ID) +
                                ",\"appkey\":\"" + String(BAIDU_APP_KEY) +
                                "\",\"dev_pid\": 1537,\"cuid\": \"cuid-1\",\"format\": \"pcm\", \"sample\": 16000}}";

    xiaozhi_ws_client.onMessage(xiaozhi_ws_on_message);
    xiaozhi_ws_client.onEvent(xiaozhi_ws_on_event);

    if (!xiaozhi_ws_client.connect(xiaozhi_baidu_input_url))
    {
        Serial.println("[XIAOZHI] WS connect failed");
        return;
    }

    if (!xiaozhi_ws_client.send(xiaozhi_baidu_start_json))
    {
        Serial.println("[XIAOZHI] send START failed");
        xiaozhi_ws_client.close();
        return;
    }

    xiaozhi_ws_state = 1;
    // xiaozhi_current_text = "";
    // xiaozhi_question_text = "";
    // xiaozhi_have_question = false;
}

static void xiaozhi_stop_asr()
{
    if (xiaozhi_ws_state == 1)
    {
        xiaozhi_ws_client.send(xiaozhi_baidu_end_json);
    }
    xiaozhi_ws_client.close();
    xiaozhi_ws_state = 0;
}

static void xiaozhi_update_ws()
{
    xiaozhi_ws_client.poll();

    if (xiaozhi_ws_state == 1)
    {
        // 控制发送节奏，大约每 160ms 发送一包
        uint32_t now = millis();
        if (now - xiaozhi_last_send_ms < 160)
        {
            return;
        }

        int n = xiaozhi_read_i2s(xiaozhi_i2s_buffer, xiaozhi_i2s_buffer_size);
        if (n > 0)
        {
            xiaozhi_ws_client.sendBinary(xiaozhi_i2s_buffer, n);
            xiaozhi_last_send_ms = now;
        }
    }
}

static void xiaozhi_ws_on_message(WebsocketsMessage message)
{
    String data = message.data();

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, data);
    if (error)
    {
        if (XIAOZHI_LOG_ENABLE)
        {
            Serial.print("[XIAOZHI] JSON error: ");
            Serial.println(error.c_str());
        }
        return;
    }

    if (XIAOZHI_LOG_ENABLE)
    {
        // 打印原始报文，便于调试
        Serial.print("[XIAOZHI] RAW: ");
        Serial.println(data);
    }

    const char *type = doc["type"] | "";

    // 只有当返回里明确带有 err_no 且不为 0 时才认为出错
    if (doc.containsKey("err_no"))
    {
        int32_t err_no = doc["err_no"].as<int32_t>();
        if (err_no != 0)
        {
            if (XIAOZHI_LOG_ENABLE)
            {
                Serial.print("[XIAOZHI] ASR err_no=");
                Serial.println(err_no);
            }
            xiaozhi_stop_asr();
            return;
        }
    }

    if (strcmp(type, "MID_TEXT") == 0 || strcmp(type, "FIN_TEXT") == 0)
    {
        xiaozhi_current_text = doc["result"].as<String>();
        if (XIAOZHI_LOG_ENABLE)
        {
            Serial.print("[XIAOZHI] text: ");
            Serial.println(xiaozhi_current_text);
        }

        xiaozhi_question_text = xiaozhi_current_text; 
        xiaozhi_have_question = true;                 

        if (strcmp(type, "FIN_TEXT") == 0)
        {
            // 一句话结束：关闭 ASR，并触发 AI + TTS
            xiaozhi_stop_asr();

            // 调用用户提供的 AI 宏函数，得到答案文本
            String ai_answer = xiaozhi_ai(xiaozhi_question_text);

            // 记录并打印 AI 完整回答
            xiaozhi_answer_text = ai_answer;
            if (XIAOZHI_LOG_ENABLE)
            {
                Serial.print("[XIAOZHI] AI ANSWER: ");
                Serial.println(xiaozhi_answer_text);
            }

            // 直接将整段回答作为一次 TTS 文本
            xiaozhi_tts_from_text(xiaozhi_answer_text);
        }
    }
}

static void xiaozhi_ws_on_event(WebsocketsEvent event, String data)
{
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        if (XIAOZHI_LOG_ENABLE)
        {
            Serial.println("[XIAOZHI] WS opened");
        }
    }
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        if (XIAOZHI_LOG_ENABLE)
        {
            Serial.println("[XIAOZHI] WS closed");
        }
    }
}

// 简单 URL 编码，复用 Arduino 提供的 urlEncode
static String xiaozhi_url_encode(const String &s)
{
    return urlEncode(s);
}

static void xiaozhi_tts_from_text(const String &text)
{
    if (!text.length())
    {
        return;
    }

    String encoded = xiaozhi_url_encode(xiaozhi_url_encode(text));
    String tts_url = "http://tsn.baidu.com/text2audio?tex=" + encoded +
                     "&lan=zh&cuid=cuid-1&ctp=1&per=5977&tok=" + xiaozhi_baidu_access_token;

    xiaozhi_answer_host = tts_url;
    xiaozhi_have_answer = true;

    if (XIAOZHI_LOG_ENABLE)
    {
        Serial.print("[XIAOZHI] TTS url: ");
        Serial.println(tts_url);
    }
}
