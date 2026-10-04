/**
 * [INPUT]: app 模块、单槽 FreeRTOS 队列与 ESP32-C3 深睡唤醒
 * [OUTPUT]: 循环翻阅、北京时间每日推进、最新目标合并与 GPIO/RTC 双源休眠
 * [POS]: 内容和合成的唯一写入者；后台只消费已冻结位图
 * [PROTOCOL]: 变更时更新此头部，然后检查 CLAUDE.md
 */
#include "app.h"
#include <esp_sleep.h>
#include <esp_random.h>
#include <esp_rom_crc.h>
#include <driver/gpio.h>
namespace linkbit {
State state;
struct Job {uint32_t revision=0,id=0;bool management=false;uint8_t pixels[FrameBytes]{};};
struct Result {bool ok;char error[160];};
static QueueHandle_t jobs,results;
static Job pending;
static uint8_t applied[FrameBytes];static bool appliedKnown=false,workerReady=false;
static Button buttons[]={{KeyA},{KeyB},{KeyC}};static HoldPress hold;
RTC_DATA_ATTR static uint32_t retainedMagic=0,retainedHash=0;
static bool retainedValid=false;
void touch() {state.lastActivity=millis();}
void markDirty() {state.request(millis());state.displayError[0]=0;}
bool requestCard(uint32_t id) {
    Card c;Layout l;char error[160];static uint8_t validation[FrameBytes];
    if(!readCard(id,c)||!composeCard(c,validation,l,error,sizeof(error))) {copyText(state.displayError,"卡片无法读取或排版，请先预览检查");return false;}
    Settings next=state.settings;next.currentId=id;
    if(!saveSettings(next)) return false;
    state.settings=next;state.desiredId=id;state.managementVisible=false;touch();markDirty();return true;
}
bool setCollection(bool favorites) {
    if(favorites&&!navigate(state.entries,state.count,state.desiredId,1,true)) {copyText(state.storageError,"收藏为空，请先收藏至少一条书摘");return false;}
    Settings next=state.settings;next.favoritesOnly=favorites;
    if(!saveSettings(next)) return false;
    state.settings=next;touch();return true;
}
bool automaticAdvance(uint32_t day) {
    if(day<=state.settings.lastAutoDay) return true;
    uint32_t id=navigate(state.entries,state.count,state.desiredId,1,state.settings.favoritesOnly);
    Settings next=state.settings;next.lastAutoDay=day;
    if(id) next.currentId=id;
    if(!saveSettings(next)) {copyText(state.clockError,"自动换句保存失败，将在下次唤醒重试");return false;}
    state.settings=next;
    if(!id) {copyText(state.clockError,"收藏为空，今日未自动换句");return true;}
    state.desiredId=id;state.managementVisible=false;state.clockError[0]=0;touch();markDirty();
    Serial.printf("[clock] daily card day=%lu id=%lu\n",static_cast<unsigned long>(day),static_cast<unsigned long>(id));return true;
}
void showManagement() {touch();if(!state.managementVisible) {state.managementVisible=true;markDirty();}}
void completeLogin() {
    if(!state.settings.loginCompleted) {Settings next=state.settings;next.loginCompleted=true;if(saveSettings(next)) state.settings=next;}
    // 登录和预览不触发物理刷新；用按键返回或显式上屏。
    touch();
}
static void worker(void*) {
    static Job job;Result r{};
    for(;;) {
        if(xQueueReceive(jobs,&job,portMAX_DELAY)!=pdTRUE) continue;
        r={};r.ok=displayFrame(job.pixels,r.error,sizeof(r.error));xQueueSend(results,&r,portMAX_DELAY);
    }
}
static void completed() {
    Result r{};if(xQueueReceive(results,&r,0)!=pdTRUE) return;
    state.finish(pending.revision,r.ok);
    if(r.ok) {
        memcpy(applied,pending.pixels,FrameBytes);appliedKnown=true;
        state.displayError[0]=0;state.appliedId=pending.management?0:pending.id;
        retainedMagic=pending.management?0:0x4D414F31;retainedHash=esp_rom_crc32_le(0,applied,FrameBytes);
        Serial.printf("[display] completed revision=%lu id=%lu\n",static_cast<unsigned long>(pending.revision),static_cast<unsigned long>(state.appliedId));
    } else {retainedMagic=0;appliedKnown=false;state.failedRevision=pending.revision;copyText(state.displayError,r.error);Serial.printf("[display] %s\n",r.error);}
}
static bool released() {return digitalRead(KeyA)&&digitalRead(KeyB)&&digitalRead(KeyC);}
static void sleepNow() {
    gpio_pullup_en(GPIO_NUM_5);gpio_pulldown_dis(GPIO_NUM_5);
    if(esp_deep_sleep_enable_gpio_wakeup(1ULL<<KeyB,ESP_GPIO_WAKEUP_GPIO_LOW)!=ESP_OK) {touch();copyText(state.displayError,"B 键唤醒配置失败，设备保持唤醒");return;}
    uint64_t timer=clockWakeDelayUs();
    if(esp_sleep_enable_timer_wakeup(timer)!=ESP_OK) {touch();copyText(state.displayError,"定时唤醒配置失败，设备保持唤醒");return;}
    state.shuttingDown=true;webStop();clockStop();networkStop();
    Serial.printf("[power] sleep; wake on B or timer in %llu seconds\n",timer/1000000ULL);Serial.flush();esp_deep_sleep_start();
}
static void schedule() {
    if(state.workerBusy) return;
    if(state.ready(millis())) {
        pending.revision=state.desiredRevision;pending.id=state.desiredId;pending.management=state.managementVisible;
        if(state.managementVisible) composeManagement(networkAddress().c_str(),state.settings.admin,pending.pixels);
        else {
            Card c;Layout layout;char error[160]{};
            if(!readCard(state.desiredId,c)||!composeCard(c,pending.pixels,layout,error,sizeof(error))) {
                state.dirty=false;state.failedRevision=state.desiredRevision;copyText(state.displayError,error[0]?error:"卡片读取失败");return;
            }
        }
        bool same=appliedKnown&&!memcmp(applied,pending.pixels,FrameBytes);
        if(retainedValid) {same=!pending.management&&retainedHash==esp_rom_crc32_le(0,pending.pixels,FrameBytes);retainedValid=false;}
        if(same) {
            memcpy(applied,pending.pixels,FrameBytes);appliedKnown=true;state.appliedRevision=pending.revision;state.appliedId=pending.management?0:pending.id;state.dirty=false;
        } else if(xQueueSend(jobs,&pending,0)==pdTRUE) {state.begin();retainedMagic=0;}
        return;
    }
    if(!state.dirty&&!networkTesting()&&released()&&elapsed(millis(),state.lastActivity,AwakeMs)) {
        if(state.managementVisible) {state.managementVisible=false;markDirty();return;}
        sleepNow();
    }
}
}
void setup() {
    using namespace linkbit;Serial.begin(115200);Serial.println("\nLinkBit 毛选卡片 1.1.3");
    for(auto& b:buttons) {pinMode(b.pin,INPUT_PULLUP);b.raw=b.stable=digitalRead(b.pin);b.armed=false;b.changed=millis();}
    storageBegin();state.desiredId=state.settings.currentId;touch();
    auto cause=esp_sleep_get_wakeup_cause();retainedValid=(cause==ESP_SLEEP_WAKEUP_GPIO||cause==ESP_SLEEP_WAKEUP_TIMER)&&retainedMagic==0x4D414F31;
    state.managementVisible=!state.settings.loginCompleted&&!retainedValid;
    networkBegin();clockBegin();jobs=xQueueCreate(1,sizeof(Job));results=xQueueCreate(1,sizeof(Result));
    workerReady=jobs&&results&&xTaskCreate(worker,"paper",6144,nullptr,1,nullptr)==pdPASS;
    if(!workerReady) copyText(state.displayError,"屏幕后台任务创建失败");
    webBegin();markDirty();
}
void loop() {
    using namespace linkbit;if(workerReady) completed();webTick();networkTick();clockTick();uint32_t now=millis();
    for(int i=0;i<3;++i) {
        bool edge=buttons[i].update(digitalRead(buttons[i].pin),now);Press action=Press::None;
        if(i==1) {if(edge) touch();action=hold.update(buttons[i],edge,now);if(action==Press::Long) {showManagement();continue;}}
        if((i!=1&&edge)||action==Press::Short) {
            touch();
            if(state.managementVisible) {state.managementVisible=false;markDirty();continue;}
            uint32_t id=i==1?randomCard(state.entries,state.count,state.desiredId,state.settings.favoritesOnly,esp_random()):navigate(state.entries,state.count,state.desiredId,i==0?-1:1,state.settings.favoritesOnly);
            if(id) requestCard(id);else copyText(state.displayError,"收藏为空，请在手机添加收藏或切换为全部");
        }
    }
    if(workerReady) schedule();delay(2);
}
