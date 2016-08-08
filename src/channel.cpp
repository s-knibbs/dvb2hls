#include <algorithm>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "channel.hpp"
#include "util.hpp"
#include "log.hpp"

#define CHANNEL_BUF_SIZE (22*TS_PACKET_SIZE) // Approx 4kB
#define NUM_SEGMENTS 9
#define SEGMENT_LENGTH 10000000 // 10s
#define INDEX_SUFFIX ".m3u8"

timeval Channel::m_curr_time = { 0 };

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
    m_enabled(1)
{
  m_buf = new uint8_t[CHANNEL_BUF_SIZE];
}

void Channel::_process_pmt(void* self, dvbpsi_pmt_t* pmt)
{
  Channel* ths = static_cast<Channel*>(self);
  dvbpsi_pmt_es_t* es = pmt->p_first_es;
  while (es)
  {
    ths->m_pids.push_back(es->i_pid);
    es = es->p_next;
  }
  dvbpsi_pmt_delete(pmt);
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
    "#EXT-X-TARGETDURATION:10\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-MEDIA-SEQUENCE:%u\n",
    m_sequence_number
  );
  m_sequence_number++;
  // Segments must be available for the length of the playlist
  // after they are removed from the file.
  for (int i = (NUM_SEGMENTS - 1) / 2; i > 0; i--)
  {
    fprintf(index_fd, "#EXTINF:10.0\n");
    fprintf(index_fd, "%s\n", m_segments[i].c_str());
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
  std::string segment(m_out_dir);
  segment += "/";
  segment += time_str;
  segment += ".ts";

  DEBUG("Creating new segment: %s...", segment.c_str());
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
    std::string old = m_segments.back();
    m_segments.pop_back();
    DEBUG("Deleting %s", old.c_str());
    unlink(old.c_str());
  }
  if (m_output_fd > 0)
  {
    // Flush any remaining packets.
    _flush_channel();
    close(m_output_fd);
  }

  int mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
  if ((m_output_fd = open(segment.c_str(), O_WRONLY | O_CREAT, mode)) < 0)
  {
    throw DvbException
    (
      fmt("Failed to create new segment %s : %s") % segment % strerror(errno)
    );
  }

  m_segments.push_front(segment);
  if (m_segments.size() == NUM_SEGMENTS)
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

void Channel::writePacket(uint8_t* buf)
{
  if (!m_enabled) return;

  try
  {
    if (_check_new_segment_required())
    {
      _create_new_segment();
    }

    memcpy(m_buf + m_buffer_len, buf, TS_PACKET_SIZE);
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

bool Channel::_check_new_segment_required()
{
  bool new_segment = 0;
  unsigned long delta = (m_curr_time.tv_sec - m_time.tv_sec) * 1000000 +
    (m_curr_time.tv_usec - m_time.tv_usec);
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
    remove(segment.c_str());
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
