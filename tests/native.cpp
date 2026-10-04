/**
 * [INPUT]: 固件 core、clock_policy、render、card_json 及真实 U8g2 字库
 * [OUTPUT]: assert 回归检查与 JSON → L2 位图的主机命令行
 * [POS]: 浏览器开发服务使用同一排版二进制，测试不模拟文字尺寸
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "core.h"
#include "card_json.h"
#include "network_policy.h"
#include "clock_policy.h"
#include "commit.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace linkbit;
struct TestFs {
    std::map<std::string,std::string> files;bool shortWrite=false,cut=false,renameFails=false;
    struct File {TestFs& fs;std::string path;operator bool() const{return true;}
        size_t write(const uint8_t* data,size_t n){if(fs.shortWrite)--n;fs.files[path]=std::string(reinterpret_cast<const char*>(data),n);return n;}
        void flush(){if(fs.cut)throw std::runtime_error("power cut before commit");}
        size_t size(){return fs.files[path].size();}void close(){}
    };
    File open(const char* path,const char*){files[path]="";return {*this,path};}
    bool rename(const char* from,const char* to){if(renameFails)return false;files[to]=files[from];files.erase(from);return true;}
};
static void check(){
    const uint8_t data[]="new card";TestFs fs;fs.files["saved"]="old card";
    fs.shortWrite=true;assert(!atomicWrite(fs,"temp","saved",data,8)&&fs.files["saved"]=="old card");
    fs.shortWrite=false;fs.renameFails=true;assert(!atomicWrite(fs,"temp","saved",data,8)&&fs.files["saved"]=="old card");
    fs.renameFails=false;fs.cut=true;try{atomicWrite(fs,"temp","saved",data,8);assert(false);}catch(const std::runtime_error&){}assert(fs.files["saved"]=="old card");
    fs.cut=false;assert(atomicWrite(fs,"temp","saved",data,8)&&fs.files["saved"]=="new card");
    RefreshState refresh;refresh.request(0);assert(!refresh.ready(349)&&refresh.ready(350));refresh.begin();
    refresh.request(400);refresh.request(500);assert(refresh.desiredRevision==3&&!refresh.ready(10000));
    refresh.finish(1,true);assert(refresh.appliedRevision==1&&refresh.dirty&&refresh.ready(10000));refresh.begin();assert(refresh.activeRevision==3);
    refresh.finish(3,false);assert(refresh.failedRevision==3&&refresh.appliedRevision==1&&!refresh.workerBusy);
    assert(validText("调查",512));assert(!validText("  ",512));assert(!validText("\xC0\x80",512));assert(!validText("\xED\xA0\x80",512));
    Entry e[]={{1,false,0},{2,true,0},{3,true,0}};
    assert(navigate(e,3,1,-1,false)==3);assert(navigate(e,3,3,1,false)==1);assert(navigate(e,3,1,-1,true)==3);
    for(uint32_t i=0;i<100;i++)assert(randomCard(e,3,2,true,i)==3);
    for(auto& item:e)item.favorite=false;
    assert(navigate(e,3,1,1,true)==0);assert(randomCard(e,3,1,true,0)==0);
    Button b{5};b.raw=b.stable=false;HoldPress hold;
    assert(!b.update(false,100));assert(hold.update(b,false,4000)==Press::None);
    b.update(true,4100);b.update(true,4200);b.update(false,4300);assert(b.update(false,4400));
    assert(hold.update(b,true,4400)==Press::None);assert(hold.update(b,false,7399)==Press::None);assert(hold.update(b,false,7400)==Press::Long);
    b.update(true,7500);b.update(true,7600);assert(hold.update(b,false,7600)==Press::None);
    b.update(false,7700);bool edge=b.update(false,7800);hold.update(b,edge,7800);b.update(true,7900);b.update(true,8000);assert(hold.update(b,false,8000)==Press::Short);
    ConnectionRound net;net.queue();net.start(0);net.stopped(10);net.tick(30001);assert(net.stage==NetStage::Failed);
    assert(eventBelongsToAttempt(101,100)&&!eventBelongsToAttempt(99,100));
    assert(eventBelongsToAttempt(1,UINT32_MAX));
    assert(shouldRetryStaOnly(true,false,true,0,false));
    assert(!shouldRetryStaOnly(true,false,true,1,false)&&!shouldRetryStaOnly(true,false,true,0,true));
    ClockPoint clock{true,20260913,7*3600+57*60+59};
    assert(validStoredDay(0)&&validStoredDay(20260913)&&validStoredDay(20280229));
    assert(!validStoredDay(123)&&!validStoredDay(UINT32_MAX)&&!validStoredDay(20260229)&&!validStoredDay(20261301)&&!validStoredDay(20260931));
    assert(!automaticDue(clock,0)&&nextWakeSeconds(clock,0)==1);
    clock.second=PrewakeSecond;assert(!automaticDue(clock,0)&&nextWakeSeconds(clock,0)==120);
    clock.second=AutoSecond;assert(automaticDue(clock,0)&&nextWakeSeconds(clock,0)==UnknownClockRetrySeconds);
    assert(!automaticDue(clock,20260913)&&nextWakeSeconds(clock,20260913)==SecondsPerDay-AutoSecond+PrewakeSecond);
    clock={true,20261001,AutoSecond+1};assert(automaticDue(clock,20260930));
    clock={true,20270101,AutoSecond+1};assert(automaticDue(clock,20261231));
    clock={true,20260915,AutoSecond+1};assert(automaticDue(clock,20260913));assert(!automaticDue(clock,20260915));
    clock={true,20260913,0};assert(nextWakeSeconds(clock,0)==PrewakeSecond);
    clock={true,20260913,SecondsPerDay-1};assert(nextWakeSeconds(clock,20260913)==PrewakeSecond+1);
    clock={false,0,0};assert(!automaticDue(clock,0)&&nextWakeSeconds(clock,0)==UnknownClockRetrySeconds);
    uint8_t pixels[FrameBytes];char error[160];Layout layout;Card c;
    for(uint32_t id=1;id<=30;id++)for(int a=0;a<8;a++){
        loadBuiltin(id,c);c.illustration=a;bool ok=composeCard(c,pixels,layout,error,sizeof(error));
        if(!ok)std::cerr<<"card "<<id<<": "<<error<<"\n";
        assert(ok);assert(validFrame(pixels));
    }
    loadBuiltin(1,c);copyText(c.text,"短句");assert(composeCard(c,pixels,layout,error,sizeof(error))&&layout.fontSize==32);
    copyText(c.text,"\xF0\x9F\x98\x80");assert(!composeCard(c,pixels,layout,error,sizeof(error)));assert(strstr(error,"U+1F600"));
    std::string tooLong;for(int i=0;i<165;i++)tooLong+="实";copyText(c.text,tooLong.c_str());assert(!composeCard(c,pixels,layout,error,sizeof(error)));
    loadBuiltin(1,c);copyText(c.text,"调查，研究。（实践）\n解决问题。");assert(composeCard(c,pixels,layout,error,sizeof(error)));
    JsonDocument j;deserializeJson(j,"{\"text\":\"abc\\u0000x\",\"source\":\"test\"}");assert(!parseCard(j.as<JsonObjectConst>(),c,error,sizeof(error)));
    deserializeJson(j,"{\"text\":\"hello\",\"source\":\"test\",\"illustration\":256}");assert(!parseCard(j.as<JsonObjectConst>(),c,error,sizeof(error)));
    std::cout<<"native: atomic saves, refresh, navigation, buttons, daily clock, UTF-8/JSON, network deadline, 240 real-font layouts passed\n";
}
int main(int argc,char** argv){
    if(argc==1){check();return 0;}
    Card c;char error[160]{};Layout layout;uint8_t pixels[FrameBytes];
    if(!strcmp(argv[1],"render")){
        std::ifstream file;if(argc>2)file.open(argv[2]);std::istream& stream=argc>2?file:std::cin;
        std::string input((std::istreambuf_iterator<char>(stream)),{});JsonDocument j;
        if(deserializeJson(j,input)||!parseCard(j.as<JsonObjectConst>(),c,error,sizeof(error))){if(argc>3)std::ofstream(std::string(argv[3])+".meta")<<error;else std::cerr<<error;return 2;}
    }else if(!strcmp(argv[1],"builtin")&&argc>2){if(!loadBuiltin(atoi(argv[2]),c))return 2;}
    else return 2;
    if(!composeCard(c,pixels,layout,error,sizeof(error))){if(argc>3)std::ofstream(std::string(argv[3])+".meta")<<error;else std::cerr<<error;return 2;}
    if(argc>3){
        std::ofstream out(argv[3],std::ios::binary);out.write(FrameHeader,sizeof(FrameHeader)-1);out.write(reinterpret_cast<const char*>(pixels),FrameBytes);
        std::ofstream(std::string(argv[3])+".meta")<<layout.fontSize<<" "<<layout.lines<<" "<<layout.artWidth;return out?0:2;
    }
    std::cerr<<layout.fontSize<<" "<<layout.lines<<" "<<layout.artWidth;
#ifdef _WIN32
    _setmode(_fileno(stdout),_O_BINARY);
#endif
    fwrite(FrameHeader,1,sizeof(FrameHeader)-1,stdout);fwrite(pixels,1,FrameBytes,stdout);return 0;
}
