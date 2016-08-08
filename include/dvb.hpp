#ifndef DVB_H__
#define DVB_H__

#include <stdint.h>
#include <stdbool.h>
#include <string>

#define MAX_REQUIRED_PID 20
#define BASE_PATH "/dev/dvb/adapter%u/"
#define FRONTEND_PATH BASE_PATH "frontend0"
#define TUNING_PATH "/usr/share/dvb/dvb-t/"
#define DEMUX_PATH BASE_PATH "demux0"
#define DVR_PATH BASE_PATH "dvr0"
#define NUM_PIDS 8192


class DvbDevice
{
  std::string m_multiplex;
  std::string m_transmitter;
  uint32_t m_frequency;
  uint8_t m_inversion;
  uint8_t m_bandwidth;
  uint8_t m_fec_hp;
  uint8_t m_fec_lp;
  uint8_t m_modulation;
  uint8_t m_trans_mode;
  uint8_t m_guard_interval;
  uint8_t m_hierarchy;
  uint8_t m_delivery_sys;
  int m_demux;
  int m_frontend;
  uint16_t m_adapter_id;

  int _poll_status();
  int _set_ts_filter();
  int _read_multiplex();
  int _get_status();

public:
  DvbDevice(std::string multiplex, std::string transmitter, uint16_t adapter);
  int open_device();
  int tune();
  int read_card(uint8_t *buf, size_t size);
  const std::string& get_multiplex() const
  {
    return m_multiplex;
  }

  ~DvbDevice();
};

bool* get_required_pids();

#endif
