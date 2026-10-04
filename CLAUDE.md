# 毛选卡片 - Linkbit 上可离线翻阅、用手机管理的原文书摘卡
PlatformIO + Arduino ESP32 3.3.8 + GxEPD2 1.6.9 + U8g2 1.8.0 + ArduinoJson 7.4.3 + LittleFS + 原生 HTML/CSS/JavaScript + Bun

<directory>
src/ - 固件：内容、排版、北京时间、持久化、配网、HTTP 与屏幕调度。
web/ - 手机管理界面；接收固件生成的位图，预览不触发上屏。
content/ - 30 条原文及出处、配图来源与可重复生成的输入。
tools/ - TypeScript 素材生成、来源核对和本地验证入口。
tests/ - 直接复用固件逻辑的主机检查与浏览器验证。
</directory>

<config>
platformio.ini - 固定工具链、3MB 应用与 896KB LittleFS 分区、嵌入网页。
huge_app.csv - 固定 4MB Flash 分区边界，应用与个人 LittleFS 数据分离。
build.ps1 - 使用 PATH 或 PLATFORMIO 的构建入口，首次安装固定依赖，检查命令失败并停止；默认不擦除数据。
webserver_guard.py - 复用既有有界 HTTP 解析补丁，只修改构建副本。
board.json - Linkbit V2 的能力元数据；固件引脚以 core.h 为唯一运行时定义。
README.md - 使用、数据边界、构建烧录与实际验证记录。
LICENSE - 原创代码的 MIT 开源授权。
THIRD_PARTY_NOTICES.md - 第三方依赖与内容的来源、许可边界，不将素材纳入原创代码授权。
.gitignore - 排除工具链、缓存、检查产物、日志和本地凭据文件。
</config>

主循环是内容与文件的唯一写入者，后台任务只接收不可变帧并操作屏幕。原文不截断，不将个人输入标为已核验。内置资源位于固件；个人文件和 NVS 使用 maoxuancard 独立命名。挂载失败不格式化非空分区。
