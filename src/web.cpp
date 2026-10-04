/**
 * [INPUT]: app 单写者接口、WebServer Digest、随固件嵌入的静态资源
 * [OUTPUT]: 卡片 CRUD、时间状态、外观/收藏、独立位图预览、上屏修订状态及配网 API
 * [POS]: HTTP 只执行有界主循环动作；慢屏刷新由后台接收不可变快照
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "app.h"
#include <WebServer.h>
#include <WiFi.h>
#include <uri/UriBraces.h>
#define ASSET(name,file) extern const uint8_t name##Start[] asm("_binary_web_" file "_start"); extern const uint8_t name##End[] asm("_binary_web_" file "_end");
ASSET(index,"index_html") ASSET(css,"style_css") ASSET(app,"app_js") ASSET(codec,"image_js") ASSET(data,"data_js") ASSET(arts,"arts_json")
namespace linkbit {
static WebServer server(80);
static char body[4097]{};static size_t bodySize=0;static bool bodyReady=false;
static uint8_t pixels[FrameBytes];
static void headers() {
    server.sendHeader("Cache-Control","no-store");server.sendHeader("X-Content-Type-Options","nosniff");
    server.sendHeader("Referrer-Policy","same-origin");
    server.sendHeader("Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
}
static void json(int code,JsonDocument& j) {String s;serializeJson(j,s);headers();server.send(code,"application/json; charset=utf-8",s);}
static void error(int code,const char* s) {JsonDocument j;j["error"]=s;json(code,j);}
static bool allowed(bool write=false) {
    String host=server.hostHeader(),plain=host;if(plain.endsWith(":80")) plain.remove(plain.length()-3);
    if(plain!=String(networkHost())+".local"&&plain!=WiFi.localIP().toString()&&plain!="192.168.4.1") {error(403,"请使用设备地址访问");return false;}
    String origin=server.header("Origin");
    if((origin.length()&&origin!=String("http://")+host)||server.header("Sec-Fetch-Site")=="cross-site") {error(403,"不允许跨站请求");return false;}
    if(!server.authenticate("admin",state.settings.admin)) {
        headers();server.requestAuthentication(DIGEST_AUTH,networkHost(),"用户名 admin；六位密码见屏幕。唤醒松开 B 后，长按 B 三秒显示入口。");return false;
    }
    if(write&&state.shuttingDown) {error(409,"设备休眠中，请按 B 唤醒");return false;}return true;
}
static void asset(const uint8_t* a,const uint8_t* b,const char* type) {
    if(!allowed()) return;if(server.uri()=="/") completeLogin();headers();server.send_P(200,type,reinterpret_cast<const char*>(a),b-a-1);
}
static void readBody() {
    if(!server.header("Content-Type").startsWith("application/json")) {error(415,"需要 JSON 正文");server.client().stop();return;}
    auto& raw=server.raw();
    if(raw.status==RAW_START) {
        bodySize=0;bodyReady=false;if(!allowed(true)) {server.client().stop();return;}
        if(server.header("Content-Length").toInt()>4096) {error(413,"正文超过 4096 字节");server.client().stop();}
    } else if(raw.status==RAW_WRITE) {
        if(bodySize+raw.currentSize>4096) {error(413,"正文过大");bodyReady=false;server.client().stop();return;}
        memcpy(body+bodySize,raw.buf,raw.currentSize);bodySize+=raw.currentSize;
    } else if(raw.status==RAW_END) {body[bodySize]=0;bodyReady=true;} else bodyReady=false;
}
static bool request(JsonDocument& j) {
    if(!allowed(true)) return false;
    bool ok=bodyReady&&!deserializeJson(j,static_cast<const char*>(body),bodySize)&&j.is<JsonObject>();
    memset(body,0,sizeof(body));bodyReady=false;if(!ok) {error(400,"JSON 正文无效");return false;}touch();return true;
}
static uint32_t routeId() {
    String s=server.pathArg(0);if(!s.length()||s.length()>10||strspn(s.c_str(),"0123456789")!=s.length()) return 0;
    unsigned long long n=strtoull(s.c_str(),nullptr,10);return n<=UINT32_MAX?uint32_t(n):0;
}
static void frame(const Card& c) {
    Layout l;char message[160];if(!composeCard(c,pixels,l,message,sizeof(message))) {error(422,message);return;}
    headers();server.sendHeader("X-Font-Size",String(l.fontSize));server.sendHeader("X-Art-Width",String(l.artWidth));
    server.setContentLength(sizeof(FrameHeader)-1+FrameBytes);server.send(200,"application/octet-stream","");
    server.sendContent(FrameHeader,sizeof(FrameHeader)-1);server.sendContent(reinterpret_cast<const char*>(pixels),FrameBytes);
}
static void save(bool update) {
    JsonDocument j;if(!request(j)) return;Card c;char message[160];Layout layout;
    if(!parseCard(j.as<JsonObjectConst>(),c,message,sizeof(message))) {error(400,message);return;}
    c.id=update?routeId():0;
    if(update&&(!c.id||c.builtin())) {error(409,"精选原文只读，请复制为个人书摘");return;}
    if(!composeCard(c,pixels,layout,message,sizeof(message))) {error(422,message);return;}
    bool duplicate=false;if(!saveCard(c,duplicate)) {error(507,state.storageError);return;}
    j.clear();j["id"]=c.id;j["duplicate"]=duplicate;json(duplicate||update?200:201,j);
}
static void wifi(bool diagnose) {
    JsonDocument j;if(!request(j)) return;
    if(!j["ssid"].is<const char*>()||!j["password"].is<const char*>()) {error(400,"请填写网络名称与密码");return;}
    auto ssid=j["ssid"].as<JsonString>(),pass=j["password"].as<JsonString>();char id[24];
    int result=configureNetwork(ssid.c_str(),ssid.size(),pass.c_str(),pass.size(),diagnose,id,sizeof(id));
    if(result!=202) {error(result,result==400?"网络名称、密码或认证方式无效":"设备正在连接或扫描");return;}
    j.clear();j["sessionId"]=id;json(202,j);
}
void webBegin() {
    const char* names[]={"Origin","Sec-Fetch-Site","Content-Length","Content-Type"};server.collectHeaders(names,4);
    server.on("/",HTTP_GET,[]{asset(indexStart,indexEnd,"text/html; charset=utf-8");});
    server.on("/style.css",HTTP_GET,[]{asset(cssStart,cssEnd,"text/css; charset=utf-8");});
    server.on("/app.js",HTTP_GET,[]{asset(appStart,appEnd,"text/javascript; charset=utf-8");});
    server.on("/image.js",HTTP_GET,[]{asset(codecStart,codecEnd,"text/javascript; charset=utf-8");});
    server.on("/data.js",HTTP_GET,[]{asset(dataStart,dataEnd,"text/javascript; charset=utf-8");});
    server.on("/arts.json",HTTP_GET,[]{asset(artsStart,artsEnd,"application/json");});
    server.on("/api/status",HTTP_GET,[]{
        if(!allowed()) return;JsonDocument j;j["version"]="1.1.3";j["count"]=state.count;j["personalLimit"]=PersonalLimit;
        j["currentId"]=state.desiredId;j["appliedId"]=state.appliedId;j["favoritesOnly"]=state.settings.favoritesOnly;
        j["desiredRevision"]=state.desiredRevision;j["activeRevision"]=state.activeRevision;j["appliedRevision"]=state.appliedRevision;j["failedRevision"]=state.failedRevision;
        j["workerBusy"]=state.workerBusy;j["queued"]=state.dirty;j["managementVisible"]=state.managementVisible;
        j["storageReady"]=state.storageReady;j["used"]=storageUsed();j["total"]=storageTotal();
        j["storageError"]=state.storageError;j["displayError"]=state.displayError;
        uint32_t idle=millis()-state.lastActivity;j["awakeSeconds"]=idle<AwakeMs?(AwakeMs-idle+999)/1000:0;
        networkStatus(j["network"].to<JsonObject>());clockStatus(j["clock"].to<JsonObject>());json(200,j);
    });
    server.on("/api/touch",HTTP_POST,[]{if(!allowed(true)) return;touch();JsonDocument j;j["ok"]=true;json(200,j);});
    server.on("/api/cards",HTTP_GET,[]{
        if(!allowed()) return;JsonDocument j;int offset=server.arg("offset").toInt();if(offset<0||offset>state.count) {error(400,"分页位置无效");return;}
        auto items=j["items"].to<JsonArray>();
        for(int i=offset;i<state.count&&i<offset+10;++i) {Card c;if(readCard(state.entries[i].id,c)) cardJson(c,items.add<JsonObject>());}
        j["next"]=offset+10<state.count?offset+10:-1;j["count"]=state.count;json(200,j);
    });
    server.on("/api/cards",HTTP_POST,[]{save(false);},readBody);
    server.on(UriBraces("/api/cards/{}"),HTTP_GET,[]{if(!allowed()) return;Card c;if(!readCard(routeId(),c)) {error(404,"卡片不存在或已损坏");return;}JsonDocument j;cardJson(c,j.to<JsonObject>());json(200,j);});
    server.on(UriBraces("/api/cards/{}"),HTTP_PUT,[]{save(true);},readBody);
    server.on(UriBraces("/api/cards/{}"),HTTP_DELETE,[]{
        if(!allowed(true)) return;uint32_t id=routeId();
        if(!deleteCard(id)) {error(409,state.storageError);return;}
        // 删除不会刷新屏幕；当前图像保留，下一次按键从目录重新选择。
        touch();JsonDocument j;j["ok"]=true;json(200,j);
    });
    server.on(UriBraces("/api/cards/{}/appearance"),HTTP_PUT,[]{
        JsonDocument j;if(!request(j)) return;
        if(!j["favorite"].is<bool>()||!j["illustration"].is<int>()||j["illustration"].as<int>()<0||j["illustration"].as<int>()>=ArtCount) {error(400,"收藏或配图无效");return;}
        if(!saveAppearance(routeId(),j["favorite"],j["illustration"])) {error(507,state.storageError);return;}
        j.clear();j["ok"]=true;json(200,j);
    },readBody);
    server.on("/api/preview",HTTP_POST,[]{
        JsonDocument j;if(!request(j)) return;Card c;char message[160];
        if(j["id"].is<uint32_t>()) {if(!readCard(j["id"],c)) {error(404,"卡片不存在");return;}}
        else if(!parseCard(j.as<JsonObjectConst>(),c,message,sizeof(message))) {error(400,message);return;}
        frame(c);
    },readBody);
    server.on("/api/display",HTTP_POST,[]{
        JsonDocument j;if(!request(j)) return;
        if(!j["id"].is<uint32_t>()) {error(400,"卡片编号无效");return;}
        if(!requestCard(j["id"])) {error(422,state.storageError[0]?state.storageError:state.displayError);return;}
        j.clear();j["revision"]=state.desiredRevision;json(202,j);
    },readBody);
    server.on("/api/collection",HTTP_PUT,[]{
        JsonDocument j;if(!request(j)) return;if(!j["favoritesOnly"].is<bool>()) {error(400,"集合设置无效");return;}
        if(!setCollection(j["favoritesOnly"])) {error(409,state.storageError);return;}
        j.clear();j["ok"]=true;json(200,j);
    },readBody);
    server.on("/api/wifi/scan",HTTP_POST,[]{if(!allowed(true)) return;if(!scanNetwork()) {error(409,"正在连接或扫描");return;}JsonDocument j;j["ok"]=true;json(202,j);});
    server.on("/api/wifi/scan",HTTP_GET,[]{if(!allowed()) return;JsonDocument j;networkScanStatus(j.to<JsonObject>());json(200,j);});
    server.on("/api/wifi",HTTP_PUT,[]{wifi(false);},readBody);
    server.on("/api/wifi/diagnose",HTTP_POST,[]{wifi(true);},readBody);
    server.onNotFound([]{if(allowed()) error(404,"地址不存在");});server.begin();
}
void webTick() {server.handleClient();}void webStop() {server.stop();}bool uploadBusy() {return false;}
}
