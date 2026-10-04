/**
 * [INPUT]: card_json.h 与 core 的语义校验
 * [OUTPUT]: 卡片 JSON 读写；来源类别由稳定 ID 决定，输入不能自封核验
 * [POS]: 主机预览、HTTP 和磁盘文件采用同一解析规则
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "card_json.h"
namespace linkbit {
template<size_t N> static bool field(JsonVariantConst v,char (&out)[N],bool required=false) {
    if(v.isNull()&&!required) {out[0]=0;return true;}
    if(!v.is<const char*>()) return false;
    JsonString s=v.as<JsonString>();
    if(s.size()>=N||strlen(s.c_str())!=s.size()) return false;
    copyText(out,s.c_str());return true;
}
bool parseCard(JsonObjectConst j,Card& c,char* error,size_t n) {
    c=Card{};
    if(!field(j["text"],c.text,true)||!field(j["source"],c.source,true)||!field(j["url"],c.url)||!field(j["topic"],c.topic)||
       (!j["volume"].isNull()&&(!j["volume"].is<int>()||j["volume"].as<int>()<0||j["volume"].as<int>()>4))||
       (!j["illustration"].isNull()&&(!j["illustration"].is<int>()||j["illustration"].as<int>()<0||j["illustration"].as<int>()>=ArtCount))||
       (!j["favorite"].isNull()&&!j["favorite"].is<bool>())) {
        snprintf(error,n,"卡片字段类型、长度或字符无效");return false;
    }
    c.volume=j["volume"]|0;c.illustration=j["illustration"]|0;c.favorite=j["favorite"]|false;
    return validCard(c,error,n);
}
void cardJson(const Card& c,JsonObject j) {
    j["id"]=c.id;j["text"]=c.text;j["source"]=c.source;j["url"]=c.url;j["topic"]=c.topic;
    j["volume"]=c.volume;j["illustration"]=c.illustration;j["favorite"]=c.favorite;
    j["origin"]=c.builtin()?"curated":"personal";j["verified"]=c.builtin();
}
}
