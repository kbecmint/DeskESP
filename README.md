# DeskESP

ESP8266 (ESP-WROOM-02) を使用して、室内の温度と湿度を測定し、OLEDディスプレイに表示するデスク環境モニターです。

## 機能

- 温度の測定と表示
- 湿度の測定と表示
- スイッチ操作によるWake-on-LAN (WoL) パケットの送信
- Yahoo!天気APIを利用した雨雲接近の通知

## ハードウェア

- ESP8266開発ボード (ESP-WROOM-02)
- DHTセンサー (DHT11 or DHT22)
- SSD1306 I2C OLEDディスプレイ (128x64)
- ブレッドボードとジャンパーワイヤー

## 配線

各コンポーネントを以下のように接続します。

### DHTセンサー

| DHTピン | ESP8266ピン |
| :--- | :--- |
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO2 (D4) |

### SSD1306 OLEDディスプレイ (I2C)

| SSD1306ピン | ESP8266ピン |
| :--- | :--- |
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO5 (D1) |
| SDA | GPIO4 (D2) |

*注意: 使用するESP8266ボードのピン配置に合わせて、ピン番号を調整してください。*

## ソフトウェア

このプロジェクトは [PlatformIO](https://platformio.org/) を使用して開発されています。

### ライブラリ

以下のライブラリに依存しています (`platformio.ini`で管理されています)。

- Adafruit Unified Sensor
- DHT sensor library
- Adafruit GFX Library
- Adafruit SSD1306
- ArduinoJson

## セットアップとビルド

1. このリポジトリをクローンします。
2. Visual Studio CodeでPlatformIO拡張機能を使ってプロジェクトを開きます。
3. `src/secrets.h.example` をコピーして `src/secrets.h` という名前のファイルを作成します。
4. 作成した `src/secrets.h` を開き、ご自身の環境に合わせて以下の情報を設定します。
    - `MAC_ADDRESS`: WoLで起動したいPCのMACアドレス
    - `ssid`: Wi-FiのSSID
    - `password`: Wi-Fiのパスワード
    - `YAHOO_APP_ID`: Yahoo! JAPAN Developer Network で取得したClient ID
    - `LATITUDE`: 観測したい地点の緯度
    - `LONGITUDE`: 観測したい地点の経度
5. `src/main.cpp`内のピン定義が、ご自身の配線と一致していることを確認します。
6. PlatformIOの "Upload" タスクを実行して、ファームウェアをESP8266に書き込みます。