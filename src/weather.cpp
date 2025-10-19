#include "weather.h"
#include "secrets.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

RainInfo checkRainCloud() {
  RainInfo rainInfo = {false, 0, ""};

  if (WiFi.status() != WL_CONNECTED) {
    rainInfo.statusMessage = "WiFi not connected";
    return rainInfo;
  }

  WiFiClient client;
  HTTPClient http;

  // APIエンドポイントのURLを構築
  String url = "https://map.yahooapis.jp/weather/V1/place?coordinates=";
  url += LONGITUDE;
  url += ",";
  url += LATITUDE;
  url += "&appid=";
  url += YAHOO_APP_ID;
  url += "&output=json&interval=5"; // 5分間隔のデータを取得

  Serial.print("Requesting URL: ");
  Serial.println(url);

  if (http.begin(client, url)) {
    int httpCode = http.GET();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        // JSONをパース
        // メモリ不足を避けるため、スタック上に静的にメモリを確保します。
        // APIの応答サイズに応じて、この値は調整が必要になる場合があります。
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          rainInfo.statusMessage = "JSON Parse Error";
        } else {
          // 60分後までの降水強度をチェック
          JsonArray weatherList = doc["Feature"][0]["Property"]["WeatherList"]["Weather"];
          for (JsonObject weather : weatherList) {
            int rainFall = weather["Rainfall"]; // 降水強度
            int minutes = weather["Date"].as<String>().substring(8).toInt() - doc["Feature"][0]["Property"]["WeatherList"]["Weather"][0]["Date"].as<String>().substring(8).toInt();
            minutes = (minutes / 100) * 60 + (minutes % 100);

            if (rainFall > 0) {
              rainInfo.willRain = true;
              rainInfo.minutesUntilRain = minutes;
              rainInfo.statusMessage = "Rain approaching!";
              break; // 雨を見つけたらループを抜ける
            }
          }
          if (!rainInfo.willRain) {
            rainInfo.statusMessage = "No rain expected.";
          }
        }
      } else {
        rainInfo.statusMessage = "HTTP Error: " + String(httpCode);
      }
    } else {
      rainInfo.statusMessage = "HTTP GET failed";
    }
    http.end();
  } else {
    rainInfo.statusMessage = "HTTP connection failed";
  }

  Serial.println(rainInfo.statusMessage);
  return rainInfo;
}