#include <Arduino.h>
#include <unity.h>
#include "weather.h" // テスト対象の関数と構造体をインクルード

// テスト対象の関数は `src/weather.cpp` にありますが、テスト実行時にはデフォルトでコンパイルされません。
// .cppファイルを直接インクルードすることで、そのコードをテストビルドで利用可能にします。
#include "../../src/weather.cpp"

// setUpとtearDownは、各テストの前後で実行されますが、今回は不要です
void setUp(void) {}
void tearDown(void) {}

void test_parse_no_rain(void)
{
    const char *json = "{\"Feature\":[{\"Property\":{\"WeatherList\":{\"Weather\":[{\"Date\":\"202310271000\",\"Rainfall\":0},{\"Date\":\"202310271005\",\"Rainfall\":0},{\"Date\":\"202310271010\",\"Rainfall\":0}]}}}]}";
    RainInfo result = parseYahooWeatherJson(json);
    TEST_ASSERT_FALSE(result.willRain);
    TEST_ASSERT_EQUAL(0, result.minutesUntilRain);
}

void test_parse_rain_in_10_minutes(void)
{
    const char *json = "{\"Feature\":[{\"Property\":{\"WeatherList\":{\"Weather\":[{\"Date\":\"202310271000\",\"Rainfall\":0},{\"Date\":\"202310271005\",\"Rainfall\":0},{\"Date\":\"202310271010\",\"Rainfall\":5}]}}}]}";
    RainInfo result = parseYahooWeatherJson(json);
    TEST_ASSERT_TRUE(result.willRain);
    TEST_ASSERT_EQUAL(10, result.minutesUntilRain);
}

void test_parse_raining_now_but_stops(void)
{
    const char *json = "{\"Feature\":[{\"Property\":{\"WeatherList\":{\"Weather\":[{\"Date\":\"202310271000\",\"Rainfall\":5},{\"Date\":\"202310271005\",\"Rainfall\":0},{\"Date\":\"202310271010\",\"Rainfall\":0}]}}}]}";
    RainInfo result = parseYahooWeatherJson(json);
    // 0分後の雨は「今降っている」と判定される
    TEST_ASSERT_TRUE(result.willRain);
    TEST_ASSERT_EQUAL(0, result.minutesUntilRain);
}

void test_parse_rain_in_5_minutes(void)
{
    const char *json = "{\"Feature\":[{\"Property\":{\"WeatherList\":{\"Weather\":[{\"Date\":\"202310271000\",\"Rainfall\":0},{\"Date\":\"202310271005\",\"Rainfall\":2},{\"Date\":\"202310271010\",\"Rainfall\":5}]}}}]}";
    RainInfo result = parseYahooWeatherJson(json);
    TEST_ASSERT_TRUE(result.willRain);
    TEST_ASSERT_EQUAL(5, result.minutesUntilRain);
}

void setup()
{
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_parse_no_rain);
    RUN_TEST(test_parse_rain_in_10_minutes);
    RUN_TEST(test_parse_raining_now_but_stops);
    RUN_TEST(test_parse_rain_in_5_minutes);
    UNITY_END();
}

void loop()
{
    // Do nothing
}