#ifndef CHANNEL_H__
#define CHANNEL_H__

#include <stdint.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <deque>
#include <time.h>
#include <sys/time.h>
#include <boost/format.hpp>

#include "dvbpsi.hpp"
#include "log.hpp"
#include "dvb_hls.hpp"

#define CHANNEL_BUF_SIZE (22 * TS_PACKET_SIZE) // Approx 4kB

class WriteException : public std::runtime_error
{
public:

  WriteException(const boost::format& msg) :
    std::runtime_error(msg.str())
  {
  }
  
  WriteException(const std::string& msg) :
    std::runtime_error(msg)
  {
  }
};

class Segment;

class Channel
{
  static timespec m_curr_time;
  timespec m_time;
  uint16_t m_id;
  std::string m_name;
  std::string m_out_dir;
  int m_output_fd;
  uint16_t m_buffer_len;
  std::deque<Segment> m_segments;
  uint32_t m_sequence_number;
  std::vector<int> m_pids;
  uint8_t *m_buf;
  dvbpsi_t *m_dvbpsi_pmt;
  bool m_enabled;
  uint8_t m_pat[TS_PACKET_SIZE];
  uint16_t m_vpid;

  static void _process_pmt(void* self, dvbpsi_pmt_t* pmt);
  void _flush_channel();
  void _create_new_segment();
  void _write_index_file();
  bool _check_new_segment_required();
  void _del_output();
  void _create_pat(uint16_t pmt_pid);
  uint8_t _has_dts(uint8_t* buf);

public:
  ~Channel();

  Channel(uint16_t id);

  void setName(const std::string& name);

  const std::string& getName() const
  {
    return m_name;
  }

  void disable();

  bool enabled() const
  {
    return m_enabled;
  }

  uint16_t id() const
  {
    return m_id;
  }
  std::string index_file() const;

  int startPmtScan(dvbpsi_message_cb callback);

  std::vector<int>* readPmt(uint8_t* buf);

  void writePacket(uint8_t* buf, uint16_t pid);

  static void set_curr_time()
  {
    clock_gettime(CLOCK_MONOTONIC, &m_curr_time);
  }
};

#endif
