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