/**
 * [INPUT]: C++ 标准库，不依赖设备、网络或存储
 * [OUTPUT]: 卡片、目录索引、按键、UTF-8 与双平面位图的共同契约
 * [POS]: 固件和主机检查的共享边界，容量与引脚只有这一处定义
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>

namespace linkbit {
constexpr int Width=250, Height=122, Stride=32, BitmapBytes=3904, FrameBytes=7808;
constexpr char FrameHeader[]="L2\n250 122\n";
constexpr int BuiltinCount=30, PersonalLimit=100, CatalogLimit=130, ArtCount=8;
constexpr int ArtWidth=64, ArtHeight=74, ArtStride=8, ArtBytes=ArtStride*ArtHeight;
constexpr int KeyA=10, KeyB=5, KeyC=8;
constexpr int ScreenCs=2, ScreenDc=3, ScreenRst=1, ScreenBusy=0, ScreenSck=6, ScreenMosi=7;
constexpr int AdminDigits=6;
constexpr uint32_t AwakeMs=300000, DebounceMs=35;
struct Card {
    uint32_t id=0;
    char text[513]{}, source[193]{}, url[257]{}, topic[49]{};
    uint8_t volume=0, illustration=0;
    bool favorite=false;
    bool builtin() const { return id>=1 && id<=BuiltinCount; }
};
struct Entry { uint32_t id=0; bool favorite=false; uint8_t illustration=0; };
struct Builtin { const char *text, *source, *url, *topic; uint8_t volume, illustration; };
extern const Builtin Builtins[BuiltinCount];
struct Art { const char* name; const uint8_t* pixels; };
extern const Art Arts[ArtCount]; // 64×74，逐行 8 字节，1 为黑
template<size_t N> inline void copyText(char (&dest)[N], const char* src) { snprintf(dest,N,"%s",src?src:""); }
inline bool elapsed(uint32_t now,uint32_t since,uint32_t interval) { return uint32_t(now-since)>=interval; }
bool validText(const char* text,size_t maxBytes);
bool validCard(const Card& card,char* error,size_t size);
bool validUrl(const char* url);
uint32_t nextCodepoint(const char*& p);
bool sameQuote(const Card& a,const Card& b);
int findEntry(const Entry* entries,int count,uint32_t id);
uint32_t navigate(const Entry* entries,int count,uint32_t current,int direction,bool favorites);
uint32_t randomCard(const Entry* entries,int count,uint32_t current,bool favorites,uint32_t random);
bool validFrame(const uint8_t* pixels);
bool loadBuiltin(uint32_t id,Card& card);
struct Button {
    int pin; bool raw=true,stable=true,armed=false; uint32_t changed=0;
    bool update(bool high,uint32_t now) {
        if(high!=raw) {raw=high;changed=now;}
        if(!elapsed(now,changed,DebounceMs)) return false;
        if(high) armed=true;
        if(stable==high) return false;
        stable=high; return !high && armed;
    }
};
enum class Press { None,Short,Long };
struct HoldPress {
    bool pressed=false,longSent=false; uint32_t pressedAt=0;
    Press update(const Button& button,bool edge,uint32_t now) {
        if(edge) {pressed=true;longSent=false;pressedAt=now;}
        if(!pressed) return Press::None;
        if(button.stable) {pressed=false;return longSent?Press::None:Press::Short;}
        if(!button.raw && !longSent && elapsed(now,pressedAt,3000)) {longSent=true;return Press::Long;}
        return Press::None;
    }
};
struct Layout { int fontSize=0, lines=0, artWidth=0; };
struct RefreshState {
    bool workerBusy=false,dirty=false;
    uint32_t dirtyAt=0,desiredRevision=0,activeRevision=0,appliedRevision=0,failedRevision=0;
    void request(uint32_t now) {dirty=true;dirtyAt=now;++desiredRevision;}
    bool ready(uint32_t now) const {return !workerBusy&&dirty&&elapsed(now,dirtyAt,350);}
    void begin() {workerBusy=true;dirty=false;activeRevision=desiredRevision;}
    void finish(uint32_t revision,bool ok) {workerBusy=false;if(ok)appliedRevision=revision;else failedRevision=revision;}
};
bool composeCard(const Card& card,uint8_t* pixels,Layout& layout,char* error,size_t size);
void composeManagement(const char* address,const char* password,uint8_t* pixels);
}
