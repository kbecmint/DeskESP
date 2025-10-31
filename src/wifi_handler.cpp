#include "wifi_handler.h"
#include <ESP8266WiFi.h>
#include "secrets.h" // ssid, password

// --- 静的IPアドレスの設定 ---
// main.cppから設定を引用
extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress primaryDNS;
extern IPAddress secondaryDNS;

bool ensureWiFiConnected(Adafruit_SSD1306 *display)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        // DNS解決失敗からの回復を試みるため、接続済みの場合でもDNSサーバーを再設定する。
        // これにより、長時間稼働中にDNS設定が失われる問題に対処する。
        if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
        {
            Serial.println("STA Failed to re-configure DNS");
        }
        return true; // すでに接続済み
    }

    Serial.println("WiFi disconnected. Reconnecting...");
    if (display)
    {
        display->clearDisplay();
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);
        display->setCursor(0, 28);
        display->print("Reconnecting WiFi...");
        display->display();
    }

    // 静的IPアドレスを再設定
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
        Serial.println("STA Failed to configure");
    }

    WiFi.begin(ssid, password);

    unsigned long startTime = millis();
    // 最大15秒間、再接続を試みる
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000)
    {
        Serial.print(".");
        if (display)
        {
            display->print(".");
            display->display();
        }
        // delay()はバックグラウンド処理をブロックするため、短いdelayとyield()を組み合わせる
        for (int i = 0; i < 50; i++)
        {
            delay(10);
            yield();
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi reconnected!");
        return true;
    }

    Serial.println("\nFailed to reconnect WiFi.");
    return false;
}