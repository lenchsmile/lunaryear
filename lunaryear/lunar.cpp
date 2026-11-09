// lunar.cpp
#include <iostream>
#include <iomanip>
#include <vector>
#include <tuple>
#include <cstdint>

using std::cout;
using std::endl;
using std::vector;
using std::tuple;
using std::tie;
using std::setw;
using std::setfill;

// lunarInfo for 1900..2100
static const int lunarInfo[] = {
	0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,
	0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,
	0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,
	0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,
	0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,
	0x06ca0,0x0b550,0x15355,0x04da0,0x0a5d0,0x14573,0x052d0,0x0a9a8,0x0e950,0x06aa0,
	0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,
	0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b5a0,0x195a6,
	0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,
	0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x05ac0,0x0ab60,0x096d5,0x092e0,
	0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,
	0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,
	0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,
	0x05aa0,0x076a3,0x096d0,0x04bd7,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,
	0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0,
	0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,0x0ea50,0x06b20,0x1a6c4,0x0aae0,
	0x092e0,0x0d2e3,0x0c960,0x0d557,0x0d4a0,0x0da50,0x05d55,0x056a0,0x0a6d0,0x055d4,
	0x052d0,0x0a9b8,0x0a950,0x0b4a0,0x0b6a6,0x0ad50,0x055a0,0x0aba4,0x0a5b0,0x052b0,
	0x0b273,0x06930,0x07337,0x06aa0,0x0ad50,0x14b55,0x04b60,0x0a570,0x054e4,0x0d160,
	0x0e968,0x0d520,0x0daa0,0x16aa6,0x056d0,0x04ae0,0x0a9d4,0x0a2d0,0x0d150,0x0f252,
	0x0d520
};

// days_from_civil: returns days since 1970-01-01 (Howard Hinnant). No timezone issues.
long long days_from_civil(int y,unsigned m,unsigned d)
{
	y -= m <= 2;
	const long long era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = static_cast<unsigned>(y - era * 400);      // [0, 399]
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;         // [0, 146096]
	return era * 146097 + static_cast<long long>(doe) - 719468;
}

int daysBetween(int y,int m,int d)
{
	long long a = days_from_civil(y,m,d);
	long long b = days_from_civil(1900,1,31);
	return static_cast<int>(a - b);
}

int yearDays(int y)
{
	int info = lunarInfo[y - 1900];
	int sum = 348; // 12*29
	for(int i = 0; i < 12; i++) if(info & (0x8000 >> i)) sum += 1;
	int leap = info & 0xf;
	if(leap) sum += (info & 0x10000) ? 30 : 29;
	return sum;
}

int monthDays(int y,int m)
{
	int info = lunarInfo[y - 1900];
	return (info & (0x8000 >> (m - 1))) ? 30 : 29;
}

int leapMonth(int y)
{
	return lunarInfo[y - 1900] & 0xf;
}
int leapDays(int y)
{
	return (lunarInfo[y - 1900] & 0x10000) ? 30 : 29;
}

struct Lunar
{
	int y,m,d; bool isLeap;
};

Lunar solarToLunar(int y,int m,int d)
{
	int offset = daysBetween(y,m,d);
	int iYear = 1900;
	while(offset >= yearDays(iYear))
	{
		offset -= yearDays(iYear);
		iYear++;
	}
	int leap = leapMonth(iYear);
	bool isLeap = false;
	int iMonth = 1;
	while(true)
	{
		int mdays;
		// 正确的闰月判断：当 isLeap==true 且 iMonth==leap 时，取闰月天数
		if(leap && isLeap && iMonth == leap)
		{
			mdays = leapDays(iYear);
		}
		else
		{
			mdays = monthDays(iYear,iMonth);
		}
		if(offset < mdays) break;
		offset -= mdays;
		// 进入或退出闰月的逻辑
		if(leap && iMonth == leap && !isLeap)
		{
			isLeap = true; // 进入闰月（下次循环以闰月天数判断）
		}
		else
		{
			if(isLeap && iMonth == leap)
			{
				// 刚处理完闰月，退出闰月并推进到下一个月
				isLeap = false;
				iMonth++;
			}
			else
			{
				iMonth++;
			}
		}
	}
	return {iYear,iMonth,offset + 1,isLeap};
}

int main()
{
	vector<tuple<int,int,int>> tests = {
		{2023,1,22},
		{2024,2,10},
		{2020,1,25},
		{2000,2,5},
		{2026,1,3},
		{2026,2,1},
		{2033,12,31},
		{2044,2,29},
		{2050,12,25},
		{2099,1,1},
	};
	for(auto& t : tests)
	{
		int y,m,d; tie(y,m,d) = t;
		Lunar L = solarToLunar(y,m,d);
		cout << y << "-" << setw(2) << setfill('0') << m << "-" << setw(2) << d
			<< " -> 农历 " << L.y << "年 " << (L.isLeap ? "闰" : "") << L.m << "月 " << L.d << "日" << endl;
	}
	return 0;
}
