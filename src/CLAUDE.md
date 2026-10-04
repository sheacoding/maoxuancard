# src/
> L2 | 父级: ../CLAUDE.md

app.h: 汇总运行态、设置与模块边界；主循环是内容和持久化的单写者。
assets.cpp: 生成的八款 64×74 单色配图，只读驻留固件；肖像使用 Floyd–Steinberg 误差扩散。
card_json.h: 声明卡片 JSON 信任边界。
card_json.cpp: 在 HTTP 与持久化之间执行严格解析和稳定序列化。
clock.cpp: 以北京时间非阻塞执行 SNTP 校时、每日触发、RTC 唤醒计划和状态输出。
clock_policy.h: 定义 07:58 预唤醒、08:00 每日判定与无有效时间六小时重试的纯策略。
commit.h: 提供可注入文件系统的临时文件提交，测试覆盖短写、失败与中断。
content.cpp: 生成的 30 条只读精选及其逐条来源。
core.h: 定义屏幕尺寸、引脚、卡片、按键、布局与刷新状态协议。
core.cpp: 实现不依赖 Arduino 的校验、导航、按键与刷新合并规则。
display.cpp: 独占 GDEY0213Z98 和 SPI，将不可变双平面帧完整刷新并核实 BUSY。
main.cpp: 编排按键、内容、网络、每日推进、刷新任务与 GPIO/RTC 双源深度休眠。
network_policy.h: 用可测试的状态机限定配网尝试、纯 STA 回退、总截止时间及旧驱动事件归属。
network.cpp: AP＋STA 失败后以纯 STA 重试，隔离跨轮次事件并清理候选凭据。
render.cpp: 使用完整中文字库生成正文朱红、篇名与配图墨黑的 250×122 卡面，配图按正文空间取 64×74 或 40×46。
storage.cpp: 在独立 LittleFS 目录和 NVS 命名空间中原子保存个人数据。
web.cpp: 提供 Digest 认证、本地静态页、卡片管理、预览、上屏和状态接口。

[PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
