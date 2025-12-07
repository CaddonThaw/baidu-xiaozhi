# baidu-xiaozhi
基于百度在线实时语音识别、百度在线短语音合成和MiniMax聊天模型的小智AI对话功能，适配于ESP32S3智能终端

## 安装库
- 在 `platformio.ini` 中添加以下内容：
    ```
    lib_deps =
        bblanchon/ArduinoJson@^6.21.4
        plageoj/UrlEncode@^1.0.1
        gilmaimon/ArduinoWebsockets@^0.5.3
        https://github.com/CaddonThaw/baidu-xiaozhi.git
    ```

## 运行小智示例
- 打开 `xiaozhi_ai.h` 头文件，修改 `APP_ID` `API_KEY` `SECRET KEY` 为你的百度 API 信息；
- 打开 `xiaozhi_minimax.h` 头文件，修改 `MiniMaxKey` 为你的 MiniMax API Key；
- 打开 `examples/xiaozhi.cpp` 目录，修改 `ssid` 和 `password` 为你的 WiFi 账号和密码：
    ```C
    #include "xiaozhi_ai.h"
    #include <WiFi.h>

    void setup() {
        Serial.begin(115200);

        WiFi.begin("ssid", "password");
        while(WiFi.status()!= WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println("WiFi connected");

        xiaozhi_init();
    }

    void loop() {
        xiaozhi_loop();

        // check if there is a new question
        if (xiaozhi_listen()) 
        {
            // get the question text
            String question_text = xiaozhi_question();
            Serial.println(question_text);
        }

        // check if there is a new answer
        if (xiaozhi_speak()) 
        {
            // get the answer text 
            String answer_text = xiaozhi_answer(0);
            Serial.println(answer_text);

            // get the answer url for audio
            String answer_url = xiaozhi_answer(1);
            Serial.println(answer_url);
        }
    }
    ```
