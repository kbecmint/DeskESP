# ESP8266 多機能デスクガジェット

ESP8266を使用した、時刻、温湿度、天気予報を表示し、Wake-on-LAN (WoL) パケット送信機能も備えた多機能なデスクガジェットです。

## 主な機能

- **情報表示**:
  - 現在時刻 (NTPサーバーから取得)
  - 温度・湿度 (DHT11センサー)
  - 1時間以内の降雨予報 (Yahoo!天気API)
  - 次のデータ送信までのカウントダウン
- **Wake-on-LAN (WoL)**:
  - 本体スイッチを押すことで、指定したMACアドレスのPCを起動させるマジックパケットを送信します。
- **データロギング**:
  - 10分ごとに測定したセンサーデータ（部屋ID、温度、湿度）を指定したサーバーへJSON形式でPOSTします。

## ハードウェア要件

- ESP8266開発ボード (ESP-WROOM-02)
- OLEDディスプレイ (SSD1306, 128x64, I2C)
- 温湿度センサー (DHT11)
- タクトスイッチ (モーメンタリ)
- ブレッドボードとジャンパーワイヤー

### 回路図 (ピン接続)

| DHTピン | ESP8266ピン |
| :--- | :--- |
| DHT11 (DATA) | D4 (GPIO 2) |
| OLED (SDA)   | D2 (GPIO 4) |
| OLED (SCL)   | D5 (GPIO 14)|
| スイッチ     | D1 (GPIO 5) |

※ スイッチは `D1` と `GND` の間に接続します。

## ソフトウェア要件

このプロジェクトは PlatformIO を使用して開発されています。

### ライブラリ

`platformio.ini` に記載されている以下のライブラリが必要です。PlatformIOが自動的にインストールします。

- `adafruit/Adafruit Unified Sensor`
- `adafruit/DHT sensor library`
- `adafruit/Adafruit GFX Library`
- `adafruit/Adafruit SSD1306`
- `bblanchon/ArduinoJson`

## セットアップ方法

1.  **リポジトリのクローン**:
    ```bash
    git clone <repository-url>
    cd <repository-name>
    ```

2.  **機密情報の設定**:
    `src/` ディレクトリに `secrets.h` という名前のファイルを作成し、以下の内容を記述します。ご自身の環境に合わせて値を書き換えてください。

    ```cpp
    #pragma once

    // --- Wi-Fi設定 ---
    const char* ssid = "YOUR_WIFI_SSID";
    const char* password = "YOUR_WIFI_PASSWORD";

    // --- WoLターゲットPCのMACアドレス ---
    const char* MAC_ADDRESS = "AA:BB:CC:DD:EE:FF";

    // --- Yahoo!天気API設定 ---
    const char* YAHOO_APP_ID = "YOUR_YAHOO_APP_ID";
    const char* LATITUDE = "35.681236";   // 緯度 (例: 東京駅)
    const char* LONGITUDE = "139.767125"; // 経度 (例: 東京駅)

    // --- データPOST先URL ---
    inline const char* POST_URL = "http://your-server-address/api/record";
    ```

3.  **コードの調整 (任意)**:
    `src/main.cpp` 内の以下の定数をご自身の環境に合わせて調整してください。
    - `local_IP`, `gateway`, `subnet`: 静的IPアドレスの設定
    - `TEMP_OFFSET`: 温度センサーの補正値
    - `ROOM_ID`: データPOST時に使用する部屋のID

4.  **ビルドと書き込み**:
    PlatformIOのUIまたはCLIを使用して、ESP8266にプログラムをビルド・書き込みします。