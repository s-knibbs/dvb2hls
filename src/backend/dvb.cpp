#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/unistd.h>
#include <sys/poll.h>
#include <signal.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <fstream>
#include <string.h>
#include <map>
#include <array>

#include "dvb.hpp"
#include "log.hpp"
#include "util.hpp"
#include "dvb_hls.hpp"

#define READ_TIMEOUT_MSECS 5000

DvbDevice::DvbDevice(std::string multiplex, std::string transmitter, uint16_t adapter) :
    m_multiplex(multiplex),
    m_transmitter(transmitter),
    m_frequency(0),
    m_inversion(0),
    m_bandwidth(0),
    m_fec_hp(0),
    m_fec_lp(0),
    m_modulation(0),
    m_trans_mode(0),
    m_guard_interval(0),
    m_hierarchy(0),
    m_delivery_sys(0),
    m_demux(-1),
    m_frontend(-1),
    m_adapter_id(adapter)
{
}

int DvbDevice::_read_multiplex()
{
  std::ifstream scan_file(m_transmitter.c_str());
  const size_t buf_len = 256;
  char line[buf_len];
  char multiplex[buf_len] = { 0 };
  char property[buf_len] = { 0 };
  char value[buf_len] = { 0 };
  bool multiplex_found = 0;

  if (!scan_file.good())
  {
    throw DvbException(fmt("Failed to read scanning file: %s") % strerror(errno));
  }

  while (scan_file.good())
  {
    scan_file.getline(line, buf_len);
    if (line[0] == '[')
    {
      sscanf(line, "[%*s %[^]]]", multiplex);
    }
    else if (m_multiplex == multiplex)
    {
      multiplex_found = 1;
      if (sscanf(line, "\t%s = %s", property, value) < 2)
        continue;
      if (strcmp(property, "DELIVERY_SYSTEM") == 0)
      {
        if (strcmp(value, "DVBT2") == 0)
        {
          m_delivery_sys = SYS_DVBT2;
        }
        else
        {
          m_delivery_sys = SYS_DVBT;
        }
      }
      else if (strcmp(property, "FREQUENCY") == 0)
      {
        m_frequency = strtoul(value, NULL, 0);
      }
      else if (strcmp(property, "BANDWIDTH_HZ") == 0)
      {
        unsigned long bdw = strtoul(value, NULL, 0);
        switch (bdw)
        {
        case 8000000:
          m_bandwidth = BANDWIDTH_8_MHZ;
          break;
        case 7000000:
          m_bandwidth = BANDWIDTH_7_MHZ;
          break;
        case 6000000:
          m_bandwidth = BANDWIDTH_6_MHZ;
          break;
        default:
          m_bandwidth = BANDWIDTH_AUTO;
          break;
        }
      }
      else if (strcmp(property, "CODE_RATE_HP") == 0)
      {
        if (strcmp(value, "2/3") == 0)
        {
          m_fec_hp = FEC_2_3;
        }
        else if (strcmp(value, "3/4") == 0)
        {
          m_fec_hp = FEC_3_4;
        }
        else
        {
          m_fec_hp = FEC_AUTO;
        }
      }
      else if (strcmp(property, "TRANSMISSION_MODE") == 0)
      {
        if (strcmp(value, "2K") == 0)
        {
          m_trans_mode = TRANSMISSION_MODE_2K;
        }
        else if (strcmp(value, "8K") == 0)
        {
          m_trans_mode = TRANSMISSION_MODE_8K;
        }
        else if (strcmp(value, "32K") == 0)
        {
          m_trans_mode = TRANSMISSION_MODE_32K;
        }
        else
        {
          m_trans_mode = TRANSMISSION_MODE_AUTO;
        }
      }
      else if (strcmp(property, "GUARD_INTERVAL") == 0)
      {
        if (strcmp(value, "1/32") == 0)
        {
          m_guard_interval = GUARD_INTERVAL_1_32;
        }
        else if (strcmp(value, "1/128") == 0)
        {
          m_guard_interval = GUARD_INTERVAL_1_128;
        }
        else if (strcmp(value, "1/16") == 0)
        {
          m_guard_interval = GUARD_INTERVAL_1_16;
        }
        else
        {
          m_guard_interval = GUARD_INTERVAL_AUTO;
        }
      }
      else if (strcmp(property, "MODULATION") == 0)
      {
        if (strcmp(value, "QPSK") == 0)
        {
          m_modulation = QPSK;
        }
        else if (strcmp(value, "QAM/16") == 0)
        {
          m_modulation = QAM_16;
        }
        else if (strcmp(value, "QAM/64") == 0)
        {
          m_modulation = QAM_64;
        }
        else if (strcmp(value, "QAM/256") == 0)
        {
          m_modulation = QAM_256;
        }
        else
        {
          m_modulation = QAM_AUTO;
        }
      }
    }
  }
  m_inversion = INVERSION_AUTO;

  if (!multiplex_found)
  {
    throw DvbException(fmt("Multiplex '%s' not found in scanning file.") % m_multiplex.c_str());
  }

  // Check that we have valid parameters.
  if (m_frequency == 0)
  {
    throw DvbException("Failed to parse the scanning file - no frequency set.");
  }

  return 0;
}

