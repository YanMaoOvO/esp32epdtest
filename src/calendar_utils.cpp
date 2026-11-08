#include "calendar_utils.h"

#include <stdio.h>
#include <string.h>

#include "solar_terms.h"

// ================= 农历数据（1900-2100） =================
// 低 4 位：闰月月份（0 表示无闰月）
// bit 16：闰月大小（1 = 30 天，0 = 29 天）
// bit 15..4：正月到十二月的大小（1 = 30 天，0 = 29 天）
static const uint32_t LUNAR_INFO[201] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,  // 1900-1909
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,  // 1910-1919
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,  // 1920-1929
    0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,  // 1930-1939
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,  // 1940-1949
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,  // 1950-1959
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,  // 1960-1969
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,  // 1970-1979
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,  // 1980-1989
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x05ac0, 0x0ab60, 0x096d5, 0x092e0,  // 1990-1999
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,  // 2000-2009
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,  // 2010-2019
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,  // 2020-2029
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,  // 2030-2039
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,  // 2040-2049
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0,  // 2050-2059
    0x0a2e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4,  // 2060-2069
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0,  // 2070-2079
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160,  // 2080-2089
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252,  // 2090-2099
    0x0d520                                                                                   // 2100
};

static const char *const LUNAR_MONTHS[13] = {
    "", "正月", "二月", "三月", "四月", "五月", "六月",
    "七月", "八月", "九月", "十月", "冬月", "腊月"};

static const char *const LUNAR_DAYS[31] = {
    "", "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"};

static const char *const WEEKDAYS[7] = {
    "星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};

static const char *const STEMS[10] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
static const char *const BRANCHES[12] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
static const char *const ZODIAC[12] = {"鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"};

static int leapMonth(int year) {
  return static_cast<int>(LUNAR_INFO[year - 1900] & 0x0f);
}

static int leapMonthDays(int year) {
  if (leapMonth(year) == 0) {
    return 0;
  }
  return (LUNAR_INFO[year - 1900] & 0x10000) ? 30 : 29;
}

static int monthDays(int year, int month) {
  return (LUNAR_INFO[year - 1900] & (0x10000 >> month)) ? 30 : 29;
}

static int yearDays(int year) {
  int sum = 348;  // 12 * 29
  for (uint32_t mask = 0x8000; mask > 0x8; mask >>= 1) {
    if (LUNAR_INFO[year - 1900] & mask) {
      sum++;
    }
  }
  return sum + leapMonthDays(year);
}

int64_t daysFromCivil(int year, int month, int day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = static_cast<unsigned>((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

LunarDate solarToLunar(int year, int month, int day) {
  LunarDate result = {0, 0, 0, false};
  if (year < 1900 || year > 2100) {
    return result;
  }

  int offset = static_cast<int>(daysFromCivil(year, month, day) - daysFromCivil(1900, 1, 31));
  int lunarYear = 1900;
  int temp = 0;

  int i = lunarYear;
  while (i < 2101 && offset > 0) {
    temp = yearDays(i);
    offset -= temp;
    i++;
  }
  if (offset < 0) {
    offset += temp;
    i--;
  }
  lunarYear = i;

  const int leap = leapMonth(lunarYear);
  bool isLeap = false;
  int lunarMonth = 1;

  for (i = 1; i <= 12 && offset > 0; i++) {
    if (leap > 0 && i == leap + 1 && !isLeap) {
      --i;
      isLeap = true;
      temp = leapMonthDays(lunarYear);
    } else {
      temp = monthDays(lunarYear, i);
    }

    if (isLeap && i == leap + 1) {
      isLeap = false;
    }
    offset -= temp;
  }

  if (offset == 0 && leap > 0 && i == leap + 1) {
    if (isLeap) {
      isLeap = false;
    } else {
      isLeap = true;
      --i;
    }
  }
  if (offset < 0) {
    offset += temp;
    --i;
  }

  lunarMonth = i;
  result.year = lunarYear;
  result.month = lunarMonth;
  result.day = offset + 1;
  result.leap = isLeap;
  return result;
}

const char *lunarMonthName(int month) {
  if (month < 1 || month > 12) {
    return "";
  }
  return LUNAR_MONTHS[month];
}

const char *lunarDayName(int day) {
  if (day < 1 || day > 30) {
    return "";
  }
  return LUNAR_DAYS[day];
}

const char *weekdayName(int weekday) {
  if (weekday < 0 || weekday > 6) {
    return "";
  }
  return WEEKDAYS[weekday];
}

const char *zodiacName(int lunarYear) {
  int offset = (lunarYear - 4) % 12;
  if (offset < 0) {
    offset += 12;
  }
  return ZODIAC[offset];
}

void formatGanzhiYear(int lunarYear, char *out, size_t size) {
  int offset = (lunarYear - 4) % 60;
  if (offset < 0) {
    offset += 60;
  }
  snprintf(out, size, "%s%s", STEMS[offset % 10], BRANCHES[offset % 12]);
}

void formatLunarDate(const LunarDate &date, char *out, size_t size) {
  snprintf(out, size, "农历%s%s%s", date.leap ? "闰" : "",
           lunarMonthName(date.month), lunarDayName(date.day));
}

void formatSolarTermInfo(const struct tm &now, char *out, size_t size) {
  const int year = now.tm_year + 1900;
  if (year < SOLAR_TERM_FIRST_YEAR || year > SOLAR_TERM_LAST_YEAR) {
    snprintf(out, size, "节气 --");
    return;
  }

  const int row = year - SOLAR_TERM_FIRST_YEAR;
  const int base = now.tm_mon * 2;

  // 今天正好是节气
  for (int k = 0; k < 2; k++) {
    const int index = base + k;
    if (SOLAR_TERM_DAYS[row][index] == now.tm_mday) {
      snprintf(out, size, "今日 %s", SOLAR_TERM_NAMES[index]);
      return;
    }
  }

  const int64_t today = daysFromCivil(year, now.tm_mon + 1, now.tm_mday);
  for (int index = base; index < 24; index++) {
    const int termMonth = index / 2 + 1;
    const int termDay = SOLAR_TERM_DAYS[row][index];
    const int64_t delta = daysFromCivil(year, termMonth, termDay) - today;
    if (delta > 0) {
      snprintf(out, size, "距 %s %d天", SOLAR_TERM_NAMES[index], static_cast<int>(delta));
      return;
    }
  }

  // 跨年：下一个小寒
  if (year < SOLAR_TERM_LAST_YEAR) {
    const int termDay = SOLAR_TERM_DAYS[row + 1][0];
    const int64_t delta = daysFromCivil(year + 1, 1, termDay) - today;
    snprintf(out, size, "距 %s %d天", SOLAR_TERM_NAMES[0], static_cast<int>(delta));
  } else {
    snprintf(out, size, "节气 --");
  }
}
