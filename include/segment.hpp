#ifndef SEGMENT_H__
#define SEGMENT_H__

#include <string>
#include "stdint.h"

#define DECODE_CLOCK 90000ul

class Segment
{
  std::string m_name;
  uint64_t m_first_dts;
  uint64_t m_last_dts;

public:
  Segment(const std::string& name);

  void updateDts(uint64_t dts);
  uint16_t duration() const;
  const char* name() const
  {
    return m_name.c_str();
  }
  static const unsigned target_duration = 10;
};

Segment::Segment(const std::string& name) :
    m_name(name),
    m_first_dts(UINT64_MAX),
    m_last_dts(0)
{
}

void Segment::updateDts(uint64_t dts)
{
  if (m_first_dts == UINT64_MAX)
  {
    m_first_dts = dts;
  }
  else
  {
    m_last_dts = dts;
  }
}

uint16_t Segment::duration() const
{
  return (m_last_dts > m_first_dts) ?
      (m_last_dts - m_first_dts) / (DECODE_CLOCK / 1000) :
      target_duration * 1000;
}

#endif
