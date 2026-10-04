/**
 * [INPUT]: core 的回绕计时与 UTF-8 校验；不依赖 WiFi、线程或存储
 * [OUTPUT]: 连接轮次、纯 STA 回退、凭据边界与跨毫秒回绕的事件归属判定
 * [POS]: 设备与主机测试共用的生产策略，副作用留在 network.cpp
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#pragma once
#include "core.h"
#include <algorithm>

namespace linkbit {
constexpr uint32_t ConnectBudgetMs = 30000, AttemptBudgetMs = 9000;


enum class NetStage { Idle, Queued, Disconnecting, Connecting, Ip, Backoff, Success, Failed };
inline const char* stageName(NetStage stage) {
    const char* names[] = {"idle","queued","disconnecting","connecting","ip","backoff","success","failed"};
    return names[int(stage)];
}
inline bool validWifi(const char* ssid, size_t ssidSize, const char* password, size_t passwordSize) {
    if (!ssid || !password || strlen(ssid) != ssidSize || strlen(password) != passwordSize || !validText(ssid,32)) return false;
    for (size_t i = 0; i < ssidSize; ++i) if (uint8_t(ssid[i]) < 32 || uint8_t(ssid[i]) == 127) return false;
    if (!passwordSize) return true;
    if (passwordSize < 8 || passwordSize > 64) return false;
    if (passwordSize == 64 && strspn(password,"0123456789abcdefABCDEF") != 64) return false;
    for (size_t i = 0; i < passwordSize; ++i) if (uint8_t(password[i]) < 32 || uint8_t(password[i]) == 127) return false;
    return true;
}
struct ConnectionRound {
    NetStage stage = NetStage::Idle;
    NetStage errorStage = NetStage::Idle;
    uint32_t started = 0, changed = 0, attemptStarted = 0;
    uint8_t attempt = 0;
    int reason = 0, error = 0;
    bool busy() const { return stage >= NetStage::Queued && stage <= NetStage::Backoff; }
    bool running() const { return busy() && stage != NetStage::Queued; }
    void queue() { *this = {}; stage = NetStage::Queued; }
    void fail(int code) { error = code; errorStage = stage; stage = NetStage::Failed; }
    void nextAttempt(uint32_t now) { ++attempt; changed = now; stage = NetStage::Disconnecting; }
    void start(uint32_t now) { started = now; nextAttempt(now); }
    bool stopped(uint32_t now) {
        if (stage != NetStage::Disconnecting || elapsed(now,started,ConnectBudgetMs)) return false;
        changed = attemptStarted = now; stage = NetStage::Connecting; return true;
    }
    void retry(uint32_t now, int code) {
        error = code; errorStage = stage;
        if (attempt >= 3 || elapsed(now,started,ConnectBudgetMs)) { stage = NetStage::Failed; return; }
        changed = now; stage = NetStage::Backoff;
    }
    void submitted(uint32_t now, int code, bool permanent) {
        if (!code) return;
        if (permanent) fail(code); else retry(now,code);
    }
    void disconnected(uint32_t now, int rawReason, bool intentional, bool frameworkRetry) {
        if (intentional || (stage != NetStage::Connecting && stage != NetStage::Ip)) return;
        reason = rawReason;
        if (frameworkRetry) { stage = NetStage::Connecting; return; }
        retry(now,rawReason);
    }
    bool associated(uint32_t now) {
        if ((stage != NetStage::Connecting && stage != NetStage::Ip) || elapsed(now,started,ConnectBudgetMs)) return false;
        stage = NetStage::Ip; return true;
    }
    bool gotIp(uint32_t now, bool matches) {
        if (stage != NetStage::Ip || !matches || elapsed(now,started,ConnectBudgetMs)) return false;
        stage = NetStage::Success; error = 0; return true;
    }
    void tick(uint32_t now) {
        if (!running()) return;
        if (elapsed(now,started,ConnectBudgetMs)) { fail(-1); return; }
        if (stage == NetStage::Disconnecting && elapsed(now,changed,1500)) { fail(-2); return; }
        if ((stage == NetStage::Connecting || stage == NetStage::Ip) && elapsed(now,attemptStarted,AttemptBudgetMs)) retry(now,-1);
        if (stage == NetStage::Backoff && elapsed(now,changed,attempt * 1000U)) nextAttempt(now);
    }
};
inline bool mayAutoConnect(uint32_t now, uint32_t lastActivity, bool busy) {
    return !busy && !elapsed(now,lastActivity,AwakeMs);
}
inline bool eventBelongsToAttempt(uint32_t eventMs,uint32_t attemptStarted) {
    return int32_t(eventMs-attemptStarted)>=0;
}
inline bool shouldRetryStaOnly(bool active,bool diagnostic,bool apStarted,uint8_t phase,bool success) {
    return active&&!diagnostic&&apStarted&&!phase&&!success;
}
}
