/**
 * [INPUT]: content 的原文 JSON、已注明出处的 JPEG 与 FFmpeg
 * [OUTPUT]: 内置语录 C++、八款 64×74 位图和网页同源配图数据
 * [POS]: 可重复生成的素材入口；固件运行时不需要 Bun、网络或 FFmpeg
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
import {mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
process.chdir(resolve(import.meta.dir,'..'));
const header=(description:string)=>`/**\n * [INPUT]: content/ 中已核对的输入，由 tools/prepare.ts 生成\n * [OUTPUT]: ${description}\n * [POS]: 只读固件资源，禁止手工修改生成数据\n * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md\n */\n#include "core.h"\nnamespace linkbit {\n`;
const quotes=await Bun.file('content/quotes.json').json();
if(quotes.length!==30||quotes.some((q:any,i:number)=>q.id!==i+1||q.volume<1||q.volume>4))throw Error('需要 30 条稳定编号、第一至第四卷的原文');
await Bun.write('src/content.cpp',header('30 条原文及篇名、卷次、主题与核对链接')+'const Builtin Builtins[BuiltinCount]={\n'+quotes.map((q:any)=>`    {${[q.text,q.source,q.url,q.topic].map(JSON.stringify).join(',')},${q.volume},${q.illustration}}`).join(',\n')+'\n};\n}\n');
await mkdir('.cache',{recursive:true});
const ArtWidth=64,ArtHeight=74,ArtStride=8,ArtBytes=ArtStride*ArtHeight;
const images:{name:string,width:number,height:number,pixels:number[]}[]=[];
for(const [year,name,crop] of [[1927,'青年肖像',''],[1937,'延安肖像','crop=220:270:15:25,']] as const){
  const output=`.cache/portrait-${year}.rgb`;
  const args=[process.env.FFMPEG||Bun.which('ffmpeg.exe')||'ffmpeg','-y','-v','error','-i',`content/portrait-${year}.jpg`,'-vf',`${crop}scale=${ArtWidth}:${ArtHeight}:force_original_aspect_ratio=decrease,pad=${ArtWidth}:${ArtHeight}:(ow-iw)/2:(oh-ih)/2:color=white,format=rgb24`,'-frames:v','1','-f','rawvideo',output];
  // Windows 的 FFmpeg 由 PowerShell 启动；二进制写文件，避开管道编码转换。
  const command=process.platform==='win32'?['powershell','-NoProfile','-NonInteractive','-Command','& '+args.map(s=>"'"+s.replaceAll("'","''")+"'").join(' ')]:args;
  const p=Bun.spawn(command,{stdout:'inherit',stderr:'inherit'});
  if(await p.exited)throw Error('FFmpeg 转换失败');
  const bytes=new Uint8Array(await Bun.file(output).arrayBuffer());if(bytes.length!==ArtWidth*ArtHeight*3)throw Error('肖像像素尺寸错误');
  const gray=new Float32Array(ArtWidth*ArtHeight),bits=new Uint8Array(ArtBytes);
  for(let y=0;y<ArtHeight;y++)for(let x=0;x<ArtWidth;x++){
    const i=(y*ArtWidth+x)*3,[r,g,b]=bytes.slice(i,i+3);let value=.299*r+.587*g+.114*b;
    if(year===1927&&b>r+12&&g>r+4)value=255;
    gray[y*ArtWidth+x]=Math.max(0,Math.min(255,(value-128)*1.18+140));
  }
  // --- Floyd–Steinberg 将灰阶误差扩散到邻点，保留五官和衣纹层次 ---
  for(let y=0;y<ArtHeight;y++)for(let x=0;x<ArtWidth;x++){
    const at=y*ArtWidth+x,old=gray[at],value=old<145?0:255,error=old-value;
    if(!value)bits[y*ArtStride+(x>>3)]|=128>>(x&7);
    if(x+1<ArtWidth)gray[at+1]+=error*7/16;
    if(y+1<ArtHeight){if(x)gray[at+ArtWidth-1]+=error*3/16;gray[at+ArtWidth]+=error*5/16;if(x+1<ArtWidth)gray[at+ArtWidth+1]+=error/16;}
  }
  images.push({name,width:ArtWidth,height:ArtHeight,pixels:[...bits]});
}
// --- 六幅原创简笔画：整数像素笔画，原尺寸避免细线消失 ---
for(let id=0;id<6;id++){
  const bits=new Uint8Array(ArtBytes);
  function dot(x:number,y:number){x=Math.round(x*ArtWidth/56);y=Math.round(y*ArtHeight/64);if(x>=0&&x<ArtWidth&&y>=0&&y<ArtHeight)bits[y*ArtStride+(x>>3)]|=128>>(x&7);}
  function line(x:number,y:number,u:number,v:number,w=1){const steps=Math.ceil(Math.max(Math.abs(u-x)*ArtWidth/56,Math.abs(v-y)*ArtHeight/64));for(let i=0;i<=steps;i++)for(let k=0;k<w;k++)dot(x+(u-x)*i/(steps||1)+k,y+(v-y)*i/(steps||1));}
  function path(points:number[][],w=1){for(let i=1;i<points.length;i++)line(...points[i-1] as [number,number],...points[i] as [number,number],w);}
  function circle(cx:number,cy:number,r:number){let prior=[cx+r,cy];for(let i=1;i<=80;i++){const p=[cx+r*Math.cos(i/80*Math.PI*2),cy+r*Math.sin(i/80*Math.PI*2)];path([prior,p]);prior=p;}}
  if(id===0){path([[6,16],[24,13],[27,16],[30,13],[48,16],[48,52],[30,49],[27,52],[24,49],[6,52],[6,16]],2);line(27,17,27,49);for(let y=24;y<44;y+=7){line(11,y,21,y-1);line(33,y-1,43,y);}path([[39,7],[43,9],[35,33],[31,37],[31,31],[39,7]],2);}
  if(id===1){path([[3,46],[18,18],[27,33],[36,13],[53,47]],2);path([[12,29],[18,25],[23,28]]);path([[30,24],[36,21],[41,25]]);path([[29,37],[22,44],[34,49],[19,56],[23,62]],2);line(2,55,13,55);line(41,53,54,53);circle(12,10,4);}
  if(id===2){path([[14,57],[27,42],[28,13]],2);path([[27,42],[40,27],[43,16]],2);for(let y=16;y<=37;y+=7){path([[28,y+6],[19,y-2],[20,y+4],[28,y+8]],1);path([[28,y+6],[36,y-2],[35,y+4],[28,y+8]],1);}path([[22,52],[12,45],[6,45],[14,53],[22,55]]);path([[25,46],[42,41],[50,43],[39,49],[25,49]]);}
  if(id===3){circle(28,33,22);circle(28,33,18);path([[28,14],[34,34],[28,52],[22,32],[28,14]],2);line(28,16,28,49);for(let k=0;k<4;k++){let a=k*Math.PI/2;line(28+Math.cos(a)*19,33+Math.sin(a)*19,28+Math.cos(a)*22,33+Math.sin(a)*22);}line(26,4,26,9);line(26,4,30,9);line(30,4,30,9);}
  if(id===4){line(28,14,28,54,2);path([[15,57],[40,57],[35,53],[21,53],[15,57]],2);line(9,22,47,18,2);circle(28,15,3);path([[12,23],[5,40],[20,40],[12,23]],1);path([[43,20],[35,37],[51,37],[43,20]],1);path([[5,40],[8,45],[17,45],[20,40]],2);path([[35,37],[39,42],[47,42],[51,37]],2);}
  if(id===5){path([[10,58],[45,58],[41,53],[15,53],[10,58]],2);path([[28,52],[38,31],[26,18]],2);path([[26,18],[17,17],[7,31],[30,37],[30,27],[26,18]],2);line(11,42,8,48);line(21,43,20,50);line(4,37,1,38);circle(38,31,3);}
  images.push({name:['调查笔记','山间小路','麦穗','指南针','天平','书桌灯'][id],width:ArtWidth,height:ArtHeight,pixels:[...bits]});
}
let cpp=header('两款历史肖像与六款原创主题简笔画');
for(let i=0;i<images.length;i++)cpp+=`static const uint8_t art${i}[ArtBytes]={\n${Array.from({length:Math.ceil(ArtBytes/16)},(_,n)=>'    '+images[i].pixels.slice(n*16,n*16+16).map(v=>'0x'+v.toString(16).padStart(2,'0')).join(',')).join(',\n')}\n};\n`;
cpp+='const Art Arts[ArtCount]={\n'+images.map((a,i)=>`    {${JSON.stringify(a.name)},art${i}}`).join(',\n')+'\n};\n}\n';
await Bun.write('src/assets.cpp',cpp);await Bun.write('web/arts.json',JSON.stringify(images));
const board=await Bun.file('board.json').json();
board.analogPins=[];
board.pwmPins=[['RGB','4']];
board.interruptPins=[['KEY_A','10'],['KEY_B','5'],['KEY_C','8'],['BOOT','9']];
await Bun.write('board.json',JSON.stringify(board,null,4)+'\n');
console.log('Generated 30 quotes, 8 illustrations, corrected board pin metadata.');
