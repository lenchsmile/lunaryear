// solar_terms.cpp
// 计算某年 24 节气（太阳视黄经每隔15°）的时刻（UTC）与公历日期
// MSVC / C++17 兼容

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <tuple>
#include <string>

using namespace std;

// 常量
const double PI = 3.14159265358979323846;
const double RAD = PI / 180.0;
const double DAYSEC = 86400.0;

// 把角度归一到 [0,360)
double norm360(double x)
{
	double r = fmod(x,360.0);
	if(r < 0) r += 360.0;
	return r;
}

// 儒略日（UTC）从公历日期（年、月、日. fraction）
// 参考：Jean Meeus 算法
double julian_day(int year,int month,double day)
{
	int Y = year;
	int M = month;
	if(M <= 2)
	{
		Y -= 1; M += 12;
	}
	int A = Y / 100;
	int B = 2 - A + A / 4;
	double JD = floor(365.25 * (Y + 4716)) + floor(30.6001 * (M + 1)) + day + B - 1524.5;
	return JD;
}

// 从儒略日得到公历日期（UTC），返回 (year, month, day, fraction_of_day)
tuple<int,int,int,double> date_from_jd(double jd)
{
	double Zf = floor(jd + 0.5);
	double F = (jd + 0.5) - Zf;
	long long Z = (long long)Zf;
	long long A = Z;
	if(Z >= 2299161)
	{
		long long alpha = (long long)((Z - 1867216.25) / 36524.25);
		A = Z + 1 + alpha - alpha / 4;
	}
	long long B = A + 1524;
	long long C = (long long)((B - 122.1) / 365.25);
	long long D = (long long)(365.25 * C);
	long long E = (long long)((B - D) / 30.6001);
	double day = B - D - (long long)(30.6001 * E) + F;
	int month = (E < 14) ? (E - 1) : (E - 13);
	int year = (month > 2) ? (C - 4716) : (C - 4715);
	int day_int = (int)floor(day);
	double frac = day - day_int;
	return make_tuple(year,month,day_int,frac);
}

// 太阳视黄经（degrees）近似计算（含章动修正）
// 采用简化的 Meeus 算法：计算太阳的真黄经并加上小修正得到视黄经
// 精度通常可达数角秒到几十角秒，足够求节气到分钟级
double sun_apparent_longitude(double jd)
{
	// jd in UT
	double T = (jd - 2451545.0) / 36525.0; // Julian centuries from J2000.0

	// Sun's mean longitude (deg), L0
	double L0 = norm360(280.46646 + 36000.76983 * T + 0.0003032 * T * T);

	// Sun's mean anomaly (deg), M
	double M = norm360(357.52911 + 35999.05029 * T - 0.0001537 * T * T);

	// Earth's eccentricity
	double e = 0.016708634 - 0.000042037 * T - 0.0000001267 * T * T;

	// Sun's equation of center C (deg)
	double Mr = M * RAD;
	double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * sin(Mr)
		+ (0.019993 - 0.000101 * T) * sin(2 * Mr)
		+ 0.000289 * sin(3 * Mr);

	// Sun's true longitude (deg)
	double trueLong = L0 + C;

	// Sun's apparent longitude (deg) with nutation/aberration correction
	// omega = longitude of ascending node of Moon's mean orbit on the ecliptic
	double omega = norm360(125.04 - 1934.136 * T);
	double lambda = trueLong - 0.00569 - 0.00478 * sin(omega * RAD);

	return norm360(lambda);
}

// 计算某年从 jd_start 到 jd_end（包含）之间，太阳视黄经是否穿过目标角度 target (deg).
// 返回 true 并把区间 [a,b] 填入 bracket_jd（粗略区间），否则 false.
bool find_bracket_for_angle(double jd_start,double jd_end,double target,double step_days,double& a,double& b)
{
	double prev_jd = jd_start;
	double prev_lon = sun_apparent_longitude(prev_jd);
	// normalize difference to [-180,180)
	auto diff = [&](double lon)
	{
		double d = norm360(lon - target);
		if(d >= 180.0) d -= 360.0;
		return d;
	};
	double prev_d = diff(prev_lon);

	for(double jd = jd_start + step_days; jd <= jd_end + 1e-9; jd += step_days)
	{
		double lon = sun_apparent_longitude(jd);
		double cur_d = diff(lon);
		// if sign change or very close to zero, we have a bracket
		if(prev_d == 0.0 || cur_d == 0.0 || (prev_d > 0 && cur_d < 0) || (prev_d < 0 && cur_d > 0))
		{
			a = prev_jd;
			b = jd;
			return true;
		}
		prev_jd = jd;
		prev_d = cur_d;
	}
	return false;
}

