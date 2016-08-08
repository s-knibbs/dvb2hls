#include <algorithm>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "util.hpp"
#include "log.hpp"
#include "segment.hpp"

#include "channel.hpp"
#include <dvbpsi/psi.h>

#define NUM_SEGMENTS 9
#define SEGMENT_LENGTH 9850000000ull // 9.85s in ns
#define NS 1000000000ull
#define INDEX_SUFFIX ".m3u8"
#define HAS_PCR(pkt) ((pkt[3] & 0x20) && (pkt[5] & 0x10) && (pkt[4] >= 7))

timespec Channel::m_curr_time = { 0 };

Channel::Channel(uint16_t id) :
    m_time { 0 },
    m_id(id),
    m_name(),
    m_out_dir(),
    m_output_fd(-1),
    m_buffer_len(0),
    m_segments(),
    m_sequence_number(0),
    m_pids(0),
    m_buf(0),
    m_dvbpsi_pmt(0),
    m_enabled(1),
    m_pat { 0 },
    m_vpid(0)
{
  m_buf = new uint8_t[CHANNEL_BUF_SIZE];
}

void Channel::_process_pmt(void* self, dvbpsi_pmt_t* pmt)
{
  Channel* ths = static_cast<Channel*>(self);
  dvbpsi_pmt_es_t* es = pmt->p_first_es;

  ths->m_pids.push_back(pmt->i_pcr_pid);

  while (es)
  {
    ths->m_pids.push_back(es->i_pid);
    es = es->p_next;
    if (es->i_type == 0x01 || es->i_type == 0x02) ths->m_vpid = es->i_pid;
  }
  dvbpsi_pmt_delete(pmt);
}

void Channel::_create_pat(uint16_t pmt_pid)
{
  uint8_t* p_pkt = &m_pat[0];
  uint8_t len = 0;
  dvbpsi_pat_t pat;
  dvbpsi_psi_section_t* section = 0;
  dvbpsi_t *dvbpsi = dvbpsi_new(handle_dvbpsi_message, DVBPSI_MESSAGE_LEVEL);
  dvbpsi_pat_init(&pat, 1, 0, 1);
  dvbpsi_pat_program_add(&pat, m_id, pmt_pid);
  section = dvbpsi_pat_sections_generate(dvbpsi, &pat, 1);

  m_pat[0] = 0x47;
  m_pat[1] = 0x40;
  m_pat[2] = 0x00;
  m_pat[3] = 0x10;
  m_pat[4] = 0x00;

  len = section->p_payload_end - section->p_data;
  if (section->b_syntax_indicator) len += 4;

  p_pkt += 5;
  memcpy(p_pkt, section->p_data, len);
  p_pkt += len;
  memset(p_pkt, 0xFF, &m_pat[TS_PACKET_SIZE] - p_pkt);

  dvbpsi_DeletePSISections(section);
  dvbpsi_pat_empty(&pat);
  dvbpsi_delete(dvbpsi);
}

void Channel::_flush_channel()
{
  if (write(m_output_fd, m_buf, m_buffer_len) < 0)
  {
    throw WriteException(fmt("Failed writing channel output: %s") % strerror(errno));
  }
  m_buffer_len = 0;
}

void Channel::_write_index_file()
{
  std::string index_file = m_out_dir + INDEX_SUFFIX;
  FILE *index_fd = fopen(index_file.c_str(), "w");
  if (index_fd == NULL)
  {
    throw WriteException
    (
      fmt("Failed to write the index - %s: %s") % index_file % strerror(errno)
    );
  }
  fprintf
  (
    index_fd,
    "#EXTM3U\n"
    "#EXT-X-TARGETDURATION:%u\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-MEDIA-SEQUENCE:%u\n",
    Segment::target_duration,
    m_sequence_number
  );
  m_sequence_number++;
  // Segments must be available for the length of the playlist
  // after they are removed from the file.
  for (int i = (NUM_SEGMENTS - 1) / 2; i > 0; i--)
  {
    // TODO: Use calculate the length of the file using dts values
    fprintf(index_fd, "#EXTINF:%.1f\n", m_segments[i].duration() / 1000.0);
    fprintf(index_fd, "%s\n", m_segments[i].name());
  }
  fprintf(index_fd, "\n");
  fclose(index_fd);
  DEBUG("Wrote index file: %s", index_file.c_str());
}

std::string Channel::index_file() const
{
  return m_out_dir + INDEX_SUFFIX;
}

void Channel::setName(const std::string& name)
{
  m_name = name;
  for (auto chr : name)
  {
    m_out_dir += (chr == ' ') ? '_' : tolower(chr);
  }
}