int DvbDevice::open_device()
{
  char path[128];
  snprintf(path, sizeof(path), FRONTEND_PATH, m_adapter_id);
  if ((m_frontend = open(path, O_RDWR | O_NONBLOCK)) < 0)
  {
    throw DvbException(fmt("Failed to open the frontend device: %s") % strerror(errno));
  }
  return 0;
}

int DvbDevice::_get_status()
{
  fe_status_t festatus;
  memset(&festatus, 0, sizeof(fe_status_t));
  int ret = 0;
  if (ioctl(m_frontend, FE_READ_STATUS, &festatus) != -1)
  {
    DEBUG("Fe-status 0x%x", festatus);
    ret = ((festatus & FE_HAS_LOCK) != 0);
    if (festatus & FE_TIMEDOUT)
    {
      throw DvbException("No signal");
    }
  }
  else
  {
    if (errno == EINTR) return -1;
    throw DvbException(fmt("Failed to read the card status: %s") % strerror(errno));
  }
  return ret;
}

DvbDevice::~DvbDevice()
{
  if (m_demux != -1)
  {
    ioctl(m_demux, DMX_STOP);
    close(m_demux);
  }

  if (m_frontend != -1)
    close(m_frontend);
}


int DvbDevice::_set_ts_filter()
{
  struct dmx_pes_filter_params filter_params;
  // Common filter_params
  filter_params.input = DMX_IN_FRONTEND;
  filter_params.output = DMX_OUT_TSDEMUX_TAP;
  filter_params.flags = DMX_IMMEDIATE_START;
  filter_params.pes_type = DMX_PES_OTHER;
  const int pid = 8192;
  char path[128];
  snprintf(path, sizeof(path), DEMUX_PATH, m_adapter_id);
  if ((m_demux = open(path, O_RDONLY | O_NONBLOCK)) < 0)
  {
    throw DvbException(fmt("Failed to open demux device: %s") % strerror(errno));
  }
  // Increase buffer to 1MB
  if (ioctl(m_demux, DMX_SET_BUFFER_SIZE, 1 << 20) < 0)
    throw DvbException("Failed to increase demux buffer");
  filter_params.pid = pid;
  if (ioctl(m_demux, DMX_SET_PES_FILTER, &filter_params) < 0)
  {
    throw DvbException(fmt("Failed to set the filter for PID %d: %s") % pid % strerror(errno));
  }
  return 0;
}

int DvbDevice::_poll_status()
{
  struct pollfd pfd[1] = 
  {
    { /* .fd = */ m_frontend, /* .events = */ POLLPRI }
  };
  int has_lock = 0;
  const int max_poll = 40;
  int polls = 0;
  while (!has_lock && polls < max_poll)
  {
    int ret = poll(pfd, 1, READ_TIMEOUT_MSECS);
    if (ret > 0)
    {
      if (pfd[0].revents & POLLPRI)
      {
        if ((has_lock = _get_status()) < 0)
        {
          break;
        }
      }
    }
    else if (ret < 0)
    {
      throw DvbException(fmt("Getting device status failed: %s") % strerror(errno));
    }
    else
    {
      throw DvbException("Device timed out");
    }
    usleep(200000);
    polls++;
  }
  if (polls == max_poll)
  {
    throw DvbException("Failed to tune device.");
  }
  return has_lock;
}

