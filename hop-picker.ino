#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <Adafruit_HX8357.h>

#include <simpleDSTadjust.h>

#include <math.h>

#include "settings.h"
/*
#define UTC_OFFSET -7
struct dstRule StartRule = {"MDT", Second, Sun, Mar, 2, 3600};
struct dstRule EndRule = {"MST", First, Sun, Nov, 1, 0};

#define WIFI_SSID "mynetwork"
#define WIFI_PASS "mypassword"
*/

#define STMPE_CS 16
#define TFT_CS   0
#define TFT_DC   15
#define SD_CS    2
#define TFT_RST -1

#define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"

char DAYS[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

simpleDSTadjust dstAdjusted(StartRule, EndRule);

Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

boolean connectWifi() {
    if(WiFi.status() == WL_CONNECTED) return true;

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int i = 0;
    while(WiFi.status() != WL_CONNECTED) {
        delay(300);
        i++;
        if (i > 30) {
            return false;
        }
    }
    return true;
}

time_t syncTime() {
    configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);

    time_t this_second = 0;
    while(this_second < 86400 * 365) {
        delay(100);
        time(&this_second);
    }

    char *dstAbbrev;
    time_t now = dstAdjusted.time(&dstAbbrev);

    return now;
}

#define RADIUS 600
#define LAB_OFFSET 40
#define DATE_OFFSET 40
#define EDGE_OFFSET 120
#define HAND_OFFSET 20

#define T_OFF 8

float getAngle(int hour, int minute) {
    return (((hour * 60 + minute) / 720.0) + 0.5) * M_PI;
}

void drawTime(struct tm *timeinfo, bool blank) {
    // angle correspoding to current time
    float radCurrentTime = getAngle(timeinfo->tm_hour, timeinfo->tm_min);

    // compute offset
    int xorig = tft.width() / 2;
    int yorig = tft.height() / 2;

    int xoff = (RADIUS - xorig + EDGE_OFFSET) * cos(radCurrentTime);
    int yoff = (RADIUS - yorig + EDGE_OFFSET) * sin(radCurrentTime);

    tft.setTextColor(blank ? HX8357_BLACK : HX8357_WHITE);

    // current time
    tft.fillTriangle(
        (xorig - xoff) + HAND_OFFSET * cos(radCurrentTime + M_PI * 0.5),
        (yorig - yoff) + HAND_OFFSET * sin(radCurrentTime + M_PI * 0.5),
        (xorig - xoff) + HAND_OFFSET * cos(radCurrentTime - M_PI * 0.5),
        (yorig - yoff) + HAND_OFFSET * sin(radCurrentTime - M_PI * 0.5),
        (xorig - xoff) + RADIUS * cos(radCurrentTime),
        (yorig - yoff) + RADIUS * sin(radCurrentTime),
        blank ? HX8357_BLACK : HX8357_RED);

    // day + date
    char dayStr[7];
    sprintf(dayStr, "%s %02d", DAYS[timeinfo->tm_wday], timeinfo->tm_mday);
    int16_t x1, y1;
    uint16_t w, h;
    tft.setFont(&FreeSans12pt7b);
    tft.getTextBounds(dayStr, 0, 0, &x1, &y1, &w, &h);
    int xpos = (xorig - xoff) + (RADIUS - 4 * DATE_OFFSET) * cos(radCurrentTime) - w / 2;
    int ypos = (yorig - yoff) + (RADIUS - 4 * DATE_OFFSET) * sin(radCurrentTime) + h / 2;
    tft.getTextBounds(dayStr, xpos, ypos, &x1, &y1, &w, &h);
    tft.fillRoundRect(x1 - 10, y1 - 10, w + 20, h + 20, 10, HX8357_BLACK);
    tft.drawRoundRect(x1 - 10, y1 - 10, w + 20, h + 20, 10, blank ? HX8357_BLACK : HX8357_RED);
    tft.setCursor(xpos, ypos);
    tft.print(dayStr);

    // hours before and after
    for(int i = -2; i < 3; i++) {
        float radHour = getAngle((timeinfo->tm_hour + 24 + i) % 24, 0);
        tft.fillCircle((xorig - xoff) + RADIUS * cos(radHour),
            (yorig - yoff) + RADIUS * sin(radHour),
            6, blank ? HX8357_BLACK : HX8357_WHITE);

        // local timezone label
        char timeStr[3];
        sprintf(timeStr, "%02d", (timeinfo->tm_hour + 24 + i) % 24);
        tft.setFont(&FreeSans24pt7b);
        tft.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
        tft.setCursor((xorig - xoff) + (RADIUS - LAB_OFFSET) * cos(radHour) - w / 2,
            (yorig - yoff) + (RADIUS - LAB_OFFSET) * sin(radHour) + h / 2);
        tft.print(timeStr);

        // second timezone label
        sprintf(timeStr, "%02d", (timeinfo->tm_hour + 24 + i + T_OFF) % 24);
        tft.setFont(&FreeSans12pt7b);
        tft.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
        tft.setCursor((xorig - xoff) + (RADIUS + LAB_OFFSET / 1.5) * cos(radHour) - w / 2,
            (yorig - yoff) + (RADIUS + LAB_OFFSET / 1.5) * sin(radHour) + h / 2);
        tft.print(timeStr);

        for(int j = 1; j < 12; j++) {
            float radMin = getAngle((timeinfo->tm_hour + 24 + i) % 24, j * 5);
            int r = j == 6 ? 4 : j == 3 || j == 9 ? 2 : 1;
            tft.fillCircle((xorig - xoff) + RADIUS * cos(radMin),
                (yorig - yoff) + RADIUS * sin(radMin),
                r, blank ? HX8357_BLACK : HX8357_WHITE);
        }
    }
}

void updateTime(time_t time) {
    time -= 60;
    struct tm *timeinfo = localtime(&time);
    //Serial.println(timeinfo->tm_min);
    drawTime(timeinfo, true);

    time += 60;
    timeinfo = localtime(&time);
    //Serial.println(timeinfo->tm_min);
    drawTime(timeinfo, false);
}


void setup() {
    tft.begin(HX8357D);
    tft.setRotation(1);
    tft.fillScreen(HX8357_BLACK);
    //Serial.begin(115200);
}

void loop() {
    tft.setTextWrap(false);
    tft.setTextSize(1);

    boolean connected = connectWifi();
    if(connected) {
        updateTime(syncTime());

        char *dstAbbrev;
        time_t time = dstAdjusted.time(&dstAbbrev);
        struct tm *timeinfo = localtime(&time);
        delay(1000 * (60 - timeinfo->tm_sec));
    } else {
        tft.setFont(&FreeSans18pt7b);
        char err[] = "Could not connect to WiFi!";
        int16_t x1, y1;
        uint16_t w, h;
        tft.getTextBounds(err, 0, 0, &x1, &y1, &w, &h);

        tft.setTextColor(HX8357_WHITE);
        tft.setCursor(tft.width() / 2 - w / 2, tft.height() / 2 + h / 2);
        tft.print(err);
        delay(60e6);
    }
}
