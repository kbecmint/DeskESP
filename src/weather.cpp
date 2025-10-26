#include "weather.h"
#include "secrets.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

RainInfo parseYahooWeatherJson(const String &payload)
{
  RainInfo rainInfo = {false, 0, 0.0, ""};
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    rainInfo.statusMessage = "JSON Parse Error";
    return rainInfo;
  }

  JsonArray weatherList = doc["Feature"][0]["Property"]["WeatherList"]["Weather"].as<JsonArray>();
  if (weatherList.isNull() || weatherList.size() == 0)
  {
    rainInfo.statusMessage = "WeatherList is empty";
    return rainInfo;
  }

  // --- デバッグ用: 受信したWeatherListをシリアルに出力 ---
  Serial.println(F("--- Received WeatherList ---"));
  serializeJson(doc["Feature"][0]["Property"]["WeatherList"]["Weather"], Serial);
  Serial.println(); // 見やすいように改行を追加
  Serial.println(F("----------------------------"));

  Serial.println("--- Precipitation Forecast (10-60 min) ---");
  String firstDateStr = weatherList[0]["Date"].as<String>();

  // 基準となる時刻を分単位で計算
  int firstHour = firstDateStr.substring(8, 10).toInt();
  int firstMinute = firstDateStr.substring(10, 12).toInt();
  int firstTotalMinutes = firstHour * 60 + firstMinute;

  // 最初の雨が降る時間を探す
  for (JsonObject weather : weatherList)
  {
    // 降水量は小数点を含むためfloatで取得する
    float rainFall = weather["Rainfall"];
    String currentDateStr = weather["Date"].as<String>();
    int currentHour = currentDateStr.substring(8, 10).toInt();
    int currentMinute = currentDateStr.substring(10, 12).toInt();
    int minutes = (currentHour * 60 + currentMinute) - firstTotalMinutes;

    // 10分後から60分後の予報をシリアルに出力
    if (minutes >= 10 && minutes <= 60)
    {
      Serial.printf("  %d min later: %.2f mm/h\n", minutes, rainFall);
    }

    // 最初に雨が降る時間を見つける (0mmより大きい場合)
    // まだ雨が降ると判定されていない場合のみチェック
    if (!rainInfo.willRain && rainFall > 0)
    {
      rainInfo.willRain = true;
      rainInfo.minutesUntilRain = minutes;
      rainInfo.rainfall = rainFall;
      // break; // 最初の雨を見つけたらループを抜ける -> 全ての予報を出力するためにコメントアウト
    }
  }
  rainInfo.statusMessage = rainInfo.willRain ? "Rain approaching!" : "No rain expected.";
  return rainInfo;
}

RainInfo checkRainCloud()
{
  RainInfo rainInfo = {false, 0, 0.0, ""};

  // HTTPS通信のためにWiFiClientSecureを使用
  WiFiClientSecure client;
  // Yahoo APIはドメインが固定なので、証明書の検証をスキップしてもリスクは比較的低い
  client.setInsecure();
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

  if (http.begin(client, url))
  {
    int httpCode = http.GET();

    if (httpCode > 0)
    {
      if (httpCode == HTTP_CODE_OK)
      {
        rainInfo = parseYahooWeatherJson(http.getString());
      }
      else
      {
        rainInfo.statusMessage = "HTTP Error: " + String(httpCode);
      }
    }
    else
    {
      rainInfo.statusMessage = "HTTP GET failed";
    }
    http.end();
  }
  else
  {
    rainInfo.statusMessage = "HTTP connection failed";
  }

  Serial.println(rainInfo.statusMessage);
  return rainInfo;
}