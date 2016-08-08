#include <sys/types.h>
#include <sys/unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fstream>

#include "util.hpp"
#include "log.hpp"
#include "segmenter.hpp"
#include "daemon.hpp"

#define PID_FILE "/run/shm/dvb_hls.pid"

Daemon::Daemon(Segmenter& segmenter) :
  m_log(),
  m_segmenter(segmenter)
{
}

void Daemon::start()
{
  if (daemon(0, 0) < 0)
  {
    throw DvbException(fmt("Failed to start the daemon: %s") % strerror(errno));
  }
  std::ofstream pid_file(PID_FILE, std::ofstream::app);
  pid_file << getpid() << std::endl;
  pid_file.close();
  m_segmenter.run();
}

void Daemon::stop()
{
  std::ifstream pid_file(PID_FILE);
  pid_t pid;
  if (!pid_file.good())
    throw DvbException("No daemon process running");

  // Kill all processes listed in pid_file.
  while ((pid_file >> pid).good())
  {
    if (kill(pid, SIGTERM) < 0)
    {
      throw DvbException(fmt("Failed to kill process '%d': %s") % pid % strerror(errno));
    }
    INFO("Stopped %d", pid);
  }
  remove(PID_FILE);
}
