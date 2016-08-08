#ifndef LOG_H__
#define LOG_H__
#include <stdarg.h>

class Log
{
  static Log* m_instance;
  static void _write_msg(int pri, const char* msg, va_list args);
public:
  Log();
  ~Log();
  static void error(const char* msg, ...);
  static void warning(const char* msg, ...);
  static void info(const char* msg, ...);
};

#define ERROR(msg, ...) Log::error(msg, ##__VA_ARGS__)
#define WARNING(msg, ...) Log::warning(msg, ##__VA_ARGS__)
#define INFO(msg, ...) Log::info(msg, ##__VA_ARGS__)

#endif /* LOG_H__ */
