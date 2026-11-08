#pragma once

// ================= WiFi 配置 =================
// 填成你自己的 2.4GHz WiFi。留空则跳过联网，用固件编译时间当离线时钟。
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// ================= NTP 对时 =================
#define NTP_SERVER1 "ntp.aliyun.com"
#define NTP_SERVER2 "cn.pool.ntp.org"
#define NTP_SERVER3 "time.windows.com"

// 时区：中国标准时间 UTC+8
#define TIMEZONE "CST-8"
