/**
 * [INPUT]: .pio 固定版 U8g2、ArduinoJson 与本机 GCC
 * [OUTPUT]: 真实字体和固件逻辑组成的主机检查/预览二进制
 * [POS]: 不复制字体算法；只提取使用的三个完整字库以缩短主机构建
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
import {mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
process.chdir(resolve(import.meta.dir,'..'));await mkdir('.cache',{recursive:true});
const lib='.pio/libdeps/esp32c3/U8g2_for_Adafruit_GFX/src';
const text=await Bun.file(`${lib}/u8g2_fonts.c`).text();
const fonts=[12,14,16].map(n=>{const start=text.indexOf(`const uint8_t u8g2_font_wqy${n}_t_gb2312[`);const end=text.indexOf('\n#endif',start);if(start<0||end<0)throw Error('字库缺失');return text.slice(start,end);});
await Bun.write('.cache/fonts.c',text.slice(0,text.indexOf('*/')+2)+'\n#include "u8g2_fonts.h"\n'+fonts.join('\n'));
async function run(args:string[]){const p=Bun.spawn(args,{stdout:'inherit',stderr:'inherit'});if(await p.exited)throw Error(args[0]+' failed');}
await run(['gcc','-O2','-c','-I'+lib,'.cache/fonts.c','-o','.cache/fonts.o']);
await run(['g++','-std=c++17','-O2','-Wall','-Wextra','-Itests/host','-Isrc','-I'+lib,'-I.pio/libdeps/esp32c3/ArduinoJson/src','src/core.cpp','src/card_json.cpp','src/content.cpp','src/assets.cpp','src/render.cpp',`${lib}/U8g2_for_Adafruit_GFX.cpp`,'tests/native.cpp','.cache/fonts.o','-o','.cache/native.exe']);
await run(['.cache/native.exe']);
