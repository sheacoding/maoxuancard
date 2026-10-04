/**
 * [INPUT]: core 的卡片契约、ArduinoJson 和 FreeRTOS
 * [OUTPUT]: 主循环状态与存储、HTTP、网络、屏幕模块接口
 * [POS]: IO 模块的共同边界；位图任务不持有可变目录或设置
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#pragma once
#include "core.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace linkbit {
struct Settings {
    char ssid[33]{},password[65]{},admin[7]{};
    uint32_t currentId=1,lastAutoDay=0;
    bool favoritesOnly=false,loginCompleted=false;
};
struct State:RefreshState {
    Settings settings;
    Entry entries[CatalogLimit];int count=0;
    bool storageReady=false,shuttingDown=false,managementVisible=false;
    uint32_t lastActivity=0;
    uint32_t desiredId=1,appliedId=0;
    bool clockValid=false,clockNetworkSynced=false;uint32_t clockAttemptDay=0;
    char storageError[160]{},displayError[160]{},clockError[128]{};
};
extern State state;
void storageBegin();
bool saveSettings(const Settings& settings);
bool readCard(uint32_t id,Card& card);
bool saveCard(Card& card,bool& duplicate);
bool deleteCard(uint32_t id);
bool saveAppearance(uint32_t id,bool favorite,uint8_t illustration);
bool parseCard(JsonObjectConst json,Card& card,char* error,size_t size);
void cardJson(const Card& card,JsonObject json);
size_t storageUsed();size_t storageTotal();
void networkBegin();void networkTick();void networkStop();
bool networkConnected();bool networkTesting();
int configureNetwork(const char*,size_t,const char*,size_t,bool,char*,size_t);
bool scanNetwork();void networkScanStatus(JsonObject);void networkStatus(JsonObject);
String networkAddress();const char* networkHost();
void webBegin();void webTick();void webStop();
bool uploadBusy();
bool displayFrame(const uint8_t*,char*,size_t);void displaySleep();
void touch();void markDirty();void showManagement();void completeLogin();
bool requestCard(uint32_t id);
bool setCollection(bool favorites);
void clockBegin();void clockTick();void clockStop();void clockStatus(JsonObject object);
uint64_t clockWakeDelayUs();bool automaticAdvance(uint32_t day);
}
