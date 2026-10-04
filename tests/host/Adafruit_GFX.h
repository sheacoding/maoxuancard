/**
 * [INPUT]: 标准整数类型
 * [OUTPUT]: 主机端最小 GFX/Print 像素接口
 * [POS]: 只替代 Arduino 外壳，字体解码与排版使用生产实现
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
class Print {public:virtual size_t write(uint8_t)=0;virtual ~Print()=default;};
class Adafruit_GFX {
public:Adafruit_GFX(int16_t,int16_t) {} virtual ~Adafruit_GFX()=default;
virtual void drawPixel(int16_t,int16_t,uint16_t)=0;
void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c){for(int i=0;i<w;i++)drawPixel(x+i,y,c);}
void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c){for(int i=0;i<h;i++)drawPixel(x,y+i,c);}
};
