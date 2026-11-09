// solar_terms_chinese.cpp
// 计算某年 24 节气（太阳视黄经每隔15°），并按中国传统顺序（从立春开始）
// 输出：节气名 + 公历日期 + 时刻（北京时间，UTC+8）
// MSVC / C++17 兼容

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <tuple>
#include <string>

using namespace std;

const double PI = 3.14159265358979323846;
const double RAD = PI / 180.0;
const double DAYSEC = 86400.0;

double norm360(double x)
{
	double r = fmod(x,360.0);
	if(r < 0) r += 360.0;
	return r;
}

// Julian Day from calendar date (year, month, day.fraction), UT
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

// calendar date from Julian Day (returns year, month, day_int, fraction_of_day)
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
	int month = (E < 14) ? (int)(E - 1) : (int)(E - 13);
	int year = (month > 2) ? (int)(C - 4716) : (int)(C - 4715);
	int day_int = (int)floor(day);
	double frac = day - day_int;
	return make_tuple(year,month,day_int,frac);
}

// Sun apparent longitude (degrees) — simplified Meeus-style with nutation/aberration correction
double sun_apparent_longitude(double jd)
{
	double T = (jd - 2451545.0) / 36525.0;
	double L0 = norm360(280.46646 + 36000.76983 * T + 0.0003032 * T * T);
	double M = norm360(357.52911 + 35999.05029 * T - 0.0001537 * T * T);
	double Mr = M * RAD;
	double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * sin(Mr)
		+ (0.019993 - 0.000101 * T) * sin(2 * Mr)
		+ 0.000289 * sin(3 * Mr);
	double trueLong = L0 + C;
	double omega = norm360(125.04 - 1934.136 * T);
	double lambda = trueLong - 0.00569 - 0.00478 * sin(omega * RAD);
	return norm360(lambda);
}

// find bracket where sun longitude crosses target (scan with step_days)
bool find_bracket_for_angle(double jd_start,double jd_end,double target,double step_days,double& a,double& b)
{
	double prev_jd = jd_start;
	double prev_lon = sun_apparent_longitude(prev_jd);
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

// refine root in [a,b] where sun longitude == target using bisection to tol_seconds
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
	if(fa == 0.0) return a;
	if(fb == 0.0) return b;
	double mid = a;
	while((b - a) * DAYSEC > tol_seconds)
	{
		mid = 0.5 * (a + b);
		double fm = f(mid);
		if((fa > 0 && fm <= 0) || (fa < 0 && fm >= 0))
		{
			b = mid; fb = fm;
		}
		else
		{
			a = mid; fa = fm;
		}
	}
	return 0.5 * (a + b);
}

// compute 24 solar terms for a given year; returns vector of (angle_deg, jd_utc)
vector<pair<int,double>> compute_solar_terms_for_year(int year)
{
	double jd_start = julian_day(year,1,1.0) - 40.0;
	double jd_end = julian_day(year,12,31.0) + 40.0;
	vector<pair<int,double>> results;
	for(int k = 0; k < 24; ++k)
	{
		double target = k * 15.0;
		double a,b;
		bool found = find_bracket_for_angle(jd_start,jd_end,target,1.0,a,b);
		if(!found) found = find_bracket_for_angle(jd_start,jd_end,target,0.5,a,b);
		if(!found) continue;
		double jd_root = refine_root_bisection(a,b,target,1.0);
		results.emplace_back((int)round(target),jd_root);
	}
	return results;
}

// jd -> 北京时间字符串 (YYYY-MM-DD hh:mm:ss) ; add 8 hours (UTC+8)
string jd_to_beijing_string(double jd)
{
	double jd_bj = jd + 8.0 / 24.0;
	int Y,M,D;
	double frac;
	tie(Y,M,D,frac) = date_from_jd(jd_bj);
	double day_seconds = frac * DAYSEC;
	if(day_seconds < 0) day_seconds = 0;
	int hh = (int)(day_seconds / 3600);
	int mm = (int)((day_seconds - hh * 3600) / 60);
	int ss = (int)round(day_seconds - hh * 3600 - mm * 60);
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
		double jd_next = floor(jd_bj + 0.5) + 0.5 + 1.0;
		tie(Y,M,D,frac) = date_from_jd(jd_next);
		hh = hh - 24;
	}
	char buf[128];
	sprintf_s(buf,"%04d-%02d-%02d %02d:%02d:%02d",Y,M,D,hh,mm,ss);
	return string(buf);
}

int main()
{
	// 中文节气名按中国传统顺序（从立春开始）
	// 对应角度序列（度）：315,330,345,0,15,30,45,60,75,90,105,120,135,150,165,180,195,210,225,240,255,270,285,300
	const vector<string> jieqi_names = {
		"立春","雨水","惊蛰","春分","清明","谷雨",
		"立夏","小满","芒种","夏至","小暑","大暑",
		"立秋","处暑","白露","秋分","寒露","霜降",
		"立冬","小雪","大雪","冬至","小寒","大寒"
	};
	const vector<int> jieqi_angles = {
		315,330,345,0,15,30,45,60,75,90,105,120,135,150,165,180,195,210,225,240,255,270,285,300
	};

	int year;
	cout << "请输入要计算的年份（例如 2026），回车确认：";
	if(!(cin >> year)) return 0;

	auto terms = compute_solar_terms_for_year(year);

	// 将计算结果按角度建立映射 angle->jd
	// terms 中角度为 0,15,...,345
	vector<double> angle_to_jd(360,NAN);
	for(auto& p : terms)
	{
		int angle = p.first % 360;
		angle_to_jd[angle] = p.second;
	}

	cout << "\n" << year << " 年 24 节气（北京时间，UTC+8）\n";
	cout << left << setw(8) << "序号" << setw(8) << "节气" << setw(22) << "公历日期" << "说明\n";
	cout << string(60,'-') << "\n";

	for(size_t i = 0; i < jieqi_names.size(); ++i)
	{
		int ang = jieqi_angles[i];
		double jd = angle_to_jd[ang];
		if(isnan(jd))
		{
			// 若未找到精确角度（极少见），尝试邻近角度搜索
			double found_jd = NAN;
			for(int delta = -1; delta <= 1; ++delta)
			{
				int a = (ang + delta * 15 + 360) % 360;
				if(!isnan(angle_to_jd[a]))
				{
					found_jd = angle_to_jd[a]; break;
				}
			}
			jd = found_jd;
		}
		string bj = isnan(jd) ? string("未找到") : jd_to_beijing_string(jd);
		cout << setw(6) << (i + 1) << setw(8) << jieqi_names[i] << setw(22) << bj << " (黄经=" << ang << "°)\n";
	}

	return 0;
}
