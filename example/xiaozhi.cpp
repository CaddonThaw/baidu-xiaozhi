#include "xiaozhi_ai.h"
#include <WiFi.h>

void setup() {
    Serial.begin(115200);

    WiFi.begin("ssid", "password");
    while(WiFi.status()!= WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");


    xiaozhi_init();
}

void loop() {
    xiaozhi_loop();

    // 检测是否听到问题
    if (xiaozhi_listen()) {
        String question = xiaozhi_question();
        Serial.print("XIAOZHI QUESTION: "); // 打印问题
        Serial.println(question);
    }

    // 检测是否合成答案音频
    if (xiaozhi_speak()) {
        String answer_url = xiaozhi_answer();
        Serial.print("XIAOZHI ANSWER URL: ");
        Serial.println(answer_url);         // 打印答案URL

        // play_from_url(answer_url); 
    }
}