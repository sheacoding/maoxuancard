/**
 * [INPUT]: core 卡片与 ArduinoJson（设备与主机共用）
 * [OUTPUT]: 严格字段读取和统一内容来源序列化
 * [POS]: HTTP 与存储的信任边界，验证后复制，拒绝 NUL 和超长字段
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#pragma once
#include "core.h"
#include <ArduinoJson.h>
namespace linkbit {
bool parseCard(JsonObjectConst json,Card& card,char* error,size_t size);
void cardJson(const Card& card,JsonObject json);
}
