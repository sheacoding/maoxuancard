/**
 * [INPUT]: 已转换为北京时间的日期键与日内秒数
 * [OUTPUT]: 对外提供每日换句判定和下一次低功耗唤醒间隔
 * [POS]: 时钟模块与主机测试共用的纯策略，不依赖网络、RTC 或存储
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#pragma once
#include <cstdint>
#include <algorithm>

namespace linkbit {
constexpr uint32_t AutoSecond=8*60*60, PrewakeSecond=7*60*60+58*60;
constexpr uint32_t UnknownClockRetrySeconds=6*60*60, SecondsPerDay=24*60*60;
struct ClockPoint {bool valid=false;uint32_t day=0,second=0;};
inline bool validStoredDay(uint32_t day) {
    if(day==0) return true;
    uint32_t year=day/10000,month=day/100%100,date=day%100;
    if(year<2024||year>2099||month<1||month>12) return false;
    static constexpr uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t maximum=days[month-1]+(month==2&&((year%4==0&&year%100!=0)||year%400==0));
    return date>=1&&date<=maximum;
}
inline bool automaticDue(const ClockPoint& now,uint32_t lastDay) {
    return now.valid&&now.second>=AutoSecond&&now.day>lastDay;
}
inline uint32_t nextWakeSeconds(const ClockPoint& now,uint32_t lastDay) {
    if(!now.valid||now.second>=SecondsPerDay) return UnknownClockRetrySeconds;
    if(now.day<=lastDay) return SecondsPerDay-now.second+PrewakeSecond;
    if(now.second<PrewakeSecond) return PrewakeSecond-now.second;
    if(now.second<AutoSecond) return AutoSecond-now.second;
    return std::min(UnknownClockRetrySeconds,SecondsPerDay-now.second+PrewakeSecond);
}
}
