/**
 * [INPUT]: core.h、既有 U8g2 12/14/16 字库和八幅 64×74 单色配图
 * [OUTPUT]: 原尺寸黑红双平面、严格排版结果和按需管理页；不执行屏幕 IO
 * [POS]: 主循环与主机测试共用的唯一卡面实现，预览和上屏字形完全相同
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "core.h"
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <algorithm>

namespace linkbit {
class FrameCanvas:public Adafruit_GFX {
public:
    uint8_t* data=nullptr;int scale=1,ox=0,oy=0;bool clipped=false;
    FrameCanvas():Adafruit_GFX(Width,Height) {}
    void drawPixel(int16_t x,int16_t y,uint16_t color) override {
        for(int j=0;j<scale;++j) for(int i=0;i<scale;++i) {
            int px=ox+x*scale+i,py=oy+y*scale+j;
            if(px<0||py<0||px>=Width||py>=Height) {clipped=true;continue;}
            int pos=py*Stride+px/8;uint8_t bit=0x80>>(px%8);
            data[pos]&=~bit;data[pos+BitmapBytes]&=~bit;
            if(color) data[pos+(color==2?BitmapBytes:0)]|=bit;
        }
    }
};
static FrameCanvas canvas;
static U8G2_FOR_ADAFRUIT_GFX font;
static bool initialized=false;
static void prepare(uint8_t* pixels) {
    if(!initialized) {font.begin(canvas);font.setFontMode(1);initialized=true;}
    canvas.data=pixels;canvas.scale=1;canvas.ox=canvas.oy=0;canvas.clipped=false;
    memset(pixels,0,FrameBytes);font.setForegroundColor(1);
}
static void selectFont(int px) {font.setFont(px==12?u8g2_font_wqy12_t_gb2312:px==14?u8g2_font_wqy14_t_gb2312:u8g2_font_wqy16_t_gb2312);}
static int glyphHeight() {return font.u8g2.font_info.max_char_height;}
static int glyphAscent() {return glyphHeight()+font.u8g2.font_info.y_offset;}
struct Line {char text[513]{};int width=0;};
static bool closing(uint32_t c) {return c==0xFF0C||c==0x3002||c==0xFF01||c==0xFF1F||c==0xFF1B||c==0xFF1A||c==0x3001||c==0x201D||c==0x2019||c==0x300B||c==')'||c==0xFF09;}
static bool opening(uint32_t c) {return c==0x201C||c==0x2018||c==0x300A||c=='('||c==0xFF08;}
// --- 在字形真实 advance 上断行，闭标点不行首、开标点不行尾 ---
static bool wrap(const char* text,int width,int scale,Line* lines,int maximum,int& count) {
    struct Glyph {const char* p;int bytes,width;uint32_t cp;};
    // ponytail: 排版只由主循环串行调用，静态工作区避免耗尽 ESP32 的任务栈。
    static Glyph glyphs[513];int n=0;
    for(const char* p=text;*p;) {
        const char* begin=p;uint32_t cp=nextCodepoint(p);
        if(cp>0xFFFF) return false;
        int w=cp=='\n'?0:u8g2_GetGlyphWidth(&font.u8g2,uint16_t(cp))*scale;
        if(cp!='\n'&&w<=0) return false;
        glyphs[n++]={begin,int(p-begin),w,cp};
    }
    count=0;
    for(int at=0;at<n;) {
        if(count>=maximum) return false;
        int end=at,total=0;
        while(end<n&&glyphs[end].cp!='\n'&&total+glyphs[end].width<=width) total+=glyphs[end++].width;
        if(end==at && glyphs[at].cp!='\n') return false;
        if(end<n&&glyphs[end].cp!='\n') {
            while(end>at&&closing(glyphs[end].cp)) --end;
            while(end>at&&opening(glyphs[end-1].cp)) --end;
            if(end==at) return false;
        }
        Line& line=lines[count++];line=Line{};size_t bytes=0;
        for(int k=at;k<end;++k) {memcpy(line.text+bytes,glyphs[k].p,glyphs[k].bytes);bytes+=glyphs[k].bytes;line.width+=glyphs[k].width;}
        line.text[bytes]=0;at=end;
        if(at<n&&glyphs[at].cp=='\n') ++at;
    }
    return count>0;
}
static void lineAt(const Line& line,int x,int top,int scale,uint16_t color=1) {
    canvas.scale=scale;canvas.ox=x;canvas.oy=top;
    // --- getFontAscent() 只有拉丁 A 的度量；中文须使用整个字库的字形框 ---
    font.setForegroundColor(color);font.drawUTF8(0,glyphAscent(),line.text);
    canvas.scale=1;canvas.ox=canvas.oy=0;
}
static void artAt(int id,int w,int h,int x,int y) {
    const uint8_t* art=Arts[id].pixels;
    for(int py=0;py<h;++py) for(int px=0;px<w;++px) {
        int sx=px*ArtWidth/w,sy=py*ArtHeight/h;
        if(art[sy*ArtStride+sx/8]&(0x80>>(sx%8))) canvas.drawPixel(x+px,y+py,1);
    }
}
bool composeCard(const Card& card,uint8_t* pixels,Layout& layout,char* error,size_t size) {
    layout=Layout{};if(!validCard(card,error,size)) return false;
    prepare(pixels);
    // --- 先用全字号字库验证字符，不能把缺字替换为问号 ---
    selectFont(14);
    for(const char* text:{card.text,card.source}) for(const char* p=text;*p;) {
        uint32_t cp=nextCodepoint(p);
        if(cp!='\n'&&(cp>0xFFFF||u8g2_GetGlyphWidth(&font.u8g2,uint16_t(cp))<=0)) {
            snprintf(error,size,"字库缺少 U+%04lX，请使用可显示的原文字符",static_cast<unsigned long>(cp));return false;
        }
    }
    char attribution[201];snprintf(attribution,sizeof(attribution),"《%s》",card.source);
    static Line source[2];int sourceCount=0;selectFont(12);
    if(!wrap(attribution,Width-12,1,source,2,sourceCount)) {snprintf(error,size,"篇名超过两行或字库不支持，无法完整上屏");return false;}
    int sourceHeight=sourceCount*14,bodyHeight=Height-12-sourceHeight-6;
    static Line body[9];int count=0,chosen=0,artW=0,artH=0,scale=1;
    for(int px:{32,24,16,14}) {
        int base=px>16?px/2:px;selectFont(base);
        scale=px>16?2:1;
        int lineHeight=glyphHeight()*scale+3;
        for(int aw:{64,40}) {
            int maxLines=std::min(9,(bodyHeight+3)/lineHeight);
            if(wrap(card.text,Width-12-aw-8,scale,body,maxLines,count)) {chosen=px;artW=aw;artH=aw==64?74:46;break;}
        }
        if(chosen) break;
    }
    if(!chosen) {snprintf(error,size,"原文放不下一屏，请选择较短的原文节选；不会截断或改写");return false;}
    int lh=glyphHeight()*scale+3;
    int top=6+std::max(0,(bodyHeight-(count*lh-3))/2);
    for(int i=0;i<count;++i) lineAt(body[i],6,top+i*lh,scale,2);
    artAt(card.illustration,artW,artH,Width-6-artW,6+bodyHeight-artH);
    selectFont(12);
    for(int i=0;i<sourceCount;++i) lineAt(source[i],6,Height-6-sourceHeight+i*14,1);
    canvas.drawFastHLine(6,Height-6-sourceHeight-5,20,2);
    if(canvas.clipped||!validFrame(pixels)) {snprintf(error,size,"排版超出屏幕边界");return false;}
    layout={chosen,count,artW};return true;
}
void composeManagement(const char* address,const char* password,uint8_t* pixels) {
    prepare(pixels);selectFont(16);
    Line l;copyText(l.text,"手机管理");lineAt(l,6,6,1,2);
    selectFont(12);
    copyText(l.text,address);lineAt(l,6,34,1);
    snprintf(l.text,sizeof(l.text),"admin / %s",password);lineAt(l,6,55,1);
    copyText(l.text,"连接设备热点，浏览器打开地址");lineAt(l,6,78,1);
    copyText(l.text,"按任意短键返回当前卡片");lineAt(l,6,99,1);
}
}
