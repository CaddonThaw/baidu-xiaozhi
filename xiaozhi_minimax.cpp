#include "xiaozhi_minimax.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

String url = "https://api.minimax.chat/v1/text/chatcompletion_v2";

String minimax_answer(String question)
{
    HTTPClient http;
    http.setTimeout(20000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", MiniMaxKey);

    String data = "{\"model\":\"abab5.5s-chat\",\"messages\":[{\"role\": \"system\",\"content\": \"你叫小智,要求下面的回答严格控制在256字符以内.\"},{\"role\": \"user\",\"content\": \"" + question + "\"}]}";
    int httpResponseCode = http.POST(data);

    if (httpResponseCode == 200)
    {
        String response = http.getString();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, response);

        if(error) 
        {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return "<error>";
        }

        String answer = doc["choices"][0]["message"]["content"].as<String>();
        http.end();
        return answer;
    } 
    else
    {
        Serial.print("HTTP POST request failed, error code: ");
        Serial.println(httpResponseCode);
        http.end();
        return "<error>";
    }
}