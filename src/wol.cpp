#include "wol.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/**
 * @brief MACアドレス文字列をバイト配列に変換する
 * @param macStr "AA:BB:CC:DD:EE:FF" 形式のMACアドレス文字列
 * @param macBytes 変換後のバイト配列（6バイト）
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool macStringToBytes(const char* macStr, byte* macBytes) {
    if (strlen(macStr) != 17) {
        return false; // フォーマットが不正
    }
    for (int i = 0; i < 6; i++) {
        char temp[3] = {macStr[i * 3], macStr[i * 3 + 1], '\0'};
        macBytes[i] = (byte)strtol(temp, NULL, 16);
    }
    return true;
}

/**
 * @brief Wake-on-LANのマジックパケットを送信する
 * @param macAddress ターゲットPCのMACアドレス文字列
 */
void sendWolPacket(const char* macAddress) {
    WiFiUDP udp;
    byte targetMac[6];

    if (!macStringToBytes(macAddress, targetMac)) {
        Serial.println(F("Invalid MAC address format."));
        return;
    }

    // マジックパケットを作成 (102 bytes)
    byte magicPacket[102];

    // 1. 同期ストリーム (6 bytes of 0xFF)
    memset(magicPacket, 0xFF, 6);

    // 2. MACアドレスを16回繰り返す
    for (int i = 1; i <= 16; i++) {
        memcpy(&magicPacket[i * 6], targetMac, 6);
    }

    // ブロードキャストアドレスにパケットを送信
    IPAddress broadcastIp = WiFi.localIP();
    broadcastIp[3] = 255; // サブネットのブロードキャストアドレス (例: 192.168.1.255)

    // 信頼性を高めるために3回送信する
    for (int i = 0; i < 3; i++) {
        udp.beginPacket(broadcastIp, 9); // WoLの標準ポートは9
        udp.write(magicPacket, sizeof(magicPacket));
        udp.endPacket();
        delay(100); // パケット間に少し待機
    }
    Serial.println(F("WoL packet sent 3 times."));
}