// 在区间 [a,b] 用二分法求解太阳视黄经 = target（精确到秒级）
// 返回求得的 jd
double refine_root_bisection(double a,double b,double target,double tol_seconds = 1.0)
{
	auto f = [&](double jd)
	{
		double lon = sun_apparent_longitude(jd);
		double d = norm360(lon - target);
		if(d >= 180.0) d -= 360.0;
		return d;
	};
	double fa = f(a);
	double fb = f(b);
	// Ensure we have a sign change; if not, still proceed but be cautious
	if(fa == 0.0) return a;
	if(fb == 0.0) return b;

	// Bisection until interval smaller than tol_seconds
	double mid = a;
	while((b - a) * DAYSEC > tol_seconds)
	{
		mid = 0.5 * (a + b);
		double fm = f(mid);
		// choose side where sign changes
		if((fa > 0 && fm <= 0) || (fa < 0 && fm >= 0))
		{
			b = mid;
			fb = fm;
		}
		else
		{
			a = mid;
			fa = fm;
		}
	}
	return 0.5 * (a + b);
}

// 计算某年 24 个节气（target angles = 0,15,...,345）
// 返回 vector of (target_angle_deg, jd_utc)
vector<pair<int,double>> compute_solar_terms_for_year(int year)
{
	// We'll scan jd from Jan 1 00:00 UTC of (year-1) to Jan 1 00:00 UTC of (year+1)
	// to ensure we capture terms that fall near year boundaries.
	double jd_start = julian_day(year - 1,12,31.0 + 0.0); // 00:00 of Dec 31 previous year
	// Better to start at Jan 1 00:00 of year-1 to be safe
	jd_start = julian_day(year,1,1.0) - 40.0; // start ~40 days before Jan1
	double jd_end = julian_day(year,12,31.0) + 40.0; // end ~40 days after Dec31

	vector<pair<int,double>> results;
	// For each target angle (0..23)*15
	for(int k = 0; k < 24; ++k)
	{
		double target = k * 15.0; // degrees
		// Find a bracket where the function crosses zero
		double a,b;
		bool found = find_bracket_for_angle(jd_start,jd_end,target,1.0,a,b);
		if(!found)
		{
			// fallback: try finer scan
			found = find_bracket_for_angle(jd_start,jd_end,target,0.5,a,b);
		}
		if(!found)
		{
			// If still not found, skip (shouldn't happen for normal years)
			continue;
		}
		// refine to high precision (1 second)
		double jd_root = refine_root_bisection(a,b,target,1.0);
		results.emplace_back((int)round(target),jd_root);
	}
	return results;
}

// 辅助：把 jd -> YYYY-MM-DD hh:mm:ss (UTC) 字符串
string jd_to_utc_string(double jd)
{
	int Y,M,D;
	double frac;
	tie(Y,M,D,frac) = date_from_jd(jd);
	// frac is fraction of day
	double day_seconds = frac * DAYSEC;
	if(day_seconds < 0) day_seconds = 0;
	int hh = (int)(day_seconds / 3600);
	int mm = (int)((day_seconds - hh * 3600) / 60);
	int ss = (int)round(day_seconds - hh * 3600 - mm * 60);
	// fix rounding overflow
	if(ss >= 60)
	{
		ss -= 60; mm += 1;
	}
	if(mm >= 60)
	{
		mm -= 60; hh += 1;
	}
	if(hh >= 24)
	{
		// advance date by one day
		double jd_next = floor(jd + 0.5) + 0.5 + 1.0; // next day at 00:00
		tie(Y,M,D,frac) = date_from_jd(jd_next);
		hh = hh - 24;
	}
	char buf[128];
	sprintf_s(buf,"%04d-%02d-%02d %02d:%02d:%02d UTC",Y,M,D,hh,mm,ss);
	return string(buf);
}

// 示例：打印某年 24 节气（按角度顺序）
// 如果需要把角度映射到中文节气名，可在外部提供映射表（不同文化/约定可能有不同起点）
int main()
{
	// 你可以修改 year 以计算其它年份
	int year = 2026;

	cout << "计算年份: " << year << " 的 24 节气（太阳视黄经每隔15°）\n";
	auto terms = compute_solar_terms_for_year(year);

	// 输出表头
	cout << left << setw(8) << "角度" << setw(28) << "UTC 时刻 (公历)" << "说明\n";
	cout << string(60,'-') << "\n";

	for(auto& p : terms)
	{
		int angle = p.first;
		double jd = p.second;
		string dt = jd_to_utc_string(jd);
		// 角度 -> 对应节气（注：此处不直接给中文名，因不同约定起点不同）
		cout << setw(6) << angle << "°" << "  " << setw(28) << dt << "  (黄经=" << angle << "°)\n";
	}

	// 示例：如果你想把结果按中国时区（UTC+8）显示，可以把 jd + 8/24 再转换为日期字符串
	cout << "\n示例：将结果转换为中国时区 (UTC+8) 显示：\n";
	cout << left << setw(8) << "角度" << setw(28) << "北京时间 (公历)" << "\n";
	cout << string(60,'-') << "\n";
	for(auto& p : terms)
	{
		int angle = p.first;
		double jd = p.second + 8.0 / 24.0; // 转为北京时间
		string dt = jd_to_utc_string(jd);
		cout << setw(6) << angle << "°" << "  " << setw(28) << dt << "\n";
	}

	return 0;
}