void Channel::_create_new_segment()
{
  time_t rawtime;
  struct tm *info;
  time(&rawtime);
  info = localtime(&rawtime);
  char time_str[16];
  strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", info);
  std::string segment_file(join_path({m_out_dir, time_str}));
  segment_file += ".ts";

  DEBUG("Creating new segment: %s...", segment_file.c_str());
  if (access(m_out_dir.c_str(), F_OK) < 0)
  {
    if (mkdir(m_out_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0)
    {
      throw DvbException
      (
        fmt("Failed to create channel output directory %s : %s") % m_out_dir % strerror(errno)
      );
    }
  }
  // Delete the oldest segment
  if (m_segments.size() == NUM_SEGMENTS)
  {
    Segment& old = m_segments.back();
    DEBUG("Deleting %s", old.name());
    unlink(old.name());
    m_segments.pop_back();
  }
  if (m_output_fd > 0)
  {
    // Flush any remaining packets.
    _flush_channel();
    close(m_output_fd);
  }
  int mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
  if ((m_output_fd = open(segment_file.c_str(), O_WRONLY | O_CREAT, mode)) < 0)
  {
    throw DvbException
    (
      fmt("Failed to create new segment %s : %s") % segment_file % strerror(errno)
    );
  }

  m_segments.push_front(Segment(segment_file));
  if (m_segments.size() >= ((NUM_SEGMENTS - 1) / 2))
  {
    _write_index_file();
  }
}

int Channel::startPmtScan(dvbpsi_message_cb callback)
{
  m_dvbpsi_pmt = dvbpsi_new(callback, DVBPSI_MESSAGE_LEVEL);
  if (m_dvbpsi_pmt == NULL)
  {
    throw DvbException("Failed to decode PMT");
  }

  if (!dvbpsi_pmt_attach(m_dvbpsi_pmt, m_id, &_process_pmt, this))
  {
    throw DvbException("Failed to decode PMT");
  }
  return 0;
}

std::vector<int>* Channel::readPmt(uint8_t* buf)
{
  if (m_dvbpsi_pmt)
  {
    dvbpsi_packet_push(m_dvbpsi_pmt, buf);
    _create_pat(GET_PID(buf));

    if (m_pids.size() > 0)
    {
      dvbpsi_pmt_detach(m_dvbpsi_pmt);
      dvbpsi_delete(m_dvbpsi_pmt);
      m_dvbpsi_pmt = NULL;
      return &m_pids;
    }
  }
  return NULL;
}

uint8_t Channel::_has_dts(uint8_t* buf)
{
  return 0;
}

void Channel::writePacket(uint8_t* buf, uint16_t pid)
{
  uint8_t* pkt = buf;

  if (!m_enabled) return;
  try
  {
    // Start each segment with PAT
    if ((m_output_fd == -1) || (pid == 0 && _check_new_segment_required()))
    {
      _create_new_segment();
    }

    // Rewrite PAT
    // TODO: Also need to re-write SDT.
    if (pid == 0)
    {
      pkt = &m_pat[0];
      pkt[3] = (((pkt[3] + 1) & 0x0F) | 0x10);
    }

    memcpy(m_buf + m_buffer_len, pkt, TS_PACKET_SIZE);
    m_buffer_len += TS_PACKET_SIZE;
    if (m_buffer_len == CHANNEL_BUF_SIZE)
    {
      _flush_channel();
    }
  }
  catch(WriteException &e)
  {
    ERROR("%s : disabling '%s'", e.what(), m_name.c_str());
    disable(); 
  }
}

inline bool Channel::_check_new_segment_required()
{
  bool new_segment = 0;
  uint64_t delta = (m_curr_time.tv_sec * NS + m_curr_time.tv_nsec) -
    (m_time.tv_sec * NS + m_time.tv_nsec);
  new_segment = (delta >= SEGMENT_LENGTH);
  if (new_segment) m_time = m_curr_time;
  return new_segment;
}

void Channel::disable()
{
  m_enabled = 0;
  _del_output();
}

void Channel::_del_output()
{
  // Delete all output data
  for (auto& segment : m_segments)
  {
    remove(segment.name());
  }
  remove((m_out_dir + INDEX_SUFFIX).c_str());
  remove(m_out_dir.c_str());
}

Channel::~Channel()
{
  if (m_enabled) _del_output();
  if (m_dvbpsi_pmt)
  {
    dvbpsi_pmt_detach(m_dvbpsi_pmt);
    dvbpsi_delete(m_dvbpsi_pmt);
    m_dvbpsi_pmt = NULL;
  }
  if (m_buf)
  {
    delete[] m_buf;
  }
}
