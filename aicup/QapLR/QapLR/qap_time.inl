#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>

using namespace std;

// --- кроссплатформенная версия timegm ---
std::time_t timegm_portable(std::tm* tm) {
#if defined(_WIN32) || defined(_WIN64)
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

// --- безопасное добавление часов к tm без mktime ---
void add_hours_to_tm(std::tm& tm, int hours) {
    std::time_t t = timegm_portable(&tm);
    t += static_cast<std::time_t>(hours) * 3600; // сдвигаем UTC-время
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
}

// --- текущее московское время в строке ---
string qap_time() {
    using namespace std::chrono;
    system_clock::time_point now = system_clock::now();
    auto duration = now.time_since_epoch();
    auto sec = duration_cast<seconds>(duration);
    auto millis = duration_cast<milliseconds>(duration - sec);
    std::time_t t_c = sec.count();

    std::tm utc_tm;
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&utc_tm, &t_c);
#else
    gmtime_r(&t_c, &utc_tm);
#endif

    // Москва = UTC + 3
    add_hours_to_tm(utc_tm, 3);

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y.%m.%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << millis.count();
    return oss.str();
}

// --- парсинг московской строки в system_clock::time_point (UTC внутри) ---
chrono::time_point<chrono::system_clock> parse_qap_time(const std::string& s) {
    using namespace chrono;
    std::tm tm = {};
    int millis;
    char dot;
    std::istringstream iss(s);
    iss >> std::get_time(&tm, "%Y.%m.%d %H:%M:%S");
    iss >> dot >> millis;

    // Москва - UTC
    add_hours_to_tm(tm, -3);

    std::time_t time = timegm_portable(&tm);
    auto tp = system_clock::from_time_t(time);
    tp += milliseconds(millis);
    return tp;
}
long long qap_time_diff(const std::string&t1,const std::string&t2){
  auto tp1=parse_qap_time(t1);
  auto tp2=parse_qap_time(t2);
  return chrono::duration_cast<chrono::milliseconds>(tp2-tp1).count();
}

double qap_time_parse(const string&s){
  static const auto tp1=parse_qap_time("2025.11.04 15:12:26.288");
  auto tp2=parse_qap_time(s);
  return chrono::duration_cast<chrono::milliseconds>(tp2-tp1).count();
}


// --- форматирование UTC-времени в московское ---
string format_time_point(const chrono::system_clock::time_point& tp) {
    using namespace std;
    using namespace chrono;

    auto duration = tp.time_since_epoch();
    auto sec = duration_cast<seconds>(duration);
    auto millis = duration_cast<milliseconds>(duration - sec);
    std::time_t t_c = sec.count();

    std::tm utc_tm;
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&utc_tm, &t_c);
#else
    gmtime_r(&t_c, &utc_tm);
#endif

    // UTC - Москва
    add_hours_to_tm(utc_tm, 3);

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y.%m.%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << millis.count();
    return oss.str();
}

// --- добавление миллисекунд ---
chrono::system_clock::time_point add_milliseconds(
    const chrono::system_clock::time_point& tp, int64_t ms) {
    return tp + chrono::milliseconds(ms);
}

// --- обёртка для удобства ---
string qap_time_addms(const string& t, int64_t ms) {
    auto tp = parse_qap_time(t);
    auto new_tp = add_milliseconds(tp, ms);
    return format_time_point(new_tp);
}
