/**
 * [INPUT]: app 的网络/自动推进边界、Arduino SNTP 与 C 运行库北京时间
 * [OUTPUT]: 非阻塞联网校时、每日触发、RTC 唤醒间隔和只读状态 JSON
 * [POS]: 时间副作用的唯一所有者；SNTP 回调只发信号，主循环执行业务
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "app.h"
#include "clock_policy.h"
#include <esp_sntp.h>
#include <atomic>
#include <ctime>

namespace linkbit {
static std::atomic<bool> syncNotice{false};
static bool sntpStarted=false;
static ClockPoint point() {
    time_t epoch=time(nullptr);struct tm local{};
    if(epoch<1704067200||!localtime_r(&epoch,&local)) return {};
    return {true,uint32_t((local.tm_year+1900)*10000+(local.tm_mon+1)*100+local.tm_mday),
        uint32_t(local.tm_hour*3600+local.tm_min*60+local.tm_sec)};
}
static void formatLocal(char* out,size_t size,const struct tm& local,bool seconds) {
    strftime(out,size,seconds?"%Y-%m-%d %H:%M:%S":"%Y-%m-%d %H:%M",&local);
}
void clockBegin() {
    setenv("TZ","CST-8",1);tzset();
    sntp_set_time_sync_notification_cb([](timeval*){syncNotice.store(true);});
}
void clockTick() {
    if(networkConnected()&&!sntpStarted) {
        configTzTime("CST-8","ntp.aliyun.com","ntp.tencent.com","pool.ntp.org");sntpStarted=true;
        Serial.println("[clock] SNTP started");
    }
    if(syncNotice.exchange(false)) {state.clockNetworkSynced=true;Serial.println("[clock] synchronized");}
    ClockPoint now=point();state.clockValid=now.valid;
    if(automaticDue(now,state.settings.lastAutoDay)&&state.clockAttemptDay!=now.day) {
        state.clockAttemptDay=now.day;automaticAdvance(now.day);
    }
}
uint64_t clockWakeDelayUs() {
    uint64_t seconds=nextWakeSeconds(point(),state.settings.lastAutoDay);
    return std::max<uint64_t>(1,seconds)*1000000ULL;
}
void clockStop() {if(sntpStarted) {esp_sntp_stop();sntpStarted=false;}}
void clockStatus(JsonObject object) {
    time_t epoch=time(nullptr);struct tm local{};ClockPoint now=point();
    object["valid"]=now.valid;object["networkSynced"]=state.clockNetworkSynced;
    object["lastAutomaticDay"]=state.settings.lastAutoDay;object["error"]=state.clockError;
    if(!now.valid||!localtime_r(&epoch,&local)) {object["localTime"]=nullptr;object["nextSwitch"]=nullptr;return;}
    char text[24];formatLocal(text,sizeof(text),local,true);object["localTime"]=text;
    local.tm_hour=8;local.tm_min=local.tm_sec=0;
    if(now.second>=AutoSecond||now.day<=state.settings.lastAutoDay) ++local.tm_mday;
    mktime(&local);formatLocal(text,sizeof(text),local,false);object["nextSwitch"]=text;
}
}
