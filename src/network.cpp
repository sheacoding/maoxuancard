/**
 * [INPUT]: WiFi/ESPmDNS、network_policy 的轮次策略与 app 的设置保存
 * [OUTPUT]: AP＋STA 到纯 STA 的有界回退、旧事件隔离、临时凭据清理、异步扫描与会话诊断
 * [POS]: 网络副作用的唯一所有者；不含互联网取数流程
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "app.h"
#include "network_policy.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <atomic>

namespace linkbit {
static char host[24]{}, apName[24]{}, netError[128]{};
static bool apStarted = false, wasConnected = false, mdnsStarted = false;
static uint32_t retryAt = 0, sessionCounter = 0, bootId = 0;
static int disconnectReason = 0, failureStatus = -1, failureReason = -1;
static ConnectionRound round;
enum class Purpose { None, Automatic, Configure, ConfigureSta, DiagnoseAp, DiagnoseSta, Restore };
static Purpose purpose = Purpose::None;
struct PhaseResult {
    bool complete = false, gotIp = false;
    uint32_t durationMs = 0;
    int attempts = 0, rssi = -127, auth = -1, channel = 0, reason = 0, error = 0;
    int errors[12]{};
    size_t errorCount = 0;
};
struct Operation {
    bool active = false, diagnostic = false;
    char id[24]{}, ssid[33]{}, password[65]{}, error[128]{}, ip[16]{};
    const char* result = "idle";
    NetStage stage = NetStage::Idle;
    NetStage errorStage = NetStage::Idle;
    int stageError = 0, reason = 0;
    int attempt = 0, phase = 0;
    uint32_t seconds = 0;
    PhaseResult phases[2];
};
static Operation operation;
struct NetworkEvent { uint32_t ms; const char* event; int value; };
static constexpr size_t TraceLimit = 24;
static NetworkEvent trace[TraceLimit]{};
static size_t traceNext = 0, traceCount = 0;
struct DriverEvent {
    WiFiEvent_t type;
    uint32_t ms;
    char ssid[33]{};
    uint8_t bssid[6]{};
    int reason = 0, auth = -1, channel = 0, rssi = -127;
};
static QueueHandle_t events;
static std::atomic<bool> eventOverflow{false};
static bool firstFrameworkRetry = true, stopFence = false, associated = false;
static uint8_t associatedBssid[6]{};
static bool scanPending = false, scanning = false;
static uint32_t scanStarted = 0;
static const char* scanState = "idle";
static JsonDocument scanResults;

static void recordEvent(const char* event, int value = 0, uint32_t ms = millis()) {
    trace[traceNext] = {ms,event,value}; traceNext = (traceNext+1)%TraceLimit;
    if (traceCount < TraceLimit) ++traceCount;
    Serial.printf("[wifi] %s: %d\n",event,value);
}
static bool diagnosing() { return purpose == Purpose::DiagnoseAp || purpose == Purpose::DiagnoseSta; }
static const char* targetSsid() { return operation.active ? operation.ssid : state.settings.ssid; }
static const char* targetPassword() { return operation.active ? operation.password : state.settings.password; }
bool networkConnected() { return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0); }
bool networkTesting() { return round.busy() || operation.active || scanPending || scanning; }
const char* networkHost() { return host; }
String networkAddress() {
    if (networkConnected()) return WiFi.localIP().toString();
    if (apStarted) return "192.168.4.1";
    return String(host) + ".local";
}
static void endMdns() { if (mdnsStarted) { MDNS.end(); mdnsStarted = false; } }
static bool startAP() {
    if (apStarted) return true;
    bool ok = WiFi.enableAP(true) &&
        WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0)) && WiFi.softAP(apName,nullptr);
    apStarted = ok;
    recordEvent("热点启动",ok ? 0 : -1);
    if (!ok) copyText(netError,"设备热点启动失败");
    if (state.managementVisible) markDirty(); return ok;
}
static bool stopAP() {
    if (!apStarted) return true;
    bool ok = WiFi.softAPdisconnect(true);
    if (ok) apStarted = false;
    recordEvent("诊断关闭热点",ok ? 0 : -1);
    return ok;
}
static bool supportedAuth(int auth) {
    return auth == WIFI_AUTH_OPEN || auth == WIFI_AUTH_WPA2_PSK || auth == WIFI_AUTH_WPA_WPA2_PSK ||
        auth == WIFI_AUTH_WPA3_PSK || auth == WIFI_AUTH_WPA2_WPA3_PSK;
}
static const char* authName(int auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN: return "开放";
        case WIFI_AUTH_WEP: return "WEP（不支持）";
        case WIFI_AUTH_WPA_PSK: return "WPA（不支持）";
        case WIFI_AUTH_WPA2_PSK: return "WPA2-Personal";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-Personal";
        case WIFI_AUTH_WPA3_PSK: return "WPA3-Personal";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3-Personal";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "Enterprise（需专用认证）";
        default: return "不支持的认证方式";
    }
}
bool scanNetwork() {
    if (networkTesting() || uploadBusy() || state.shuttingDown) return false;
    scanPending = true; scanState = "scanning"; scanResults.clear();
    touch(); return true;
}
void networkScanStatus(JsonObject object) { object["state"] = scanState; object["items"] = scanResults.as<JsonArray>(); }
static void scanTick() {
    if (scanPending && !state.workerBusy) {
        scanPending = false; WiFi.setScanTimeout(15000);
        int result = WiFi.scanNetworks(true,false);
        scanning = result == WIFI_SCAN_RUNNING; scanStarted = millis();
        if (!scanning) { scanState = "failed"; recordEvent("扫描启动失败",result); }
    }
    if (!scanning) return;
    int count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING && !elapsed(millis(),scanStarted,15000)) return;
    scanning = false; scanState = count >= 0 ? "ready" : "failed";
    auto items = scanResults.to<JsonArray>();
    for (int i = 0; i < count && i < 256 && items.size() < 32; ++i) {
        String ssid = WiFi.SSID(i); int auth = WiFi.encryptionType(i);
        if (!validText(ssid.c_str(),32) || ssid == apName) continue;
        bool duplicate = false;
        for (JsonObject item : items) if (item["ssid"] == ssid && item["auth"] == auth) { duplicate = true; break; }
        if (duplicate) continue;
        auto item = items.add<JsonObject>();
        item["ssid"] = ssid; item["rssi"] = WiFi.RSSI(i); item["auth"] = auth;
        item["secure"] = auth != WIFI_AUTH_OPEN; item["channel"] = WiFi.channel(i);
        item["supported"] = supportedAuth(auth); item["authName"] = authName(auth);
    }
    if (count < 0) recordEvent("停止扫描",esp_wifi_scan_stop());
    WiFi.scanDelete();
}
static void beginRound(Purpose next) {
    purpose = next; round.queue(); associated = false;
}
static void stopForAttempt() {
    endMdns(); associated = false;
    const bool staEnabled = (WiFi.getMode() & WIFI_MODE_STA) != 0;
    stopFence = !staEnabled;
    // --- STOP 事件隔离上一尝试；主动停止后不立即 begin，防止迟到事件污染新轮次 ---
    if (staEnabled && !WiFi.STA.end()) { round.fail(-3); recordEvent("停止 STA 失败",-3); }
    else recordEvent("程序主动停止 STA");
}
static void submitAttempt() {
    if (!round.stopped(millis())) return;
    stopFence = false;
    int error = 0;
    if (!WiFi.STA.begin() || !WiFi.setHostname(host) || !WiFi.setAutoReconnect(false) || !WiFi.setSleep(false)) error = -4;
    else {
        wl_status_t submitted=WiFi.begin(targetSsid(),targetPassword());
        recordEvent("WiFi.begin 状态",int(submitted));
        if(submitted==WL_CONNECT_FAILED||submitted==WL_NO_SHIELD) error=-5;
    }
    recordEvent("提交连接",error);
    // --- WiFi.begin 是框架公开连接入口；非终态返回值表示连接仍在异步进行。 ---
    round.submitted(millis(),error,error < 0 || error == ESP_ERR_INVALID_ARG || error == ESP_ERR_WIFI_PASSWORD);
}
static void consumeEvents() {
    DriverEvent event;
    while (events && xQueueReceive(events,&event,0) == pdTRUE) {
        if (event.type == ARDUINO_EVENT_WIFI_STA_STOP) {
            recordEvent("STA 已停止",0,event.ms);
            if (round.stage == NetStage::Disconnecting && eventBelongsToAttempt(event.ms,round.changed)) stopFence = true;
        } else if (event.type == ARDUINO_EVENT_WIFI_STA_START) recordEvent("STA 已启动",0,event.ms);
        else if (event.type == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            recordEvent("断开事件，原因码",event.reason,event.ms);
            const bool implicit = firstFrameworkRetry && event.reason != WIFI_REASON_ASSOC_LEAVE;
            if (implicit) firstFrameworkRetry = false; // 与固定 3.3.8 的进程级 first_connect 同步
            bool intentional = round.stage == NetStage::Disconnecting || round.stage == NetStage::Queued ||
                round.stage == NetStage::Backoff || purpose == Purpose::None || event.reason == WIFI_REASON_ASSOC_LEAVE;
            if (!intentional && eventBelongsToAttempt(event.ms,round.attemptStarted) && (!event.ssid[0] || !strcmp(event.ssid,targetSsid()))) {
                disconnectReason = event.reason; associated = false;
                if (diagnosing()) {
                    auto& result = operation.phases[operation.phase];
                    if (result.errorCount < 12) result.errors[result.errorCount++] = event.reason;
                    result.rssi = event.rssi;
                }
                round.disconnected(millis(),event.reason,false,implicit);
                if (implicit) recordEvent("等待框架首轮重试");
            }
        } else if (event.type == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            if (eventBelongsToAttempt(event.ms,round.attemptStarted) && !strcmp(event.ssid,targetSsid()) && round.associated(millis())) {
                associated = true; memcpy(associatedBssid,event.bssid,6);
                recordEvent("已关联路由器，等待 IP",0,event.ms);
                if (diagnosing()) {
                    auto& result = operation.phases[operation.phase];
                    result.auth = event.auth; result.channel = event.channel;
                }
            } else recordEvent("忽略非本轮关联事件");
        } else if (event.type == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            wifi_ap_record_t ap{};
            bool matches = associated && eventBelongsToAttempt(event.ms,round.attemptStarted) && networkConnected() && esp_wifi_sta_get_ap_info(&ap) == ESP_OK &&
                !strcmp(WiFi.SSID().c_str(),targetSsid()) && !memcmp(ap.bssid,associatedBssid,6);
            if (round.gotIp(millis(),matches)) {
                recordEvent("本轮已获得 IP",0,event.ms);
                if (diagnosing()) operation.phases[operation.phase].rssi = ap.rssi;
            } else recordEvent("忽略非本轮 IP 事件",0,event.ms);
        } else if (event.type == ARDUINO_EVENT_WIFI_STA_LOST_IP) recordEvent("IP 已丢失",0,event.ms);
    }
    if (eventOverflow.exchange(false)) { recordEvent("事件队列溢出",-6); if (round.busy()) round.fail(-6); }
}
static void finishOperation() {
    operation.active = false;
    memset(operation.password,0,sizeof(operation.password));
    if (operation.diagnostic || !strcmp(operation.result,"failed")) {
        endMdns();
        bool cleared = true;
        if (WiFi.getMode() & WIFI_MODE_STA) {
            cleared = WiFi.STA.disconnect(true,0);
            recordEvent("清除 STA 临时凭据",cleared ? 0 : -3);
        }
        bool stopped = WiFi.STA.end(); recordEvent("结束候选连接",stopped ? 0 : -3);
        if (!cleared || !stopped) {
            stopped = WiFi.mode(WIFI_OFF);recordEvent("候选连接停止回退",stopped ? 0 : -3);
            if (stopped) apStarted = false;
        }
        if (!stopped) copyText(operation.error,"候选网络未能停止；原配置保留，请重启设备");
        if (!startAP()) copyText(operation.error,"诊断/配网结束，但热点恢复失败");
        if (state.settings.ssid[0] && stopped) beginRound(Purpose::Restore);
        else { purpose = Purpose::None; round = {}; }
    } else { purpose = Purpose::None; round = {}; }
    retryAt = millis()+30000; if (state.managementVisible) markDirty();
}
static void finishRound() {
    const bool ok = round.stage == NetStage::Success;
    failureStatus = ok ? -1 : int(WiFi.status()); failureReason = ok ? -1 : round.reason;
    if (!ok) {
        recordEvent("轮次失败（断开前状态）",failureStatus);
        snprintf(netError,sizeof(netError),"连接失败：%s，错误 %d，原因 %d；原配置保留",stageName(round.errorStage),round.error,round.reason);
    }
    if (diagnosing()) {
        auto& result = operation.phases[operation.phase];
        result.complete = true; result.gotIp = ok; result.durationMs = millis()-round.started;
        result.attempts = round.attempt; result.reason = round.reason; result.error = round.error;
        if (purpose == Purpose::DiagnoseAp) { operation.phase = 1; beginRound(Purpose::DiagnoseSta); return; }
        operation.result = "done"; operation.stage = NetStage::Success; operation.error[0] = 0;
        finishOperation(); return;
    }
    if (shouldRetryStaOnly(operation.active,operation.diagnostic,apStarted,operation.phase,ok)) {
        operation.phase=1;recordEvent("热点并行连接失败，改用仅 STA");beginRound(Purpose::ConfigureSta);return;
    }
    if (purpose == Purpose::Configure || purpose == Purpose::ConfigureSta) {
        bool saved = false;
        if (ok) {
            if (purpose == Purpose::ConfigureSta) startAP();
            Settings next = state.settings;
            copyText(next.ssid,operation.ssid); copyText(next.password,operation.password);
            saved = saveSettings(next);
            if (saved) { state.settings = next; copyText(operation.ip,WiFi.localIP().toString().c_str()); }
            else copyText(netError,"已获得 IP，但 NVS 保存失败；原配置保留");
            memset(next.password,0,sizeof(next.password));
        }
        operation.result = saved ? "saved" : "failed"; operation.stage = saved ? NetStage::Success : NetStage::Failed;
        if (saved) { netError[0] = 0; operation.error[0] = 0; } else copyText(operation.error,netError);
        finishOperation(); return;
    }
    if (ok) netError[0] = 0;
    else { recordEvent("结束自动连接",WiFi.STA.end() ? 0 : -3); startAP(); }
    purpose = Purpose::None; round = {}; retryAt = millis()+30000;
}
int configureNetwork(const char* ssid, size_t ssidSize, const char* password, size_t passwordSize, bool diagnose, char* id, size_t idSize) {
    if (!validWifi(ssid,ssidSize,password,passwordSize)) return 400;
    bool seen = false, supported = false;
    for (JsonObject item : scanResults.as<JsonArray>()) if (item["ssid"] == ssid) { seen = true; supported |= item["supported"] | false; }
    if (seen && !supported) return 400;
    if (networkTesting() || uploadBusy() || state.shuttingDown) return 409;
    operation = {}; operation.active = true; operation.diagnostic = diagnose;
    snprintf(operation.id,sizeof(operation.id),"%08lx-%lu",static_cast<unsigned long>(bootId),static_cast<unsigned long>(++sessionCounter));
    copyText(operation.ssid,ssid); copyText(operation.password,password); snprintf(id,idSize,"%s",operation.id);
    operation.result = "queued"; operation.stage = NetStage::Queued;
    netError[0] = 0; failureStatus = failureReason = -1;
    for (JsonObject item : scanResults.as<JsonArray>()) if (item["ssid"] == ssid) {
        for (auto& phase : operation.phases) { phase.auth = item["auth"]; phase.rssi = item["rssi"]; phase.channel = item["channel"]; }
        break;
    }
    beginRound(diagnose ? Purpose::DiagnoseAp : Purpose::Configure);
    recordEvent(diagnose ? "诊断请求已接收" : "配网请求已接收"); touch(); return 202;
}
void networkBegin() {
    uint64_t chip = ESP.getEfuseMac(); bootId = esp_random();
    snprintf(host,sizeof(host),"linkbit-%06x",static_cast<unsigned>((chip>>24)&0xFFFFFF));
    snprintf(apName,sizeof(apName),"LinkBit-%06X",static_cast<unsigned>((chip>>24)&0xFFFFFF));
    events = xQueueCreate(32,sizeof(DriverEvent));
    WiFi.onEvent([](WiFiEvent_t type, WiFiEventInfo_t info) {
        if (type < ARDUINO_EVENT_WIFI_STA_START || type > ARDUINO_EVENT_WIFI_STA_LOST_IP) return;
        DriverEvent event{}; event.type = type; event.ms = millis();
        if (type == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            const auto& data = info.wifi_sta_disconnected;
            memcpy(event.ssid,data.ssid,std::min<size_t>(data.ssid_len,32)); event.reason = data.reason; event.rssi = data.rssi;
        } else if (type == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            const auto& data = info.wifi_sta_connected;
            memcpy(event.ssid,data.ssid,std::min<size_t>(data.ssid_len,32)); memcpy(event.bssid,data.bssid,6);
            event.auth = data.authmode; event.channel = data.channel;
        }
        if (!events || xQueueSend(events,&event,0) != pdTRUE) eventOverflow.store(true);
    });
    WiFi.persistent(false);
    if (!events || !WiFi.mode(WIFI_STA) || !WiFi.setHostname(host) || !WiFi.setAutoReconnect(false) || !WiFi.setSleep(false)) {
        copyText(netError,"WiFi 初始化失败"); recordEvent("初始化失败",-4); startAP(); return;
    }
    if (state.settings.ssid[0]) beginRound(Purpose::Automatic); else startAP();
}
void networkTick() {
    consumeEvents(); scanTick();
    if (round.stage == NetStage::Queued && !state.workerBusy) {
        round.start(millis());
        bool modeOk = (purpose == Purpose::DiagnoseSta || purpose == Purpose::ConfigureSta) ? stopAP() : (operation.active ? startAP() : true);
        if (!modeOk) round.fail(-7); else stopForAttempt();
    }
    NetStage before = round.stage;
    round.tick(millis());
    if (round.stage == NetStage::Disconnecting && before != NetStage::Disconnecting) stopForAttempt();
    if (round.stage == NetStage::Disconnecting && stopFence) submitAttempt();
    if (operation.active) {
        operation.stage = round.stage; operation.attempt = round.attempt;
        operation.errorStage = round.errorStage; operation.stageError = round.error; operation.reason = round.reason;
        operation.seconds = round.stage == NetStage::Queued ? 0 : (millis()-round.started)/1000;
        operation.result = round.stage == NetStage::Queued ? "queued" : "testing";
    }
    if (round.stage == NetStage::Success || round.stage == NetStage::Failed) finishRound();
    const bool connected = networkConnected();
    if (connected != wasConnected) {
        wasConnected = connected; if (state.managementVisible) markDirty();

        if (!connected) { endMdns(); retryAt = millis()+3000; }
    }
    if (connected && !networkTesting() && !mdnsStarted) {
        mdnsStarted = MDNS.begin(host);
        if (mdnsStarted) MDNS.addService("http","tcp",80);
    }
    if (!connected && state.settings.ssid[0] && !state.workerBusy && !uploadBusy() &&
        mayAutoConnect(millis(),state.lastActivity,networkTesting()) && int32_t(millis()-retryAt) >= 0) beginRound(Purpose::Automatic);
}
void networkStatus(JsonObject object) {
    object["connected"] = networkConnected(); object["ssid"] = networkConnected() ? WiFi.SSID() : String(state.settings.ssid);
    object["ip"] = networkConnected() ? WiFi.localIP().toString() : apStarted ? "192.168.4.1" : "";
    object["rssi"] = networkConnected() ? WiFi.RSSI() : -100;
    object["hostname"] = String(host)+".local"; object["ap"] = apStarted; object["apSsid"] = apName;
    object["busy"] = networkTesting(); object["configuration"] = operation.diagnostic ? "idle" : operation.result;
    object["stage"] = stageName(operation.active ? operation.stage : round.stage);
    object["connectSeconds"] = operation.active ? operation.seconds : round.running() ? (millis()-round.started)/1000 : 0;
    object["error"] = netError; object["disconnectReason"] = disconnectReason;
    object["failureStatus"] = failureStatus; object["failureReason"] = failureReason;
    auto op = object["operation"].to<JsonObject>();
    op["id"] = operation.id; op["kind"] = operation.diagnostic ? "diagnose" : "configure";
    op["active"] = operation.active; op["targetSsid"] = operation.ssid;
    op["result"] = operation.result; op["stage"] = stageName(operation.stage); op["attempt"] = operation.attempt;
    op["stageError"] = operation.stageError; op["errorStage"] = stageName(operation.errorStage); op["reason"] = operation.reason;
    op["phase"] = operation.phase == 0 ? "ap_sta" : "sta"; op["seconds"] = operation.seconds;
    op["error"] = operation.error; op["savedIp"] = operation.ip;
    auto phases = op["phases"].to<JsonArray>();
    if (operation.diagnostic) for (const auto& phase : operation.phases) {
        auto item = phases.add<JsonObject>();
        item["complete"] = phase.complete; item["gotIp"] = phase.gotIp; item["durationMs"] = phase.durationMs;
        item["attempts"] = phase.attempts; item["rssi"] = phase.rssi; item["auth"] = phase.auth;
        item["authName"] = authName(phase.auth); item["channel"] = phase.channel;
        item["reason"] = phase.reason; item["error"] = phase.error;
        auto errors = item["errors"].to<JsonArray>(); for (size_t i = 0; i < phase.errorCount; ++i) errors.add(phase.errors[i]);
    }
    auto list = object["events"].to<JsonArray>();
    for (size_t i = 0; i < traceCount; ++i) {
        const auto& entry = trace[(traceNext+TraceLimit-traceCount+i)%TraceLimit];
        auto item = list.add<JsonObject>(); item["ms"] = entry.ms; item["event"] = entry.event; item["value"] = entry.value;
    }
}
void networkStop() {
    endMdns(); memset(operation.password,0,sizeof(operation.password));
    recordEvent("休眠停止无线",WiFi.mode(WIFI_OFF) ? 0 : -3); apStarted = false;
}
}
