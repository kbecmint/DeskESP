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
#include "secrets.h" // MACアドレスなどの機密情報
#include "wol.h"     // WoL送信関数
#include "weather.h" // 天気情報取得関数

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

// --- データPOST関連の設定 ---
const int ROOM_ID = 13; // 部屋のID (定数)
unsigned long lastPostTime = 0;
// 10分 (ミリ秒)
const long postInterval = 10 * 60 * 1000;

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

  // 静的IPアドレスを設定
  // これにより、DHCP関連の問題を回避できる可能性があります
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("STA Failed to configure");
    // 画面にも設定失敗を表示しても良い
  }

  // Wi-Fiに接続
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 28);
  display.print("Connecting to WiFi");
  display.display();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
  }
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

  RainInfo rainInfo = checkRainCloud();
  isRainingSoon = rainInfo.willRain;
  rainTime = rainInfo.minutesUntilRain;
  rainAmount = rainInfo.rainfall;
  lastWeatherCheck = millis(); // 次の定期チェックタイマーをリセット
  delay(1000);                 // メッセージを少し表示

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
  display.setTextSize(1);
  display.setCursor(0, 56);
  // isRainingSoonフラグに応じて文字色を切り替える
  if (isRainingSoon) // 雨が降る/降っている場合
  {
    // 点滅状態がtrueのときだけ描画する
    if (rainWarningBlinkState)
    {
      // 雨が近い場合は文字色を反転（黒文字、白背景）させて強調
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.print("Rain in ");
      if (rainTime == 0)
      {
        display.printf("now! %.1fmm", rainAmount);
      }
      else
      {
        // 表示が切り替わる際に前の表示が残らないよう、空白で埋める
        display.printf("%dmin %.1fmm  ", rainTime, rainAmount);
      }
    }
  }
  else // 雨が降らない場合
  {
    // 通常時は白文字
    display.setTextColor(SSD1306_WHITE);
    display.print("No rain for 1hr");
  }
}

// センサーデータをサーバーにPOSTする関数
void postSensorData(float temp, float hum)
{
  if (WiFi.status() == WL_CONNECTED)
  {
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

    int httpResponseCode = http.POST(jsonPayload);

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
  }
}

void loop()
{
  // スイッチが押されたかチェック (押されるとLOWになる)
  if (digitalRead(SWITCH_PIN) == LOW)
  {
    Serial.println("Switch pressed. Sending WoL packet...");

    // OLEDに送信中メッセージを表示
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 24);
    display.println(F("Sending"));
    display.println(F("  WoL..."));
    display.display();

    // 自作関数でWoLマジックパケットを送信
    sendWolPacket(MAC_ADDRESS);

    delay(2000); // メッセージを2秒間表示
  }

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
      postSensorData(temp, hum);
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

  // --- 点滅処理 ---
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
    RainInfo rainInfo = checkRainCloud();
    isRainingSoon = rainInfo.willRain;
    rainTime = rainInfo.minutesUntilRain;
    rainAmount = rainInfo.rainfall;
  }

  // 湿度と温度を読み取る
  float humidity = dht.readHumidity();
  // 温度を摂氏で読み取る
  float temperature = dht.readTemperature();

  char timeStr[9]; // HH:MM:SS 形式 (8文字 + NULL終端)
  // 時刻情報を取得
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    // 時刻取得失敗時は、表示を"--:--:--"に設定
    strcpy(timeStr, "--:--:--");
  }
  strftime(timeStr, sizeof(timeStr), "%T", &timeinfo); // %T は %H:%M:%S と同じ

  // 読み取りが成功したかチェック (失敗するとNaNを返します)
  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // 温度オフセットを適用
  temperature = temperature + TEMP_OFFSET;

  // --- シリアルモニタに結果を出力 ---
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.println(F(" *C"));
  Serial.println();

  // 10分ごとにセンサーデータをPOST
  // unsigned long currentMillis = millis(); // 上で定義済みのためコメントアウト
  if (currentMillis - lastPostTime >= postInterval)
  {
    lastPostTime = currentMillis;
    // NaNチェックをしてからPOST
    if (!isnan(humidity) && !isnan(temperature))
    {
      postSensorData(temperature, humidity);
    }
  }

  // --- OLEDディスプレイに結果を出力 ---
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); // 文字色を白に設定

  // 1行目: 時刻 (大きめの文字)
  display.setTextSize(2);
  display.setCursor(12, 0); // 中央寄りに配置
  display.println(timeStr);

  // 2行目: 次のPOSTまでの残り時間
  unsigned long remainingMillis = postInterval - (currentMillis - lastPostTime);
  // 投稿直後は大きな値になることがあるため、インターバルを超えないように補正
  if (remainingMillis > postInterval)
  {
    remainingMillis = postInterval;
  }
  int remainingMinutes = remainingMillis / 1000 / 60;
  int remainingSeconds = (remainingMillis / 1000) % 60;
  display.setTextSize(1);
  display.setCursor(0, 18);
  display.printf("Next POST: %02d:%02d", remainingMinutes, remainingSeconds);

  // 下段: 温度と湿度
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print(temperature, 1);
  display.print((char)247);
  display.print("C ");
  display.print(humidity, 0);
  display.print("%");

  // 雨雲情報を表示
  drawRainWarning();

  display.display(); // 画面に描画

  delay(1000); // ループの負荷を軽減
}