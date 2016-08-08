#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <boost/program_options.hpp>

#include "util.hpp"
#include "log.hpp"
#include "dvb.hpp"
#include "segmenter.hpp"
#include "daemon.hpp"

#define TO_STR(identifier) #identifier

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

static std::string multiplex;
static std::string transmitter;
static std::string tuning_dir;
static uint16_t adapter;
static bool start_daemon = false;
static bool stop_daemon = false;

static Segmenter *p_segmenter = 0;

namespace po = boost::program_options;

static int parse_arguments(int argc, char **argv)
{
  int ret = 0;
  boost::format description("\n"
      "DVB - HTTP Live Streaming (HLS) server V%d.%d\n"
      "\n"
      "Uses Apple's HTTP Live Streaming protocol to stream channels from a\n"
      "dvb multiplex. Currently supports DVB-T/T2.\n"
      "The generated .ts segments and .m3u8 index files are served\n"
      "up separately with a webserver such as Apache.\n\n"
      "Options");
  po::options_description desc((description % VERSION_MAJOR % VERSION_MINOR).str());
  desc.add_options()
      ("help,h", "Print this help message and exit.")
      ("tuning-file,t", po::value<std::string>(&transmitter)->required(), "Name of the tuning file.")
      ("tuning-path,p", po::value<std::string>(&tuning_dir)->default_value(TUNING_PATH),
          "Path to the tuning files.")
      ("multiplex,m", po::value<std::string>(&multiplex)->required(), "Name of the multiplex in the tuning file to use.")
      ("adapter,a", po::value<uint16_t>(&adapter)->default_value(0), "Adapter number to use.")
      ("daemon,d", "Run as a daemon.")
      ("stop,s", "Stop any existing daemon processes.");
  po::variables_map args;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), args);
  try
  {
    po::notify(args);
  }
  catch (po::error &e)
  {
    if (!(args.count("help") || args.count("stop")))
    {
      std::cerr << desc << std::endl;
      std::cerr << e.what() << std::endl;
      ret = -1;
    }
  }
  if (args.count("help"))
  {
    std::cout << desc;
    ret = -1;
  }
  else
  {
    stop_daemon = args.count("stop");
    start_daemon = args.count("daemon");
  }
  return ret;
}

static void catch_signals(int signo)
{
  std::string name;
  switch(signo)
  {
  case SIGINT:
    name = TO_STR(SIGINT);
    break;
  case SIGTERM:
    name = TO_STR(SIGTERM);
    break;
  case SIGHUP:
    name = TO_STR(SIGHUP);
    break;
  default:
    name = std::to_string(signo);
    break;
  }
  INFO("Caught signal %s shutting down...", name.c_str());
  if (p_segmenter)
    p_segmenter->exit();
  else
    exit(0);
}

static void set_sig_hndlr(int signo)
{
  if (signal(signo, catch_signals) == SIG_ERR)
  {
    DvbException(fmt("Failed to register signal handler: %s") % strerror(errno));
  }
}

static int run()
{
  DvbDevice device(multiplex, join_path({tuning_dir, transmitter}), adapter);
  Segmenter segmenter(device);
  device.open_device();
  if (device.tune() == 0)
  {
    INFO("Tuned device to %s", multiplex.c_str());
    segmenter.scan();
    p_segmenter = &segmenter;

    // Register signal handlers.
    set_sig_hndlr(SIGINT);
    set_sig_hndlr(SIGTERM);
    set_sig_hndlr(SIGHUP);

    if (start_daemon)
    {
      INFO("Starting daemon...");
      Daemon daemon(segmenter);
      daemon.start();
    }
    else
    {
      segmenter.run();
    }
  }
  return 0;
}

int main(int argc, char **argv)
{
  int exit_code = 1;

  if (parse_arguments(argc, argv) < 0)
  {
    goto exit;
  }

  try
  {
    if (stop_daemon)
    {
      Daemon::stop();
      exit_code = 0;
    }
    else
    {
      exit_code = run();
    }
  }
  catch (std::exception &e)
  {
    p_segmenter = 0;
    ERROR("%s", e.what());
  }
  exit:
    return exit_code;
}
