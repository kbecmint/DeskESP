#pragma once

#include <Arduino.h>

// Wake-on-LANのマジックパケットを送信する
// macAddress: ターゲットPCのMACアドレス文字列 (例: "AA:BB:CC:DD:EE:FF")
void sendWolPacket(const char* macAddress);