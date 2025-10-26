#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <time.h>              // 時刻取得用
#include <ESP8266HTTPClient.h> // HTTP通信用
#include <ArduinoJson.h>       // JSON作成用
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"      // MACアドレスなどの機密情報
#include "wol.h"          // WoL送信関数
#include "weather.h"      // 天気情報取得関数
#include "wifi_handler.h" // WiFi接続ハンドラ

// --- 静的IPアドレスの設定 ---
// ご自身のネットワーク環境に合わせて変更してください
IPAddress local_IP(192, 168, 223, 90); // ESP8266に割り当てるIPアドレス
IPAddress gateway(192, 168, 223, 1);   // ルーターのIPアドレス
IPAddress subnet(255, 255, 255, 0);    // サブネットマスク
IPAddress primaryDNS(8, 8, 8, 8);      // (オプション) プライマリDNS
IPAddress secondaryDNS(8, 8, 4, 4);    // (オプション) セカンダリDNS

// スイッチが接続されているピン
#define SWITCH_PIN 5       // D1ピンを使用
#define FLASH_BUTTON_PIN 0 // FlashボタンはGPIO0

// DHTセンサーのピン定義とタイプ定義
#define DHTPIN 2      // D4ピンに接続
#define DHTTYPE DHT11 // 使用するセンサーのタイプ (DHT11)

// MCUの自己発熱による温度上昇を補正するためのオフセット値 (℃)
// 正確な値は、信頼できる温度計と比較して調整してください。
#define TEMP_OFFSET -1.8

// I2Cピンの定義 (OLEDディスプレイ用)
#define I2C_SDA 4  // D2
#define I2C_SCL 14 // D5

// OLEDディスプレイの定義
#define SCREEN_WIDTH 128 // OLEDの幅 (ピクセル)
#define SCREEN_HEIGHT 64 // OLEDの高さ (ピクセル)
#define OLED_RESET -1    // リセットピン (-1はArduinoのリセットピンを共有)

// NTPサーバーとタイムゾーンの設定 (JST: 日本標準時)
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600; // 9時間 (秒単位)
const int daylightOffset_sec = 0;    // 夏時間なし

// --- スイッチ処理関連の定数と変数 ---
const long LONG_PRESS_TIME = 1000; // 長押しと判断する時間 (ms)

int switchState;
int lastSwitchState = HIGH;
unsigned long pressStartTime = 0;
bool isPressing = false;
bool longPressHandled = false;

bool isDisplayOn = true; // 画面の表示状態を管理

// --- データPOST関連の設定 ---
const int ROOM_ID = 13; // 部屋のID (定数)
unsigned long lastPostTime = 0;
// 10分 (ミリ秒)
const long postInterval = 10 * 60 * 1000;

// POST結果表示用の変数
int lastPostResult = 0; // 0:未実行, >0:HTTPコード, <0:クライアントエラー
unsigned long postResultDisplayStart = 0;
const long postResultDisplayDuration = 5000; // 5秒間表示

// DHTセンサーのオブジェクトを作成
DHT dht(DHTPIN, DHTTYPE);
// OLEDディスプレイのオブジェクトを作成
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 天気情報更新用の変数
unsigned long lastWeatherCheck = 0;
const long weatherCheckInterval = 1 * 60 * 1000; // 1分 (ミリ秒)

bool isRainingSoon = false;
int rainTime = 0;
float rainAmount = 0.0;
bool rainWarningBlinkState = true; // 雨予報の点滅表示用

void setup()
{
  // シリアル通信を初期化
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10); // シリアルポートが接続されるのを待つ
  }

  // スイッチのピンを入力モードに設定 (内蔵プルアップ抵抗を有効化)
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(FLASH_BUTTON_PIN, INPUT_PULLUP);

  Serial.println(F("Booting..."));

  // I2C通信とOLEDディスプレイを先に初期化
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // 失敗した場合は無限ループ
  }
  Serial.println(F("SSD1306 Initialized."));

  // 起動メッセージをOLEDに表示
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(18, 24); // 中央に配置
  display.println(F("Booting.."));
  display.display(); // ここで一度表示

  // Wi-Fiに接続
  ensureWiFiConnected(&display);
  Serial.println(" Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // NTPによる時刻同期を開始
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // 起動時に天気情報を取得
  Serial.println("\nChecking for rain clouds at startup...");
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 28);
  display.print("Checking weather...");
  display.display();

  if (ensureWiFiConnected(&display))
  {
    RainInfo rainInfo = checkRainCloud();
    isRainingSoon = rainInfo.willRain;
    rainTime = rainInfo.minutesUntilRain;
    rainAmount = rainInfo.rainfall;
    lastWeatherCheck = millis(); // 次の定期チェックタイマーをリセット
    delay(1000);                 // メッセージを少し表示
  }

  // DHTセンサーを初期化
  dht.begin();

  // 起動時に画面をクリア
  display.clearDisplay();
  display.display();
  Serial.println("---------------------------------");
}

