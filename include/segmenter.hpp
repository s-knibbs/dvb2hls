#ifndef SEGMENTER_H__
#define SEGMENTER_H__
#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include "dvbpsi.hpp"

class DvbDevice;
class Channel;

class Segmenter
{
  dvbpsi_t *m_dvbpsi_pat;
  dvbpsi_t *m_dvbpsi_sdt;
  DvbDevice& m_device;
  bool m_have_pat;
  bool m_have_sdt;
  std::unordered_map<uint16_t, Channel*> m_channel_pids;
  std::map<uint16_t, Channel*> m_channel_ids;
  uint16_t m_tsid;
  bool m_quit;

  static void _process_pat(void* self, dvbpsi_pat_t* pat);
  static void _process_sdt(void* self, dvbpsi_sdt_t* sdt);
  static void _attach_sdt(dvbpsi_t *dvbpsi, uint8_t table_id, uint16_t extension, void* self);
  void _write_channel_index();

public:
  Segmenter(DvbDevice& device);

  ~Segmenter();

  void scan();

  void run();

  void exit()
  {
    m_quit = 1;
  }

};

#endif
