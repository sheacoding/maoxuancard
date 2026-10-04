/**
 * [INPUT]: 用户 JSON 与卡片快照，不依赖 DOM 或网络
 * [OUTPUT]: 有界导入校验、内容去重键和不含凭据的导出结构
 * [POS]: 浏览器管理与 Bun 检查共享的数据边界
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
const bytes=s=>new TextEncoder().encode(s).length;
export const key=c=>`${c.text}\u0000${c.source}`;
export function validateCard(c){
  if(!c||typeof c!=='object'||Array.isArray(c)) throw Error('书摘须为对象。');
  const out={};
  for(const [name,limit,required] of [['text',512,true],['source',192,true],['topic',48,false],['url',256,false]]){
    const s=c[name]??'';
    if(typeof s!=='string'||bytes(s)>limit||/[\u0000-\u0009\u000b-\u001f\u007f]/.test(s)||/[\ud800-\udfff]/u.test(s)||((name==='source'||name==='topic')&&s.includes('\n'))||(required&&!s.trim())) throw Error(`${name} 字段类型、长度或字符无效。`);
    out[name]=s;
  }
  if(out.url){let u;try{u=new URL(out.url)}catch{throw Error('出处链接无效。')}if(!['http:','https:'].includes(u.protocol)||/[\s<>"\\]/.test(out.url)) throw Error('出处链接须为 HTTP 或 HTTPS。');}
  for(const [name,max] of [['volume',4],['illustration',7]]){const n=c[name]??0;if(!Number.isInteger(n)||n<0||n>max) throw Error(`${name} 无效。`);out[name]=n;}
  if(c.favorite!==undefined&&typeof c.favorite!=='boolean') throw Error('收藏字段须为布尔值。');out.favorite=c.favorite??false;
  return out;
}
export function exportData(cards,favoritesOnly){return {format:'maoxuancard',version:1,personal:cards.filter(c=>c.origin==='personal').map(c=>({id:c.id,...validateCard(c)})),appearance:cards.map(c=>({id:c.id,favorite:c.favorite,illustration:c.illustration})),favoritesOnly:!!favoritesOnly};}
export function parseImport(text){
  if(bytes(text)>1024*1024) throw Error('导入文件不能超过 1MB。');
  let j;try{j=JSON.parse(text)}catch{throw Error('文件不是有效 JSON。')}
  if(!j||j.format!=='maoxuancard'||j.version!==1||!Array.isArray(j.personal)||j.personal.length>100||!Array.isArray(j.appearance)||j.appearance.length>130||typeof j.favoritesOnly!=='boolean') throw Error('需要毛选卡片 v1 导出格式，个人书摘最多 100 条。');
  const ids=new Set();const personal=j.personal.map(c=>{if(!Number.isSafeInteger(c.id)||c.id<1001||c.id>=4294967295||ids.has(c.id)) throw Error('个人书摘编号无效或重复。');ids.add(c.id);return {id:c.id,...validateCard(c)};});
  const appearances=new Set();const appearance=j.appearance.map(a=>{if(!a||!Number.isInteger(a.id)||(!(a.id>=1&&a.id<=30)&&!ids.has(a.id))||appearances.has(a.id)||typeof a.favorite!=='boolean'||!Number.isInteger(a.illustration)||a.illustration<0||a.illustration>7) throw Error('收藏或配图引用无效。');appearances.add(a.id);return {id:a.id,favorite:a.favorite,illustration:a.illustration};});
  return {personal,appearance,favoritesOnly:j.favoritesOnly};
}
