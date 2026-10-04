# 第三方来源与许可边界

根目录 `LICENSE` 授权项目原创代码，不改变第三方库、字体、原文节选或肖像的权利状态。生成文件中的第三方内容也保留这一边界。

## 固件与主机检查依赖

固定版本见 `platformio.ini`。依赖由 PlatformIO 下载，不随源码仓库提交。再分发包含这些依赖的固件时，应保留各依赖自身的许可和版权声明；准确条款以对应版本发布包内的文件为准。

| 依赖 | 上游 |
| --- | --- |
| pioarduino / Arduino-ESP32 | https://github.com/pioarduino/platform-espressif32 · https://github.com/espressif/arduino-esp32 |
| GxEPD2 | https://github.com/ZinggJM/GxEPD2 |
| Adafruit GFX Library | https://github.com/adafruit/Adafruit-GFX-Library |
| Adafruit BusIO | https://github.com/adafruit/Adafruit_BusIO |
| U8g2 for Adafruit GFX / U8g2 字库 | https://github.com/olikraus/U8g2_for_Adafruit_GFX · https://github.com/olikraus/u8g2 |
| ArduinoJson | https://github.com/bblanchon/ArduinoJson |

## 原文与肖像

- `content/quotes.json` 保存原文节选和逐条出处；`src/content.cpp` 是生成副本。来源记录不等于 MIT 授权，项目不主张原文的版权。
- `content/portrait-1927.jpg`：来源页为 https://commons.wikimedia.org/wiki/File:Mao_Zedong_in_1927.jpg；项目现有记录注明作者不详、新华社来源及中国公版说明。
- `content/portrait-1937.jpg`：来源页为 https://commons.wikimedia.org/wiki/File:Mao_Zedong_1937.jpg；项目现有记录注明 Edgar Snow 摄、页面标注 CC0。
- 肖像的像素转换保存在 `src/assets.cpp` 与 `web/arts.json`，沿用原始素材的权利边界；六幅主题简笔画由项目代码生成。

完整来源及原有核对日期见 `content/sources.json`。上述肖像说明转录自项目已有来源记录，不表示本次发布重新核验了来源页。

[PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
