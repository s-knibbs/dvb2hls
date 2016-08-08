#ifndef DAEMON_H__
#define DAEMON_H__

class Log;
class Segmenter;

class Daemon
{
  Log m_log;
  Segmenter& m_segmenter;

public:
  Daemon(Segmenter& segmenter);
  void start();
  static void stop();
};

#endif /* DAEMON_H__ */
