#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <time.h> // 時刻取得用
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"          // MACアドレスなどの機密情報
#include "wol.h"              // WoL送信関数

// --- 静的IPアドレスの設定 ---
// ご自身のネットワーク環境に合わせて変更してください
IPAddress local_IP(192, 168, 223, 90); // ESP8266に割り当てるIPアドレス
IPAddress gateway(192, 168, 223, 1);    // ルーターのIPアドレス
IPAddress subnet(255, 255, 255, 0);   // サブネットマスク
IPAddress primaryDNS(8, 8, 8, 8);     // (オプション) プライマリDNS
IPAddress secondaryDNS(8, 8, 4, 4);   // (オプション) セカンダリDNS

// スイッチが接続されているピン
#define SWITCH_PIN 5 // D1ピンを使用

// DHTセンサーのピン定義とタイプ定義
#define DHTPIN 2     // D4ピンに接続
#define DHTTYPE DHT11  // 使用するセンサーのタイプ (DHT11)

// MCUの自己発熱による温度上昇を補正するためのオフセット値 (℃)
// 正確な値は、信頼できる温度計と比較して調整してください。
#define TEMP_OFFSET -1.8

// I2Cピンの定義 (OLEDディスプレイ用)
#define I2C_SDA 4  // D2
#define I2C_SCL 14 // D5

// OLEDディスプレイの定義
#define SCREEN_WIDTH 128 // OLEDの幅 (ピクセル)
#define SCREEN_HEIGHT 64 // OLEDの高さ (ピクセル)
#define OLED_RESET    -1 // リセットピン (-1はArduinoのリセットピンを共有)

// NTPサーバーとタイムゾーンの設定 (JST: 日本標準時)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 9 * 3600; // 9時間 (秒単位)
const int   daylightOffset_sec = 0;   // 夏時間なし

// DHTセンサーのオブジェクトを作成
DHT dht(DHTPIN, DHTTYPE);
// OLEDディスプレイのオブジェクトを作成
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  // シリアル通信を初期化
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // シリアルポートが接続されるのを待つ
  }
  
  // スイッチのピンを入力モードに設定 (内蔵プルアップ抵抗を有効化)
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  Serial.println(F("Booting..."));

  // 起動時に画面をクリアし、接続中メッセージを表示
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // 先に初期化
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F("Connecting to WiFi..."));
  display.display();

  // 静的IPアドレスを設定
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    display.println(F("IP config failed")); // OLEDにも表示
    display.display();
  }

  // Wi-Fiに接続
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // I2C通信を初期化 (ピンを指定)
  Wire.begin(I2C_SDA, I2C_SCL);

  // SSD1306ディスプレイを初期化 (I2Cアドレス 0x3C)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // 失敗した場合は無限ループ
  }
  Serial.println(F("SSD1306 Initialized."));

  // NTPによる時刻同期を開始
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // DHTセンサーを初期化
  dht.begin();

  // 起動時に画面をクリア
  display.clearDisplay();
  display.display();
  Serial.println("---------------------------------");
}

void loop() {
  // スイッチが押されたかチェック (押されるとLOWになる)
  if (digitalRead(SWITCH_PIN) == LOW) {
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

  // センサーからの読み取りには250ms以上かかるため、2秒待機します。
  delay(2000);

  // 湿度と温度を読み取る
  float humidity = dht.readHumidity();
  // 温度を摂氏で読み取る
  float temperature = dht.readTemperature();

  // 時刻情報を取得
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    // 時刻取得失敗時はOLEDに表示しない
  }
  char timeStr[9]; // HH:MM:SS 形式 (8文字 + NULL終端)
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  // 読み取りが成功したかチェック (失敗するとNaNを返します)
  if (isnan(humidity) || isnan(temperature)) {
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

  // --- OLEDディスプレイに結果を出力 ---
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); // 文字色を白に設定

  // 1行目: 時刻 (大きめの文字)
  display.setTextSize(2);
  display.setCursor(12, 0); // 中央寄りに配置
  display.println(timeStr);

  // 下段: 温度と湿度
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print(temperature, 1); display.print((char)247); display.print("C ");
  display.print(humidity, 0); display.print("%");

  display.display(); // 画面に描画
}