int DvbDevice::tune()
{
  _read_multiplex();
  const uint8_t num_commands = 11;
  struct dtv_property props[num_commands];
  struct dtv_properties dtv_props = { /*.num = */ num_commands, /* .props = */ props };
  int ret = -1;
  uint32_t dtv_bandwidth_hz = 0;

  DEBUG("Tuning device.");

  switch (m_bandwidth)
  {
  case BANDWIDTH_7_MHZ:
    dtv_bandwidth_hz = 7000000;
    break;
  case BANDWIDTH_8_MHZ:
    dtv_bandwidth_hz = 8000000;
    break;
  case BANDWIDTH_6_MHZ:
    dtv_bandwidth_hz = 6000000;
    break;
  case BANDWIDTH_AUTO:
  default:
    dtv_bandwidth_hz = 0;
    break;
  }

  uint8_t cmds[] = 
  {
    DTV_CLEAR, DTV_DELIVERY_SYSTEM, DTV_FREQUENCY,
    DTV_BANDWIDTH_HZ, DTV_CODE_RATE_HP,
    DTV_CODE_RATE_LP, DTV_MODULATION, DTV_GUARD_INTERVAL,
    DTV_TRANSMISSION_MODE, DTV_HIERARCHY, DTV_INVERSION
  };
  uint32_t data[] = 
  {
    0, m_delivery_sys, m_frequency, dtv_bandwidth_hz, m_fec_hp,
    m_fec_lp, m_modulation, m_guard_interval, m_trans_mode,
    m_hierarchy, m_inversion
  };

  for (int i = 0; i < num_commands; i++)
  {
    dtv_props.props[i].cmd = cmds[i];
    dtv_props.props[i].u.data = data[i];
  }

  if (ioctl(m_frontend, FE_SET_PROPERTY, &dtv_props) == -1)
  {
    throw DvbException(fmt("FE_SET_PROPERTY failed: %s") % strerror(errno));
  }
  // Send the DTV_TUNE command.
  struct dtv_property tune_prop[1];
  struct dtv_properties tune_props = {1, tune_prop};
  tune_props.props[0].cmd = DTV_TUNE;
  tune_props.props[0].u.data = 0;
  if (ioctl(m_frontend, FE_SET_PROPERTY, &tune_props) == -1)
  {
    throw DvbException(fmt("FE_SET_PROPERTY DTV_TUNE failed: %s") % strerror(errno));
  }
  usleep(100000);
  // Wait until tuned.
  ret = _poll_status();
  if (ret == 1)
  {
    ret = _set_ts_filter();
  }
  return ret;
}

int DvbDevice::read_card(uint8_t *buf, size_t size)
{
  int len = -1;
  struct pollfd pfd[1] =
  {
    { /* .fd = */ m_demux, /* .events = */ POLLIN }
  };
  int total = 0;

  while (size)
  {
    int p = poll(pfd, 1, READ_TIMEOUT_MSECS);
    if (p > 0)
    {
      len = read(m_demux, buf, size);
      if (len < 0 && errno == EOVERFLOW)
      {
        WARNING("Demux buffer overflow");
        continue;
      }
      if (len < 0)
      {
        if (errno == EINTR) return 0;
        throw DvbException(fmt("Failed to read from the dvr device: %s") % strerror(errno));
      }
      size -= len;
      buf += len;
      total += len;
    }
    else if (p < 0)
    {
      if (errno == EINTR) return 0;
      throw DvbException(fmt("Failed to read from the dvr device: %s") % strerror(errno));
    }
    else
    {
      // Check tuning status
      if (_get_status() == 1)
      {
        throw DvbException("Device read timeout");
      }
      else
      {
        WARNING("Signal lost.");
        return 0;
      }
    }
  }

  // Return number of packets read.
  return total / TS_PACKET_SIZE;
}