// 雨雲接近の通知を描画する関数
void drawRainWarning()
{
  // isRainingSoonフラグに応じて文字色を切り替える
  if (isRainingSoon) // 雨が降る/降っている場合
  {
    display.setTextSize(2);
    display.setCursor(0, 48);
    // 点滅状態がtrueのときだけ描画する
    if (rainWarningBlinkState)
    {
      // 雨が近い場合は文字色を反転（黒文字、白背景）させて強調
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      if (rainTime == 0)
      {
        display.printf("Rain:%.1fmm", rainAmount);
      }
      else
      {
        // 表示が切り替わる際に前の表示が残らないよう、空白で埋める
        display.printf("%dmin %.1fmm", rainTime, rainAmount);
      }
    }
  }
  else // 雨が降らない場合
  {
    display.setTextSize(1);
    display.setCursor(0, 56);
    // 通常時は白文字
    display.setTextColor(SSD1306_WHITE);
    display.print("No rain for 1 hour");
  }
}

// センサーデータをサーバーにPOSTする関数
int postSensorData(float temp, float hum)
{
  int httpResponseCode = 0;
  if (ensureWiFiConnected(&display))
  {
    // 接続が確認できたので処理を続行
    // HTTPS通信のためにWiFiClientSecureを使用
    WiFiClientSecure client;
    // ★★★ サーバー証明書の検証をスキップ（デバッグ用） ★★★
    // 本番環境では、setFingerprintやsetCACertで検証することを推奨します
    client.setInsecure();

    HTTPClient http;

    // JSONドキュメントを作成
    // ArduinoJson v7以降では、サイズ指定のないJsonDocumentを使用します
    JsonDocument doc;
    doc["room"] = ROOM_ID;
    doc["temp"] = temp;
    doc["hum"] = hum;
    doc["atm"] = nullptr; // atmはnull固定

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    Serial.println("Posting sensor data...");
    Serial.println(jsonPayload);

    // HTTP POSTリクエストを開始
    http.begin(client, POST_URL);
    http.addHeader("Content-Type", "application/json");
    // User-Agentを一般的なブラウザに偽装して、サーバー側のブロックを回避する
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36");

    httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0)
    {
      String response = http.getString();
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println(response);
    }
    else
    {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected. Cannot post data.");
    httpResponseCode = -1; // WiFi未接続エラー
  }
  return httpResponseCode;
}

/**
 * @brief スイッチの状態をチェックし、長押し/短押しを処理します。
 */
void handleSwitch()
{
  switchState = digitalRead(SWITCH_PIN);

  // スイッチが押された瞬間
  if (switchState == LOW && lastSwitchState == HIGH)
  {
    pressStartTime = millis();
    isPressing = true;
    longPressHandled = false;
  }
  // スイッチが離された瞬間
  else if (switchState == HIGH && lastSwitchState == LOW)
  {
    if (isPressing && !longPressHandled)
    {
      // --- 短押し (Short Press) の処理 ---
      if (isDisplayOn)
      {
        // 画面がONの時 -> WoLパケットを送信
        Serial.println("Switch short pressed. Sending WoL packet...");

        // WoL送信前にWiFi接続を確認・復旧
        if (!ensureWiFiConnected(&display))
          return;

        // OLEDに送信中メッセージを表示
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 24);
        display.println(F("Sending"));
        display.println(F("  WoL..."));
        display.display();

        sendWolPacket(MAC_ADDRESS);
        delay(2000); // メッセージを2秒間表示
      }
      else
      {
        // 画面がOFFの時 -> 画面をONにする
        isDisplayOn = true;
        display.ssd1306_command(SSD1306_DISPLAYON);
        Serial.println("Display ON");
      }
    }
    isPressing = false;
  }

  // スイッチが押されている間の処理
  if (isPressing && !longPressHandled)
  {
    if (millis() - pressStartTime > LONG_PRESS_TIME)
    {
      // --- 長押し (Long Press) の処理 ---
      if (isDisplayOn)
      {
        // 画面がONの時 -> 画面をOFFにする
        isDisplayOn = false;
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        Serial.println("Display OFF");
      }
      longPressHandled = true; // 長押し処理が完了したことをマーク
    }
  }

  lastSwitchState = switchState;
}

