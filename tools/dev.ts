/**
 * [INPUT]: 原生排版二进制、实际网页与 30 条固件内容
 * [OUTPUT]: 仅绑定本机的管理演示服务，明确模拟设备与慢刷状态
 * [POS]: UI 验证替身；位图真实，网络、持久化和物理屏幕不作为实机证据
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
import {resolve} from 'node:path';
import {validateCard,key} from '../web/data.js';
import {render} from './render';
process.chdir(resolve(import.meta.dir,'..'));
let cards=(await Bun.file('content/quotes.json').json()).map((c:any)=>({...c,favorite:false}));
let next=1001,revision=1,active=0,applied=1,failed=0,currentId=1,appliedId=1,favoritesOnly=false,busy=false,pending:any=null;
function dispatch(){if(busy||!pending)return;const job=pending;pending=null;busy=true;active=job.revision;setTimeout(()=>{busy=false;applied=job.revision;appliedId=job.id;dispatch();},1600);}
const json=(data:any,status=200)=>Response.json(data,{status});
const server=Bun.serve({hostname:'127.0.0.1',port:4173,async fetch(req){
  const u=new URL(req.url),path=u.pathname,method=req.method;
  try{
    if(path==='/api/status')return json({demo:true,version:'1.1.3',count:cards.length,currentId,appliedId,favoritesOnly,desiredRevision:revision,activeRevision:active,appliedRevision:applied,failedRevision:failed,workerBusy:busy,queued:!!pending,managementVisible:false,storageReady:true,used:7200,total:917504,storageError:'',displayError:'',awakeSeconds:300,network:{connected:false,ip:'192.168.4.1',apSsid:'LinkBit-84C16C',ap:true,operation:{result:'idle',active:false},events:[]},clock:{valid:true,networkSynced:true,localTime:'2026-09-13 08:12:30',nextSwitch:'2026-09-14 08:00',lastAutomaticDay:20260913,error:''}});
    if(path==='/api/touch')return json({ok:true});
    if(path==='/api/cards'&&method==='GET'){const offset=Number(u.searchParams.get('offset')||0);return json({items:cards.slice(offset,offset+10),next:offset+10<cards.length?offset+10:-1,count:cards.length});}
    const match=path.match(/^\/api\/cards\/(\d+)(\/appearance)?$/),id=match?Number(match[1]):0,c=cards.find((x:any)=>x.id===id);
    if(match&&method==='GET')return c?json(c):json({error:'卡片不存在'},404);
    if(match&&method==='DELETE'){if(!c||id<=30)return json({error:'仅可删除个人书摘'},409);cards=cards.filter((x:any)=>x.id!==id);return json({ok:true});}
    if(path==='/api/wifi/scan')return json(method==='GET'?{state:'ready',items:[{ssid:'书房 Wi-Fi',authName:'WPA2-Personal',supported:true}]}:{ok:true});
    if(method==='POST'||method==='PUT'){
      const j=await req.json();
      if(path==='/api/preview'){const card=j.id?cards.find((x:any)=>x.id===j.id):validateCard(j);if(!card)throw Error('卡片不存在');const r=await render(card);return new Response(r.bytes,{headers:{'Content-Type':'application/octet-stream','X-Font-Size':r.details.split(' ')[0]}});}
      if(path==='/api/display'){if(!cards.some((x:any)=>x.id===j.id))throw Error('卡片不存在');currentId=j.id;revision++;pending={id:j.id,revision};dispatch();return json({revision},202);}
      if(path==='/api/collection'){if(j.favoritesOnly&&!cards.some((x:any)=>x.favorite))return json({error:'收藏为空，请先收藏至少一条书摘'},409);favoritesOnly=j.favoritesOnly;return json({ok:true});}
      if(path==='/api/wifi')return json({sessionId:'demo-wifi'},202);
      if(match?.[2]){if(!c)throw Error('卡片不存在');Object.assign(c,{favorite:!!j.favorite,illustration:j.illustration});return json({ok:true});}
      if(path==='/api/cards'||match){const value=validateCard(j);if(match&&(!c||id<=30))return json({error:'精选只读'},409);await render(value);const duplicate=cards.find((x:any)=>x.id!==id&&key(x)===key(value));if(duplicate)return json({id:duplicate.id,duplicate:true});if(!match&&cards.length>=130)return json({error:'个人书摘最多 100 条'},507);const record={...value,id:match?id:next++,origin:'personal',verified:false};if(match)Object.assign(c,record);else cards.push(record);return json({id:record.id,duplicate:false},match?200:201);}
    }
    const files:Record<string,string>={'/':'index.html','/style.css':'style.css','/app.js':'app.js','/image.js':'image.js','/data.js':'data.js','/arts.json':'arts.json'};
    if(files[path])return new Response(Bun.file('web/'+files[path]));return json({error:'地址不存在'},404);
  }catch(e:any){return json({error:e.message},422);}
}});
console.log(`Mao card preview: ${server.url} (simulated IO; real firmware typography)`);
