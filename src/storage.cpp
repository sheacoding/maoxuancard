/**
 * [INPUT]: app、严格 JSON 解析、LittleFS、Preferences 与 ESP 分区检查
 * [OUTPUT]: 单卡原子提交、独立 NVS 设置与最多 130 项目录索引
 * [POS]: 主循环独占持久化；先完整写临时文件再 rename，失败不改变目录
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "app.h"
#include "network_policy.h"
#include "clock_policy.h"
#include "commit.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <algorithm>
namespace linkbit {
static Preferences prefs;static bool prefsReady=false;
static const char* Temp="/maoxuancard/commit.tmp";
static String path(uint32_t id) {return String("/maoxuancard/c")+String(id)+".json";}
static bool fail(const char* s) {copyText(state.storageError,s);return false;}
static bool blankPartition() {
    auto p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
    if(!p) return false;
    uint8_t bytes[256];
    for(size_t at=0;at<p->size;at+=sizeof(bytes)) {
        if(esp_partition_read(p,at,bytes,sizeof(bytes))!=ESP_OK) return false;
        for(auto b:bytes) if(b!=255) return false;
        delay(0);
    }return true;
}
bool saveSettings(const Settings& s) {
    if(!prefsReady) return fail("NVS 设置不可用，原配置保留");
    JsonDocument j;j["version"]=1;j["ssid"]=s.ssid;j["password"]=s.password;j["admin"]=s.admin;
    j["currentId"]=s.currentId;j["lastAutoDay"]=s.lastAutoDay;j["favoritesOnly"]=s.favoritesOnly;j["loginCompleted"]=s.loginCompleted;
    String body;serializeJson(j,body);
    if(prefs.putString("settings",body)!=body.length()) return fail("设置保存失败，原配置保留");
    return true;
}
static bool readFile(uint32_t id,Card& c) {
    File f=LittleFS.open(path(id),"r");JsonDocument j;
    if(!f||f.size()>4096||deserializeJson(j,f)||j["version"]!=1||j["id"]!=id) return false;
    if(id<=BuiltinCount) {
        if(!loadBuiltin(id,c)||!j["favorite"].is<bool>()||!j["illustration"].is<int>()||j["illustration"].as<int>()<0||j["illustration"].as<int>()>=ArtCount) return false;
        c.favorite=j["favorite"];c.illustration=j["illustration"];return true;
    }
    char error[160];if(!parseCard(j.as<JsonObjectConst>(),c,error,sizeof(error))) return false;
    c.id=id;return true;
}
bool readCard(uint32_t id,Card& c) {
    int i=findEntry(state.entries,state.count,id);if(i<0) return false;
    if(id<=BuiltinCount) {loadBuiltin(id,c);c.favorite=state.entries[i].favorite;c.illustration=state.entries[i].illustration;return true;}
    if(!state.storageReady||!readFile(id,c)) return fail("书摘文件读取失败，已保留原文件");
    return true;
}
static bool commit(const Card& c) {
    if(!state.storageReady) return fail("文件系统不可用，未修改已存内容");
    if(storageTotal()-storageUsed()<8192) return fail("存储空间不足，未修改已存内容");
    JsonDocument j;
    if(c.builtin()) {j["id"]=c.id;j["favorite"]=c.favorite;j["illustration"]=c.illustration;}
    else cardJson(c,j.to<JsonObject>());
    j["version"]=1;
    String data;serializeJson(j,data);
    if(data.length()!=measureJson(j)||!atomicWrite(LittleFS,Temp,path(c.id).c_str(),reinterpret_cast<const uint8_t*>(data.c_str()),data.length())) return fail("写入提交失败，原书摘保留");
    int i=findEntry(state.entries,state.count,c.id);
    if(i<0) i=state.count++;
    state.entries[i]={c.id,c.favorite,c.illustration};state.storageError[0]=0;return true;
}
bool saveCard(Card& c,bool& duplicate) {
    duplicate=false;
    if(c.id&&findEntry(state.entries,state.count,c.id)<0) return fail("书摘不存在");
    if(c.builtin()) return fail("精选原文只读，请复制为个人书摘");
    // ponytail: 最多 130 条，逐个读取去重；超出既定容量再引入磁盘索引。
    Card other;
    for(int i=0;i<state.count;++i) if(state.entries[i].id!=c.id&&readCard(state.entries[i].id,other)&&sameQuote(c,other)) {
        c.id=other.id;duplicate=true;return true;
    }
    if(!c.id) {
        if(state.count>=CatalogLimit) return fail("个人书摘最多 100 条");
        uint32_t id=prefs.getUInt("next",1001);
        while(id<UINT32_MAX&&LittleFS.exists(path(id))) ++id;
        if(!prefsReady||id==UINT32_MAX||prefs.putUInt("next",id+1)!=sizeof(id)) return fail("书摘编号保存失败");
        c.id=id;
    }
    return commit(c);
}
bool saveAppearance(uint32_t id,bool favorite,uint8_t illustration) {
    Card c;if(illustration>=ArtCount||!readCard(id,c)) return false;
    c.favorite=favorite;c.illustration=illustration;return commit(c);
}
bool deleteCard(uint32_t id) {
    int i=findEntry(state.entries,state.count,id);
    if(id<=BuiltinCount||i<0) return fail("仅可删除个人书摘");
    if(!state.storageReady||!LittleFS.remove(path(id))) return fail("删除失败，原书摘保留");
    for(int k=i+1;k<state.count;++k) state.entries[k-1]=state.entries[k];
    --state.count;return true;
}
size_t storageUsed() {return state.storageReady?LittleFS.usedBytes():0;}
size_t storageTotal() {return state.storageReady?LittleFS.totalBytes():0;}
void storageBegin() {
    for(uint32_t id=1;id<=BuiltinCount;++id) state.entries[state.count++]={id,false,Builtins[id-1].illustration};
    prefsReady=prefs.begin("maoxuancard",false);
    JsonDocument j;
    if(prefsReady&&!deserializeJson(j,prefs.getString("settings","{}"))&&j["version"]==1) {
        const char* ssid=j["ssid"]|"",*pass=j["password"]|"",*admin=j["admin"]|"";
        if(validWifi(ssid,strlen(ssid),pass,strlen(pass))) {copyText(state.settings.ssid,ssid);copyText(state.settings.password,pass);}
        if(strlen(admin)==6&&strspn(admin,"0123456789")==6) copyText(state.settings.admin,admin);
        state.settings.currentId=j["currentId"]|1U;
        uint32_t lastAutoDay=j["lastAutoDay"]|0U;if(validStoredDay(lastAutoDay)) state.settings.lastAutoDay=lastAutoDay;
        state.settings.favoritesOnly=j["favoritesOnly"]|false;
        state.settings.loginCompleted=j["loginCompleted"]|false;
    }
    if(!state.settings.admin[0]) {snprintf(state.settings.admin,7,"%06lu",static_cast<unsigned long>(esp_random()%1000000));state.settings.loginCompleted=false;saveSettings(state.settings);}
    state.storageReady=LittleFS.begin(false);
    if(!state.storageReady&&blankPartition()) state.storageReady=LittleFS.format()&&LittleFS.begin(false);
    if(!state.storageReady) {fail("文件系统挂载失败；未格式化已有数据");return;}
    if(!LittleFS.exists("/maoxuancard")&&!LittleFS.mkdir("/maoxuancard")) {state.storageReady=false;fail("应用目录创建失败");return;}
    LittleFS.remove(Temp);
    File dir=LittleFS.open("/maoxuancard");
    for(File f=dir.openNextFile();f;f=dir.openNextFile()) {
        unsigned long id=0;char tail=0;
        const char* name=strrchr(f.name(),'/');name=name?name+1:f.name();
        if(f.isDirectory()||sscanf(name,"c%lu.json%c",&id,&tail)!=1||id==0||(id>BuiltinCount&&id<1001)) continue;
        Card c;if(!readFile(id,c)) {fail("检测到损坏书摘，已跳过并保留原文件");continue;}
        int i=findEntry(state.entries,state.count,id);
        if(i<0) {if(state.count==CatalogLimit) {fail("文件数量超过 100 条个人书摘限制");continue;}i=state.count++;}
        state.entries[i]={c.id,c.favorite,c.illustration};
    }
    std::sort(state.entries,state.entries+state.count,[](const Entry&a,const Entry&b){return a.id<b.id;});
    if(findEntry(state.entries,state.count,state.settings.currentId)<0) state.settings.currentId=1;
}
}
