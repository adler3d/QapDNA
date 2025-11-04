string qap_time(){
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
  utc_tm.tm_hour += 3;
  std::mktime(&utc_tm);
  std::ostringstream oss;
  oss << std::put_time(&utc_tm, "%Y.%m.%d %H:%M:%S")<< '.' << std::setfill('0') << std::setw(3) << millis.count();
  return oss.str();
}
chrono::time_point<chrono::system_clock> parse_qap_time(const std::string& s) {
  using namespace chrono;
  std::tm tm = {};
  int millis;
  std::istringstream iss(s);
  iss >> std::get_time(&tm, "%Y.%m.%d %H:%M:%S");
  char dot;iss >> dot >> millis;
  tm.tm_hour -= 3;
  std::time_t time = std::mktime(&tm);
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