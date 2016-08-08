#include "util.hpp"
#include "log.hpp"

std::string join_path(std::vector<std::string> path)
{
  const char sep = '/';
  std::string joined;
  for (auto& part : path)
  {
    joined += part;
    if (part.back() != sep && part != path.back())
    {
      joined += sep;
    }
  }
  return joined;
}

void handle_dvbpsi_message(dvbpsi_t *_, const dvbpsi_msg_level_t level, const char* msg)
{
  switch (level)
  {
  case DVBPSI_MSG_ERROR:
    ERROR("%s", msg);
    break;
  case DVBPSI_MSG_WARN:
    WARNING("%s", msg);
    break;
  case DVBPSI_MSG_DEBUG:
    DEBUG("%s", msg);
    break;
  default:
    return;
  }
}
