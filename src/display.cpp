/**
 * [INPUT]: 不可变黑红位图、GxEPD2 Z98 驱动及 core 中的引脚
 * [OUTPUT]: 250×122 横屏全刷与 BUSY 完成判定
 * [POS]: 后台任务独占硬件，字体、网络和文件不进入此边界
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "app.h"
#include <SPI.h>
#include <GxEPD2_3C.h>
namespace linkbit {
static GxEPD2_3C<GxEPD2_213_Z98c,GxEPD2_213_Z98c::HEIGHT> display(GxEPD2_213_Z98c(ScreenCs,ScreenDc,ScreenRst,ScreenBusy));
static bool initialized=false;
bool displayFrame(const uint8_t* pixels,char* error,size_t n) {
    if(!validFrame(pixels)) {snprintf(error,n,"位图无效");return false;}
    if(!initialized) {
        for(int pin:{ScreenCs,ScreenDc,ScreenRst}) {pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
        SPI.begin(ScreenSck,-1,ScreenMosi,ScreenCs);display.init(115200,true,2,false);display.setRotation(3);
        if(display.width()!=Width||display.height()!=Height) {snprintf(error,n,"屏幕尺寸不符");return false;}
        initialized=true;
    }
    const uint32_t started=millis();display.setFullWindow();display.firstPage();
    do {display.fillScreen(GxEPD_WHITE);display.drawBitmap(0,0,pixels,Width,Height,GxEPD_BLACK);display.drawBitmap(0,0,pixels+BitmapBytes,Width,Height,GxEPD_RED);} while(display.nextPage());
    if(digitalRead(ScreenBusy)==HIGH||millis()-started<1000) {snprintf(error,n,"屏幕 BUSY 超时或刷新耗时异常，未确认成功");initialized=false;return false;}
    display.hibernate();return true;
}
void displaySleep() {if(initialized) display.hibernate();}
}