void loop()
{
  // D1ピンに接続されたスイッチの処理
  handleSwitch();

  // Flashボタンが押されたかチェック (手動POST)
  if (digitalRead(FLASH_BUTTON_PIN) == LOW)
  {
    Serial.println("Flash button pressed. Manual POST triggered...");

    // センサー値を読み取る
    float hum = dht.readHumidity();
    float temp = dht.readTemperature();

    // 読み取りが成功した場合のみPOST
    if (!isnan(hum) && !isnan(temp))
    {
      // 温度オフセットを適用
      temp = temp + TEMP_OFFSET;
      lastPostResult = postSensorData(temp, hum);
      postResultDisplayStart = millis(); // 結果表示の開始時刻を記録

      // 次の定期POSTまでのタイマーをリセット
      lastPostTime = millis();
    }
    else
    {
      Serial.println(F("Failed to read from DHT sensor! Cannot POST."));
    }

    // ボタンが離されるまで待機 (チャタリング防止)
    delay(50); // 短い遅延
    while (digitalRead(FLASH_BUTTON_PIN) == LOW)
      ;
  }

  // 雨が降る予報の場合、点滅用の状態を切り替える
  if (isRainingSoon)
  {
    rainWarningBlinkState = !rainWarningBlinkState;
  }
  else
  {
    rainWarningBlinkState = true; // 雨が降らない場合は常に表示状態にする
  }

  // 天気情報を定期的にチェック
  unsigned long currentMillis = millis();

  if (currentMillis - lastWeatherCheck >= weatherCheckInterval)
  {
    lastWeatherCheck = currentMillis;
    Serial.println("\nChecking for rain clouds...");
    if (ensureWiFiConnected(&display))
    {
      RainInfo rainInfo = checkRainCloud();
      isRainingSoon = rainInfo.willRain;
      rainTime = rainInfo.minutesUntilRain;
      rainAmount = rainInfo.rainfall;
    }
  }

  // 10分ごとにセンサーデータをPOST
  if (currentMillis - lastPostTime >= postInterval)
  {
    lastPostTime = currentMillis;
    // センサーを読み取り、NaNチェックをしてからPOST
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    if (!isnan(humidity) && !isnan(temperature))
    {
      temperature = temperature + TEMP_OFFSET; // オフセット適用
      lastPostResult = postSensorData(temperature, humidity);
      postResultDisplayStart = millis(); // 結果表示の開始時刻を記録
    }
  }

  // --- シリアルモニタへの定期ログ出力 ---
  // 画面の状態に関わらず、センサー値などをシリアルに出力します。
  // （POST用の読み取りとは別に、デバッグ用に毎秒読み取ります）
  float debug_hum = dht.readHumidity();
  float debug_temp = dht.readTemperature();
  if (!isnan(debug_hum) && !isnan(debug_temp))
  {
    debug_temp = debug_temp + TEMP_OFFSET; // オフセット適用
    Serial.print(F("Humidity: "));
    Serial.print(debug_hum);
    Serial.print(F("%  Temperature: "));
    Serial.print(debug_temp);
    Serial.println(F(" *C"));
  }
  else
  {
    Serial.println(F("Failed to read from DHT sensor for serial log!"));
  }

  // 画面がONのときだけ、描画処理を実行
  if (isDisplayOn)
  {
    // 湿度と温度を読み取る (表示用)
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    char timeStr[9]; // HH:MM:SS 形式 (8文字 + NULL終端)
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time for display");
      strcpy(timeStr, "--:--:--");
    }
    else
    {
      strftime(timeStr, sizeof(timeStr), "%T", &timeinfo); // %T は %H:%M:%S と同じ
    }

    // 読み取りが成功したかチェック
    if (isnan(humidity) || isnan(temperature))
    {
      Serial.println(F("Failed to read from DHT sensor for display!"));
    }
    else
    {
      // 温度オフセットを適用
      temperature = temperature + TEMP_OFFSET;
    }

    // --- OLEDディスプレイに結果を出力 ---
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(12, 0);
    display.println(timeStr);

    unsigned long remainingMillis = postInterval - (currentMillis - lastPostTime);
    if (remainingMillis > postInterval)
      remainingMillis = postInterval;
    int remainingMinutes = remainingMillis / 1000 / 60;
    int remainingSeconds = (remainingMillis / 1000) % 60;
    display.setTextSize(1);
    display.setCursor(0, 18);

    // POST結果の表示ロジック
    bool lastPostFailed = (lastPostResult <= 0 && lastPostResult != 0);
    bool showSuccessMessage = (lastPostResult > 0 && currentMillis - postResultDisplayStart < postResultDisplayDuration);

    if (showSuccessMessage)
    {
      // 成功時は5秒間だけ結果を表示
      display.printf("POST OK (%d)", lastPostResult);
    }
    else if (lastPostFailed)
    {
      // 失敗時は、カウントダウンの横に失敗コードを表示し続ける
      display.printf("Post in: %02d:%02d (F:%d)", remainingMinutes, remainingSeconds, lastPostResult);
    }
    else
    {
      // 通常時はカウントダウンのみ表示
      display.printf("Post in: %02d:%02d", remainingMinutes, remainingSeconds);
    }

    display.setTextSize(2);
    display.setCursor(0, 30);
    display.print(temperature, 1);
    display.print((char)247);
    display.print("C ");
    display.print(humidity, 0);
    display.print("%");

    drawRainWarning();
    display.display();
  }

  delay(1000); // ループの負荷を軽減
}