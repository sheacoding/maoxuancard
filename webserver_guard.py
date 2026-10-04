"""
[INPUT]: 固定 Arduino-ESP32 3.3.8 的 WebServer/Parsing.cpp 与 PlatformIO 构建中间件
[OUTPUT]: 仅在构建目录生成 9KB/15 秒有界解析器，容纳双平面图片的 multipart 开销
[POS]: 修复原生 multipart 断开后的循环；不编辑框架安装目录，升级时锚点变化立即失败
[PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
"""
from pathlib import Path

HELPERS = r'''
// --- LinkBit: 单客户端解析预算，保护表单中断与畸形请求 ---
static uint32_t lbRequestStarted = 0;
static size_t lbLineBytes = 0, lbFileBytes = 0;
static bool lbExpired() { return uint32_t(millis() - lbRequestStarted) >= 15000; }
static String lbReadUntil(NetworkClient& client, char delimiter) {
  String value;
  while (!lbExpired()) {
    if (client.available()) {
      int byte = client.read();
      if (byte < 0) continue;
      if (++lbLineBytes > 4096 || value.length() > 1024) break;
      if (byte == delimiter) return value;
      value += char(byte);
    } else if (!client.connected()) break;
    else delay(1);
  }
  client.stop();
  return String();
}
'''

def patch_source(source):
    start = 'bool WebServer::_parseRequest(NetworkClient &client) {'
    raw = '    if (!isForm && _currentHandler && _currentHandler->canRaw(*this, _currentUri)) {'
    upload = 'int WebServer::_uploadReadByte(NetworkClient &client) {'
    for anchor in (start, raw, upload):
        if source.count(anchor) != 1:
            raise RuntimeError('WebServer 版本与保护补丁不匹配：' + anchor)
    if source.count('while (1) {') != 4:
        raise RuntimeError('WebServer 表单循环已变化，需要重新审核保护补丁')
    source = source.replace(start, HELPERS + '\n' + start + '\n  lbRequestStarted = millis(); lbLineBytes = lbFileBytes = 0;')
    source = source.replace(raw, '''    if (_clientContentLength < 0 || _clientContentLength > 9216 || !client.connected() || lbExpired()) {
      send(413, "application/json", "{\\"error\\":\\"Request too large or incomplete\\"}");
      client.stop();
      return false;
    }
''' + raw)
    source = source.replace('while (1) {', 'while (1) {\n      if (!client.connected() || lbExpired()) return false;')
    source = source.replace(upload, upload + '\n  if (lbExpired() || ++lbFileBytes > 9216) { client.stop(); return -1; }')
    source = source.replace("client.readStringUntil('\\r')", "lbReadUntil(client, '\\r')")
    source = source.replace("client.readStringUntil('\\n')", "lbReadUntil(client, '\\n')")
    return source

def guarded_parser(env, node):
    source = Path(node.srcnode().get_abspath())
    if source.name != 'Parsing.cpp' or source.parent.parent.name != 'WebServer':
        return node
    destination = Path(env.subst('$BUILD_DIR')) / 'bounded-webserver' / 'Parsing.cpp'
    patched = patch_source(source.read_text(encoding='utf-8'))
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding='utf-8') != patched:
        destination.write_text(patched, encoding='utf-8')
    return env.File(str(destination))

Import('env')
env.AddBuildMiddleware(guarded_parser, '*Parsing.cpp')
