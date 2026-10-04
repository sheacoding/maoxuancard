/**
 * [INPUT]: core.h 的卡片和索引契约
 * [OUTPUT]: 严格 UTF-8/字段校验、原文去重、循环翻阅与不立即重复的随机选择
 * [POS]: 无 IO 的业务规则；网页输入、存储读取、实体按键共用
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "core.h"
#include <cctype>

namespace linkbit {
bool validText(const char* s,size_t maximum) {
    if(!s || !*s || strlen(s)>maximum) return false;
    bool visible=false;
    for(size_t i=0,n=strlen(s);i<n;) {
        uint8_t c=s[i];
        if(c<128) {if(c<32 && c!='\n') return false;visible|=c>32;++i;continue;}
        size_t k=c>=0xC2&&c<=0xDF?2:c>=0xE0&&c<=0xEF?3:c>=0xF0&&c<=0xF4?4:0;
        if(!k || i+k>n) return false;
        for(size_t j=1;j<k;++j) if((uint8_t(s[i+j])&0xC0)!=0x80) return false;
        uint8_t d=s[i+1];
        if((c==0xE0&&d<0xA0)||(c==0xED&&d>=0xA0)||(c==0xF0&&d<0x90)||(c==0xF4&&d>=0x90)) return false;
        visible=true;i+=k;
    }
    return visible;
}
uint32_t nextCodepoint(const char*& p) {
    uint8_t c=*p++; if(c<128) return c;
    int n=c<0xE0?1:c<0xF0?2:3;
    uint32_t v=c&((1u<<(6-n))-1);
    while(n--) v=(v<<6)|(uint8_t(*p++)&63);
    return v;
}
bool validUrl(const char* url) {
    if(!url[0]) return true;
    if(strlen(url)>256 || (strncmp(url,"https://",8)&&strncmp(url,"http://",7))) return false;
    const char* host=strstr(url,"://")+3;
    if(!*host || *host=='/' || *host=='.') return false;
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(url);*p;++p)
        if(*p<=32 || *p=='"' || *p=='<' || *p=='>' || *p=='\\') return false;
    return true;
}
bool validCard(const Card& c,char* error,size_t n) {
    const char* message=nullptr;
    if(!validText(c.text,512)) message="请填写有效原文（最多 512 个 UTF-8 字节）";
    else if(!validText(c.source,192) || strchr(c.source,'\n')) message="请填写有效篇名，篇名不能换行";
    else if(c.topic[0] && (!validText(c.topic,48) || strchr(c.topic,'\n'))) message="主题过长或包含无效字符";
    else if(c.volume>4 || c.illustration>=ArtCount) message="卷次或配图无效";
    else if(!validUrl(c.url)) message="出处链接须为完整的 HTTP 或 HTTPS 地址";
    if(message) {snprintf(error,n,"%s",message);return false;}
    return true;
}
bool sameQuote(const Card& a,const Card& b) {return !strcmp(a.text,b.text)&&!strcmp(a.source,b.source);}
int findEntry(const Entry* e,int n,uint32_t id) {for(int i=0;i<n;++i) if(e[i].id==id) return i;return -1;}
uint32_t navigate(const Entry* e,int n,uint32_t id,int direction,bool favorites) {
    if(n<=0) return 0;
    int at=findEntry(e,n,id);
    if(at<0) at=direction<0?0:n-1;
    for(int k=1;k<=n;++k) {int i=(at+(direction<0?-k:k)+n*2)%n;if(!favorites||e[i].favorite) return e[i].id;}
    return 0;
}
uint32_t randomCard(const Entry* e,int n,uint32_t id,bool favorites,uint32_t random) {
    uint32_t choices[CatalogLimit];int count=0;
    for(int i=0;i<n;++i) if(e[i].id!=id && (!favorites||e[i].favorite)) choices[count++]=e[i].id;
    if(count) return choices[random%count];
    int at=findEntry(e,n,id);return at>=0&&(!favorites||e[at].favorite)?id:0;
}
bool validFrame(const uint8_t* p) {
    for(int y=0;y<Height;++y) if((p[y*Stride+31]|p[BitmapBytes+y*Stride+31])&63) return false;
    for(int i=0;i<BitmapBytes;++i) if(p[i]&p[BitmapBytes+i]) return false;
    return true;
}
bool loadBuiltin(uint32_t id,Card& c) {
    if(id<1||id>BuiltinCount) return false;
    const auto& b=Builtins[id-1];c=Card{};c.id=id;
    copyText(c.text,b.text);copyText(c.source,b.source);copyText(c.url,b.url);copyText(c.topic,b.topic);
    c.volume=b.volume;c.illustration=b.illustration;return true;
}
}
