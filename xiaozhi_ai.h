#ifndef XIAOZHI_AI_H
#define XIAOZHI_AI_H

#include <Arduino.h>
#include "xiaozhi_minimax.h"

// 日志开关：1 打印调试信息，0 关闭所有 xiaozhi 日志
#ifndef XIAOZHI_LOG_ENABLE
#define XIAOZHI_LOG_ENABLE 0
#endif

#ifndef BAIDU_APP_ID
#define BAIDU_APP_ID      "APP ID" // 默认百度语音 APP ID
#endif

#ifndef BAIDU_APP_KEY
#define BAIDU_APP_KEY     "API KEY" // 默认百度语音 API KEY
#endif

#ifndef BAIDU_SECRET_KEY
#define BAIDU_SECRET_KEY  "SECRET KEY" // 默认百度语音 SECRET KEY
#endif

// I2S 引脚和参数，如果你的硬件不同，可在包含本头文件前重新 #define 覆盖
#ifndef I2S_WS
#define I2S_WS           13  // 默认 I2S LRCLK 引脚
#endif

#ifndef I2S_SD
#define I2S_SD           12  // 默认 I2S DIN 引脚
#endif

#ifndef I2S_SCK
#define I2S_SCK          11  // 默认 I2S BCLK 引脚
#endif

#ifndef I2S_PORT
#define I2S_PORT         I2S_NUM_0  // 默认 I2S 端口
#endif

#ifndef I2S_SAMPLE_RATE
#define I2S_SAMPLE_RATE  (16000)    // 默认采样率 16kHz
#endif

#ifndef I2S_SAMPLE_BITS
#define I2S_SAMPLE_BITS  (16)       // 默认位宽 16bit
#endif

// 可自行提供 AI 回答函数宏：
#ifndef xiaozhi_ai
#define xiaozhi_ai(q) minimax_answer(q)        
#endif

// ===================== 对外 API =====================

// 初始化小智：在 setup() 中调用一次，保证 I2S/内部状态就绪
void xiaozhi_init();

// 在 loop() 中周期调用，处理 WebSocket 轮询与音频推送
void xiaozhi_loop();

// 进入监听流程，如果已有一句完整文本可读则返回 true
bool xiaozhi_listen();

// 返回识别到的一句话文本，读取后内部标志会清零
String xiaozhi_question();

// 如果当前已经获得答案并完成 TTS，返回 true
bool xiaozhi_speak();

// 0返回答案文本
// 1返回答案音频的 URL（百度语音合成 host），读取后内部标志会清零
String xiaozhi_answer(bool mode);

#endif // XIAOZHI_H