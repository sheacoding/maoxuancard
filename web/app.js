/**
 * [INPUT]: 同源设备 API、image.js 位图解码与 data.js 导入边界
 * [OUTPUT]: 浏览/编辑/收藏/导入导出、设备真实预览、修订及驱动事件配网反馈
 * [POS]: 手机管理控制器；不自行排版，不通过预览触发上屏
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
import {decodeFrame} from './image.js';
import {validateCard,parseImport,exportData} from './data.js';
const $=id=>document.getElementById(id);
let cards=[],arts=[],selected=0,tab='curated',status=null,editing=0,draftFavorite=false,previewSerial=0,pendingRevision=0,polling=false,toastTimer,touchedAt=0,pendingWifi=null;
async function api(path,method='GET',data,raw=false){
  const response=await fetch(path,{method,headers:data?{'Content-Type':'application/json'}:{},body:data?JSON.stringify(data):undefined});
  if(!response.ok){let message=`设备响应 ${response.status}`;try{message=(await response.json()).error||message}catch{}throw Error(message);}
  return raw?response:response.json();
}
function message(el,text,error=false){el.textContent=text;el.classList.toggle('error',error);}
function toast(text){$('toast').textContent=text;$('toast').hidden=false;clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').hidden=true,4500);}
function action(fn){return async e=>{try{await fn(e)}catch(error){toast(error.message)}}}
function paint(canvas,bytes){canvas.getContext('2d').putImageData(new ImageData(decodeFrame(bytes),250,122),0,0);canvas.dataset.ready='true';}
function current(){return cards.find(c=>c.id===selected);}
function count(){
  $('personal-count').textContent=cards.filter(c=>c.origin==='personal').length;
  $('favorite-count').textContent=cards.filter(c=>c.favorite).length;
  $('curated-count').textContent=cards.filter(c=>c.origin==='curated').length;
  const previous=$('topic').value;$('topic').replaceChildren(new Option('全部主题',''));
  [...new Set(cards.map(c=>c.topic).filter(Boolean))].sort().forEach(t=>$('topic').add(new Option(t,t)));$('topic').value=previous;
  if(!$('topic').value) $('topic').value='';
}
async function loadCards(){
  const next=[];let offset=0;
  do{const page=await api(`/api/cards?offset=${offset}`);next.push(...page.items);offset=page.next;}while(offset>=0);
  cards=next;count();renderList();
}
function renderList(){
  const needle=$('search').value.trim().toLocaleLowerCase(),topic=$('topic').value;
  const filtered=cards.filter(c=>(tab==='favorites'?c.favorite:c.origin===tab)&&(!topic||c.topic===topic)&&`${c.text} ${c.source}`.toLocaleLowerCase().includes(needle));
  $('library-meta').textContent=`${filtered.length} 条书摘 · ${tab==='curated'?'已对照原文，点击查看卡面':tab==='personal'?'个人录入，未核验':'反复读，慢慢用'}`;
  $('cards').replaceChildren();
  for(const [i,c] of filtered.entries()){
    const row=document.createElement('button');row.className='quote-row';row.setAttribute('aria-pressed',String(c.id===selected));row.dataset.id=c.id;
    const number=document.createElement('span');number.className='quote-number';number.textContent=String(i+1).padStart(2,'0');
    const content=document.createElement('span'),text=document.createElement('span');text.className='quote-text';text.textContent=c.text;
    const meta=document.createElement('span');meta.className='quote-meta';const source=document.createElement('span');source.textContent=`《${c.source}》${c.favorite?' · 已收藏':''}`;
    const topic=document.createElement('span');topic.className='quote-topic';topic.textContent=c.topic;meta.append(source,topic);content.append(text,meta);row.append(number,content);
    row.onclick=action(()=>select(c.id));$('cards').append(row);
  }
  $('empty').hidden=filtered.length>0;$('empty-title').textContent=needle||topic?'没有匹配的书摘':tab==='favorites'?'把想反复读的句子留下':'记下自己的第一条书摘';
  $('empty-copy').textContent=needle||topic?'试试其他关键词，或选择全部主题。':'从精选库收藏，或点击“新增”记录原文。';
}
function updateSelected(){
  const c=current();for(const id of ['show','favorite','edit']) $(id).disabled=!c;
  if(!c) return;
  $('selected-source').textContent=`《${c.source}》`;$('selected-topic').textContent=c.topic||'个人书摘';
  $('selected-origin').textContent=`${c.volume?`第${['','一','二','三','四'][c.volume]}卷 · `:''}${c.verified?'已对照原文 · 原文节选':'个人录入 · 未核验'}`;
  $('source-link').hidden=!c.url;if(c.url) $('source-link').href=c.url;
  $('favorite').textContent=c.favorite?'★ 已收藏':'☆ 收藏';$('favorite').setAttribute('aria-pressed',String(c.favorite));
  $('edit').textContent=c.origin==='curated'?'复制编辑':'编辑';$('art-name').textContent=arts[c.illustration]?.name||'';
  $('preview-number').textContent=`${String(cards.indexOf(c)+1).padStart(2,'0')} / ${cards.length}`;
  for(const button of $('art-picker').children) button.setAttribute('aria-pressed',String(Number(button.dataset.id)===c.illustration));
}
async function select(id){
  selected=id;updateSelected();renderList();const serial=++previewSerial;
  $('show').disabled=true;message($('preview-state'),'正在生成卡面…');
  try{const r=await api('/api/preview','POST',{id},true),bytes=new Uint8Array(await r.arrayBuffer());
    if(serial!==previewSerial) return;paint($('preview'),bytes);$('preview-empty').hidden=true;$('preview').setAttribute('aria-label',`${current().text}，出自${current().source}`);$('show').disabled=false;
    message($('preview-state'),`250 × 122 · ${r.headers.get('X-Font-Size')||'自动'} px 字号 · 原尺寸位图`);
  }catch(e){if(serial===previewSerial){$('preview').getContext('2d').clearRect(0,0,250,122);$('preview-empty').hidden=false;$('preview-empty').textContent='卡面暂不可用';message($('preview-state'),e.message,true);}throw e;}
}
async function appearance(c,favorite,illustration){
  await api(`/api/cards/${c.id}/appearance`,'PUT',{favorite,illustration});c.favorite=favorite;c.illustration=illustration;count();renderList();updateSelected();
}
function renderArts(){
  $('art-picker').replaceChildren();$('edit-art').replaceChildren();
  arts.forEach((art,id)=>{
    const button=document.createElement('button');button.dataset.id=id;button.setAttribute('aria-label',art.name);button.setAttribute('aria-pressed','false');
    const width=art.width||64,height=art.height||74,stride=Math.ceil(width/8);
    const canvas=document.createElement('canvas');canvas.width=width;canvas.height=height;canvas.setAttribute('aria-hidden','true');
    const ctx=canvas.getContext('2d');ctx.fillStyle='#282b25';for(let y=0;y<height;y++)for(let x=0;x<width;x++)if(art.pixels[y*stride+(x>>3)]&(128>>(x%8)))ctx.fillRect(x,y,1,1);
    const label=document.createElement('span');label.textContent=art.name;button.append(canvas,label);
    button.onclick=action(async()=>{const c=current();if(!c)return;await appearance(c,c.favorite,id);await select(c.id);toast('配图已保存；点击上屏后更新屏幕。');});$('art-picker').append(button);$('edit-art').add(new Option(art.name,String(id)));
  });
}
function openEditor(c){
  editing=c?.origin==='personal'?c.id:0;draftFavorite=c?.favorite||false;
  $('editor-title').textContent=editing?'编辑个人书摘':c?'复制为个人书摘':'记下一条书摘';
  for(const [id,key] of [['text','text'],['source','source'],['url','url'],['edit-topic','topic']]) $(id).value=c?.[key]||'';
  $('volume').value=c?.volume||0;$('edit-art').value=c?.illustration||0;$('delete').hidden=!editing;
  delete $('draft-preview').dataset.ready;message($('edit-error'),'保存前会检查缺字与排版，不截断原文。');$('editor').showModal();
}
function draft(){return validateCard({text:$('text').value.trim(),source:$('source').value.trim(),url:$('url').value.trim(),topic:$('edit-topic').value.trim(),volume:Number($('volume').value),illustration:Number($('edit-art').value),favorite:draftFavorite});}
async function draftPreview(){const c=draft(),r=await api('/api/preview','POST',c,true);paint($('draft-preview'),new Uint8Array(await r.arrayBuffer()));message($('edit-error'),'可完整上屏；预览没有刷新设备。');}
async function poll(){
  if(polling||document.hidden)return;polling=true;
  try{
    status=await api('/api/status');const n=status.network;
    $('connection').textContent=status.demo?'本机预览 · 模拟设备':n.connected?'设备已连接':'设备热点已连接';
    if(pendingRevision){
      if(status.failedRevision===pendingRevision){message($('display-state'),`刷新失败：${status.displayError}。可重新点击上屏。`,true);pendingRevision=0;}
      else if(status.appliedRevision===pendingRevision){message($('display-state'),'已完成，墨水屏现在显示这条书摘。');pendingRevision=0;}
      else if(status.desiredRevision>pendingRevision){message($('display-state'),'已有更新的上屏目标，正在等待最新画面。');pendingRevision=0;}
      else message($('display-state'),status.activeRevision===pendingRevision&&status.workerBusy?'屏幕刷新中，约需 15–17 秒…':'已提交，等待当前刷新完成…');
    }else if(status.displayError) message($('display-state'),status.displayError,true);
    document.querySelector(`input[name=collection][value=${status.favoritesOnly?'favorites':'all'}]`).checked=true;
    $('collection-note').textContent=status.favoritesOnly&&!cards.some(c=>c.favorite)?'收藏为空，设备暂时没有可翻阅的书摘。':'切换集合不会刷新当前卡面。';
    $('storage').textContent=status.storageError||`${cards.filter(c=>c.origin==='personal').length} / 100 条个人书摘 · ${Math.round(status.used/1024)} KB / ${Math.round(status.total/1024)} KB`;
    $('network-info').textContent=n.connected?`${n.ssid} · ${n.ip}`:`连接热点 ${n.apSsid} 后，访问 ${n.ip||'192.168.4.1'}`;
    $('device-detail').textContent=`版本 ${status.version} · 距休眠 ${status.awakeSeconds} 秒`;
    const clock=status.clock||{};
    $('clock-info').textContent=clock.valid?`北京时间 ${clock.localTime}${clock.networkSynced?' · 本次启动已联网校准':' · 使用 RTC 计时'}`:'尚未取得有效时间，设备每六小时联网重试。';
    $('clock-next').textContent=clock.nextSwitch?`下次自动换句：${clock.nextSwitch}${clock.error?' · '+clock.error:''}`:'联网校时后，每天 08:00 自动切换下一条。';
    const op=n.operation;
    $('wifi-diagnostics').textContent=(n.events||[]).map(e=>`${(e.ms/1000).toFixed(1)} 秒｜${e.event}：${e.value}`).join('\n')||'本次开机尚无连接记录。';
    if(op?.active) message($('wifi-state'),`正在连接 ${op.targetSsid}${op.phase==='sta'?' · 已关闭设备热点重试':''} · ${op.seconds} 秒`);
    else if(op?.result==='saved') {if(pendingWifi?.id===op.id&&pendingWifi.ssid===$('ssid').value&&pendingWifi.password===$('wifi-password').value)$('wifi-password').value='';pendingWifi=null;message($('wifi-state'),`连接成功，已保存 · ${op.savedIp}`);}
    else if(op?.result==='failed') message($('wifi-state'),op.error||'连接失败，原配置保留。',true);
  }catch{ $('connection').textContent='设备未连接';if(pendingRevision) message($('display-state'),'连接中断，尚不能确认刷新结果；请唤醒设备后重试。',true); }
  finally{polling=false;}
}
async function importFile(file){
  if(!file)return;if(file.size>1024*1024)throw Error('导入文件不能超过 1MB。');
  const data=parseImport(await file.text()),mapping=new Map(),report={added:0,duplicate:0,appearance:0,failed:[]};
  $('import').disabled=true;message($('import-result'),'格式检查通过，逐条追加中…');
  try{
    for(const c of data.personal){try{const r=await api('/api/cards','POST',validateCard(c));mapping.set(c.id,r.id);report[r.duplicate?'duplicate':'added']++;}catch(e){report.failed.push(`书摘 ${c.id}：${e.message}`);}message($('import-result'),`新增 ${report.added} · 去重 ${report.duplicate} · 失败 ${report.failed.length}`);}
    for(const a of data.appearance){const id=a.id<=30?a.id:mapping.get(a.id);if(!id)continue;try{await api(`/api/cards/${id}/appearance`,'PUT',{favorite:a.favorite,illustration:a.illustration});report.appearance++;}catch(e){report.failed.push(`配图/收藏 ${a.id}：${e.message}`);}}
    await loadCards();
    try{await api('/api/collection','PUT',{favoritesOnly:data.favoritesOnly});}catch(e){report.failed.push(`翻阅集合：${e.message}`);}
    message($('import-result'),`导入结束：新增 ${report.added}，去重 ${report.duplicate}，恢复配图/收藏 ${report.appearance}，失败 ${report.failed.length}。${report.failed.length?'\n'+report.failed.join('\n'):''}`,report.failed.length>0);
    if(current())await select(selected);await poll();
  }finally{$('import').disabled=false;}
}
document.querySelectorAll('[data-tab]').forEach(button=>button.onclick=()=>{
  tab=button.dataset.tab;document.querySelectorAll('[data-tab]').forEach(b=>b===button?b.setAttribute('aria-current','page'):b.removeAttribute('aria-current'));
  $('library-view').hidden=tab==='settings';$('settings-view').hidden=tab!=='settings';if(tab!=='settings')renderList();
});
$('search').oninput=renderList;$('topic').onchange=renderList;
$('favorite').onclick=action(async()=>{const c=current();await appearance(c,!c.favorite,c.illustration);});
$('edit').onclick=()=>openEditor(current());$('add').onclick=()=>openEditor();$('close-editor').onclick=()=>$('editor').close();
$('draft-button').onclick=async()=>{try{await draftPreview()}catch(e){message($('edit-error'),e.message,true)}};
$('edit-form').onsubmit=async e=>{e.preventDefault();const button=e.submitter;button.disabled=true;try{const r=await api(editing?`/api/cards/${editing}`:'/api/cards',editing?'PUT':'POST',draft());$('editor').close();await loadCards();await select(r.id);toast(r.duplicate?'已有相同原文与篇名，已定位到现有书摘。':'书摘已保存；点击上屏后更新屏幕。');}catch(error){message($('edit-error'),error.message,true)}finally{button.disabled=false}};
$('delete').onclick=action(async()=>{if(!editing||!confirm('删除这条个人书摘？设备当前卡面会保留。'))return;await api(`/api/cards/${editing}`,'DELETE');$('editor').close();await loadCards();if(cards.length)await select(cards[0].id);toast('书摘已删除。');});
$('show').onclick=action(async()=>{const id=selected;const r=await api('/api/display','POST',{id});pendingRevision=r.revision;message($('display-state'),'已提交，等待刷新…');await poll();});
document.querySelectorAll('input[name=collection]').forEach(input=>input.onchange=action(async()=>{try{await api('/api/collection','PUT',{favoritesOnly:input.value==='favorites'});}finally{await poll();}}));
$('export').onclick=action(async()=>{await loadCards();const blob=new Blob([JSON.stringify(exportData(cards,status?.favoritesOnly),null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=`毛选书摘-${new Date().toISOString().slice(0,10)}.json`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);toast('已导出书摘、收藏和配图。');});
$('import').onclick=()=>$('import-file').click();$('import-file').onchange=async()=>{try{await importFile($('import-file').files[0]);}catch(e){message($('import-result'),e.message,true)}finally{$('import-file').value=''}};
$('scan').onclick=action(async()=>{const button=$('scan');button.disabled=true;try{await api('/api/wifi/scan','POST');message($('wifi-state'),'正在扫描…');for(let i=0;i<20;i++){await new Promise(r=>setTimeout(r,1000));const scan=await api('/api/wifi/scan');if(scan.state==='scanning')continue;if(scan.state!=='ready')throw Error('扫描失败，请重试。');$('networks').replaceChildren(new Option('请选择网络',''));for(const net of scan.items){const option=new Option(`${net.ssid} · ${net.authName}`,net.ssid);option.disabled=!net.supported;$('networks').add(option);}message($('wifi-state'),`找到 ${scan.items.length} 个网络。`);return;}throw Error('扫描超时。');}finally{button.disabled=false}});
$('networks').onchange=()=>{$('ssid').value=$('networks').value};
$('wifi-form').onsubmit=action(async e=>{e.preventDefault();const ssid=$('ssid').value,password=$('wifi-password').value,r=await api('/api/wifi','PUT',{ssid,password});pendingWifi={id:r.sessionId,ssid,password};message($('wifi-state'),'连接请求已提交，失败时保留原配置和输入。');});
for(const event of ['pointerdown','keydown'])document.addEventListener(event,()=>{if(Date.now()-touchedAt>30000){touchedAt=Date.now();api('/api/touch','POST').catch(()=>{});}},{passive:true});
async function start(){try{arts=await api('/arts.json');renderArts();await loadCards();await poll();await select(cards.some(c=>c.id===status?.currentId)?status.currentId:cards[0].id);}catch(e){message($('preview-state'),`无法加载：${e.message}`,true);$('connection').textContent='请连接设备并刷新页面';}setInterval(poll,1800);}
start();
