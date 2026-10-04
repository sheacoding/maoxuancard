/**
 * [INPUT]: 支持 open/write/flush/size/close/rename 的文件系统与完整序列化记录
 * [OUTPUT]: 临时文件完整提交，失败不删除或覆盖已提交文件
 * [POS]: LittleFS 与故障注入主机检查共用原子写入顺序
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#pragma once
#include <cstdint>
#include <cstddef>
namespace linkbit {
template<class Filesystem> bool atomicWrite(Filesystem& fs,const char* temp,const char* target,const uint8_t* data,size_t size) {
    auto file=fs.open(temp,"w");
    bool ok=file&&file.write(data,size)==size;
    file.flush();ok=ok&&file.size()==size;file.close();
    return ok&&fs.rename(temp,target);
}
}
