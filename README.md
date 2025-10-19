# DeskESP

ESP8266 (ESP-WROOM-02) を使用して、室内の温度と湿度を測定し、OLEDディスプレイに表示するデスク環境モニターです。

## 機能

- 温度の測定と表示
- 湿度の測定と表示

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

## ビルドとアップロード

1. このリポジトリをクローンします。
2. Visual Studio CodeでPlatformIO拡張機能を使ってプロジェクトを開きます。
3. `src/main.cpp`内のピン定義が、ご自身の配線と一致していることを確認します。
4. PlatformIOの "Upload" タスクを実行して、ファームウェアをESP8266に書き込みます。