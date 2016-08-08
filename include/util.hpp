#ifndef UTIL_H__
#define UTIL_H__
#include <stdexcept>
#include <string>
#include <stdio.h>
#include <vector>
#include <boost/format.hpp>
#include "dvbpsi.hpp"

#ifdef BUILD_DEBUG
#define DEBUG(msg, ...) fprintf(stderr, "Debug - " msg "\n", ##__VA_ARGS__)
#else
#define DEBUG(msg, ...) ((void)0)
#endif

#ifdef BUILD_DEBUG
#define DVBPSI_MESSAGE_LEVEL DVBPSI_MSG_DEBUG
#else
#define DVBPSI_MESSAGE_LEVEL DVBPSI_MSG_WARN
#endif

#define GET_PID(pkt) (((pkt[1] & 0x1F) << 8) | pkt[2])

class DvbException : public std::runtime_error
{
  
public:
  DvbException(const std::string& msg) :
    std::runtime_error(msg)
  {
  }

  DvbException(const boost::format& msg) :
    std::runtime_error(msg.str())
  {
  }

};

std::string join_path(std::vector<std::string> path);

void handle_dvbpsi_message(dvbpsi_t *_, const dvbpsi_msg_level_t level, const char* msg);

using fmt = boost::format;

#endif /* UTIL_H__ */
