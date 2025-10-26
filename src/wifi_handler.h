#pragma once

#include <Adafruit_SSD1306.h>

/**
 * @brief WiFi接続を確実にし、切断されている場合は再接続を試みる
 *
 * @param display OLEDディスプレイのオブジェクトへのポインタ
 * @return bool 接続が確立されればtrue、失敗すればfalse
 */
bool ensureWiFiConnected(Adafruit_SSD1306 *display);