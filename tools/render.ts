/**
 * [INPUT]: JSON 卡片与 tests/native.cpp 编译产物
 * [OUTPUT]: 真实 L2 位图和排版元数据
 * [POS]: 主机工具共用的文件桥，规避 Windows Bun 子进程二进制管道限制
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
import {unlink} from 'node:fs/promises';
export async function render(card:unknown){
  const stem=`.cache/render-${crypto.randomUUID()}`,input=stem+'.json',output=stem+'.l2',meta=output+'.meta';
  try{
    await Bun.write(input,JSON.stringify(card));
    const p=Bun.spawn(['.cache/native.exe','render',input,output],{stdin:'ignore',stdout:'inherit',stderr:'inherit'});
    const exit=await p.exited;const details=await Bun.file(meta).text();if(exit)throw Error(details);
    return {bytes:await Bun.file(output).arrayBuffer(),details};
  }finally{await Promise.all([input,output,meta].map(p=>unlink(p).catch(()=>{})));}
}
