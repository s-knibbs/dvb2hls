#include <syslog.h>
#include <stdio.h>
#include <string>

#include "log.hpp"

#define ERR_PREFIX "Error - "
#define WARN_PREFIX "Warning - "
#define INFO_PREFIX "Info - "

Log* Log::m_instance = 0;

Log::Log()
{
  setlogmask(LOG_UPTO (LOG_INFO));
  openlog("dvb-hls", LOG_PID | LOG_NDELAY, LOG_DAEMON);
  m_instance = this;
}

void Log::error(const char* msg, ...)
{
  va_list vl;
  va_start(vl, msg);
  _write_msg(LOG_ERR, msg, vl);
  va_end(vl);
}

void Log::warning(const char* msg, ...)
{
  va_list vl;
  va_start(vl, msg);
  _write_msg(LOG_WARNING, msg, vl);
  va_end(vl);
}

void Log::info(const char* msg, ...)
{
  va_list vl;
  va_start(vl, msg);
  _write_msg(LOG_INFO, msg, vl);
  va_end(vl);
}

void Log::_write_msg(int pri, const char* msg, va_list args)
{
  std::string full_msg;
  switch (pri)
  {
  case LOG_ERR:
    full_msg = ERR_PREFIX;
    break;
  case LOG_WARNING:
    full_msg = WARN_PREFIX;
    break;
  case LOG_INFO:
    full_msg = INFO_PREFIX;
    break;
  default:
    break;
  }
  full_msg += msg;

  if (m_instance)
  {
    vsyslog(pri, full_msg.c_str(), args);
  }
  else
  {
    full_msg += '\n';
    vfprintf(stderr, full_msg.c_str(), args);
  }
}

Log::~Log()
{
  closelog();
  m_instance = 0;
}

