/**
 * [INPUT]: 真实原生排版、网页数据边界与位图解码
 * [OUTPUT]: 导入导出/去重/字节校验与全部卡面的可视化检查图
 * [POS]: Bun 无框架回归入口，失败立即退出
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
import assert from 'node:assert/strict';
import {mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {parseImport,exportData,validateCard,key} from '../web/data.js';
import {decodeFrame} from '../web/image.js';
import {render} from '../tools/render';
process.chdir(resolve(import.meta.dir,'..'));
await mkdir('artifacts',{recursive:true});
const quotes=(await Bun.file('content/quotes.json').json()).map((c:any)=>({...c,favorite:false}));
const arts=await Bun.file('web/arts.json').json();assert(arts.every((a:any)=>a.width===64&&a.height===74&&a.pixels.length===592));
const personal={...quotes[0],id:1001,origin:'personal',verified:false,text:'从实际出发。',favorite:true};
const exported=exportData([...quotes,personal],true);assert(!JSON.stringify(exported).includes('password'));
assert.deepEqual(parseImport(JSON.stringify(exported)),{personal:exported.personal,appearance:exported.appearance,favoritesOnly:true});
assert.equal(key(personal),key({...personal,illustration:5}));
for(const text of ['{}','not json',JSON.stringify({...exported,personal:Array(101).fill(personal)}),JSON.stringify({...exported,personal:[personal,personal]}),JSON.stringify({...exported,appearance:[{id:900,favorite:true,illustration:0}]})])assert.throws(()=>parseImport(text));
for(const c of [{...personal,text:''},{...personal,illustration:8},{...personal,url:'javascript:alert(1)'},{...personal,text:'abc\0x'},{...personal,volume:256},{...personal,source:'a\nb'}])assert.throws(()=>validateCard(c));
const width=5*270,height=6*155,rgb=new Uint8Array(width*height*3).fill(244),layouts=[];
for(const [index,q] of quotes.entries()){
  const {bytes:buffer,details:meta}=await render(q);
  const bytes=new Uint8Array(buffer),rgba=decodeFrame(bytes);await Bun.write(`artifacts/card-${String(q.id).padStart(2,'0')}.l2`,bytes);
  assert(rgba.reduce((n,_,i)=>i%4===0&&rgba[i]===174?n+1:n,0)>60,'语录正文应使用红色平面');
  layouts.push({id:q.id,font:Number(meta.split(' ')[0]),lines:Number(meta.split(' ')[1]),artWidth:Number(meta.split(' ')[2])});
  const dx=(index%5)*270+10,dy=Math.floor(index/5)*155+12;
  for(let y=0;y<122;y++)for(let x=0;x<250;x++)for(let c=0;c<3;c++)rgb[((dy+y)*width+dx+x)*3+c]=rgba[(y*250+x)*4+c];
  if(index===0){const bad=bytes.slice();bad[11+31]|=1;assert.throws(()=>decodeFrame(bad));}
}
assert(layouts.every((l:any)=>l.artWidth===64||l.artWidth===40)&&layouts.some((l:any)=>l.artWidth===64));
await Bun.write('artifacts/cards.ppm',new Blob([`P6\n${width} ${height}\n255\n`,rgb]));await Bun.write('artifacts/layouts.json',JSON.stringify(layouts,null,2));
console.log('Bun: import/export boundaries, duplicate identity, all 30 real frame decodes passed.');
