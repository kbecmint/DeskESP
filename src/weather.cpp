#include "weather.h"
#include "secrets.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

RainInfo parseYahooWeatherJson(JsonDocument &doc)
{
  RainInfo rainInfo = {false, 0, 0.0, ""};

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
  // 日付文字列(YYYYMMDDHHmm)を数値として取得し、メモリ効率を改善
  long long firstDateNum = weatherList[0]["Date"].as<long long>();

  // 基準となる時刻を分単位で計算
  // (firstDateNum / 100) % 100 -> HH (時)
  // firstDateNum % 100 -> mm (分)
  int firstHour = (firstDateNum / 100) % 100;
  int firstMinute = firstDateNum % 100;
  int firstTotalMinutes = firstHour * 60 + firstMinute;

  // 最初の雨が降る時間を探す
  for (JsonObject weather : weatherList)
  {
    // 降水量は小数点を含むためfloatで取得する
    float rainFall = weather["Rainfall"];
    long long currentDateNum = weather["Date"].as<long long>();
    int currentHour = (currentDateNum / 100) % 100;
    int currentMinute = currentDateNum % 100;
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

  HTTPClient http;
  // WiFiClientSecureはスコープを抜けるときに自動的にリソースを解放します
  std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);

  if (!client)
  {
    rainInfo.statusMessage = "Out of memory";
    Serial.println(rainInfo.statusMessage);
    return rainInfo;
  }

  client->setInsecure(); // 証明書の検証をスキップ

  // APIエンドポイントのURLを構築
  // Stringの連結はメモリの断片化を引き起こすため、snprintfを使用してURLを構築する
  char url[256];
  snprintf(url, sizeof(url),
           "https://map.yahooapis.jp/weather/V1/place?coordinates=%s,%s&appid=%s&output=json&interval=5",
           LONGITUDE, LATITUDE, YAHOO_APP_ID);

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // --- DNS名前解決のテスト ---
  IPAddress resolvedIP;
  const char *host = "map.yahooapis.jp";
  Serial.printf("Resolving DNS for %s... ", host);
  if (!WiFi.hostByName(host, resolvedIP))
  {
    Serial.println("DNS lookup failed!");
    rainInfo.statusMessage = "DNS lookup failed";
    return rainInfo;
  }
  Serial.printf("OK. IP: %s\n", resolvedIP.toString().c_str());

  // --- 通信直前のシステム状態をログ出力 ---
  Serial.printf("[Pre-GET] Free Heap: %u bytes, WiFi Status: %d, RSSI: %d dBm\n", ESP.getFreeHeap(), WiFi.status(), WiFi.RSSI());

  if (http.begin(*client, url))
  {
    int httpCode = http.GET();

    if (httpCode > 0)
    {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        // チャンク形式のレスポンスを確実に処理するため、一度Stringに全データを受信してから解析する。
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
          rainInfo.statusMessage = "JSON Parse Error";
        else
          rainInfo = parseYahooWeatherJson(doc);
      }
      else
      {
        rainInfo.statusMessage = "HTTP GET Error: " + String(httpCode);
      }
    }
    else
    {
      // GETリクエスト失敗時の詳細なエラーを取得
      rainInfo.statusMessage = http.errorToString(httpCode).c_str();
    }
    http.end();
  }
  else
  {
    rainInfo.statusMessage = "HTTP begin failed";
  }

  Serial.println(rainInfo.statusMessage);
  return rainInfo;
}