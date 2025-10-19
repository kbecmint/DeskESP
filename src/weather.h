#pragma once

#include <Arduino.h>

// 雨雲情報の結果を格納する構造体
struct RainInfo {
  bool willRain;      // 60分以内に雨が降るか
  int minutesUntilRain; // 何分後に雨が降り始めるか (降らない場合は0)
  String statusMessage; // API通信ステータス
};

/**
 * @brief Yahoo!天気APIから降水情報を取得し、雨雲の接近をチェックする
 * @return RainInfo 雨雲情報の結果
 */
RainInfo checkRainCloud();