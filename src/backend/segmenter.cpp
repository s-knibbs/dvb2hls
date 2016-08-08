#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <string.h>
#include "segmenter.hpp"
#include <dvbpsi/psi.h>
#include <dvbpsi/dr_48.h>
#include <fstream>
#include <unistd.h>

#include "dvb.hpp"
#include "channel.hpp"
#include "util.hpp"
#include "log.hpp"

Segmenter::Segmenter(DvbDevice &device) :
    m_dvbpsi_pat(0),
    m_dvbpsi_sdt(0),
    m_device(device),
    m_have_pat(0),
    m_have_sdt(0),
    m_channel_pids(),
    m_channel_ids(),
    m_tsid(0),
    m_quit(0)
{
}

void Segmenter::_process_pat(void* self, dvbpsi_pat_t* pat)
{
  Segmenter* ths = static_cast<Segmenter*>(self);
  dvbpsi_pat_program_t* program = pat->p_first_program;
  ths->m_tsid = pat->i_ts_id;

  while (program)
  {
    if (program->i_pid != 16)
    {
      Channel* chan = new Channel(program->i_number);
      ths->m_channel_ids[program->i_number] = chan;
      ths->m_channel_pids[program->i_pid] = chan;
    }
    program = program->p_next;
  }
  ths->m_have_pat = 1;
  DEBUG("Decoded PAT");
  dvbpsi_pat_delete(pat);
}

void Segmenter::_attach_sdt(dvbpsi_t *dvbpsi, uint8_t table_id, uint16_t extension, void* self)
{
  if (table_id == 0x42)
  {
    if (dvbpsi_sdt_attach(dvbpsi, table_id, extension, &_process_sdt, self) < 0)
    {
      throw DvbException("Failed to decode SDT.");
    }
  }
}

void Segmenter::_process_sdt(void* self, dvbpsi_sdt_t* sdt)
{
  Segmenter* ths = static_cast<Segmenter*>(self);
  dvbpsi_sdt_service_t* service = sdt->p_first_service;
  while(service)
  {
    Channel* chan = ths->m_channel_ids[service->i_service_id];
    dvbpsi_descriptor_t* descriptor = service->p_first_descriptor;
    while(descriptor)
    {
      if (descriptor->i_tag == 0x48)
      {
        dvbpsi_service_dr_t* service_dr = dvbpsi_DecodeServiceDr(descriptor);
        // TODO - Handle character encodings properly here.
        std::string name
        (
            reinterpret_cast<char*>(&service_dr->i_service_name[0]),
            service_dr->i_service_name_length
        );
        chan->setName(name);
        DEBUG("Found channel: %s", name.c_str());
        break;
      }
      descriptor = descriptor->p_next;
    }
    service = service->p_next;
  }
  ths->m_have_sdt = 1;
  dvbpsi_sdt_detach(ths->m_dvbpsi_sdt, 0x42, ths->m_tsid);
}

void Segmenter::scan()
{
  m_dvbpsi_pat = dvbpsi_new(handle_dvbpsi_message, DVBPSI_MESSAGE_LEVEL);

  if (m_dvbpsi_pat == NULL)
    throw DvbException("Failed to decode PAT.");

  if (!dvbpsi_pat_attach(m_dvbpsi_pat, &_process_pat, this))
    throw DvbException("Failed to decode PAT.");

  uint8_t buf[TS_PACKET_SIZE];

  while (m_device.read_card(buf, TS_PACKET_SIZE) == 1)
  {
    if (m_quit) return;
    if (GET_PID(buf) == 0x0)
    {
      DEBUG("Decoding PAT");
      dvbpsi_packet_push(m_dvbpsi_pat, buf);
      if (m_have_pat)
      {
        dvbpsi_pat_detach(m_dvbpsi_pat);
        dvbpsi_delete(m_dvbpsi_pat);
        m_dvbpsi_pat = NULL;
        break;
      }
    }
  }

  m_dvbpsi_sdt = dvbpsi_new(handle_dvbpsi_message, DVBPSI_MESSAGE_LEVEL);
  if (m_dvbpsi_sdt == NULL)
    throw DvbException("Failed to decode SDT.");
  if (!dvbpsi_AttachDemux(m_dvbpsi_sdt, &_attach_sdt, this))
    throw DvbException("Failed to decode SDT.");

  int chan_count = m_channel_pids.size();
  for (auto pair : m_channel_pids)
  {
    pair.second->startPmtScan(handle_dvbpsi_message);
  }
  while (m_device.read_card(buf, TS_PACKET_SIZE) == 1)
  {
    if (m_quit) return;
    uint16_t pid = GET_PID(buf);
    if (m_channel_pids.count(pid))
    {
      Channel* chan = m_channel_pids[pid];
      std::vector<int>* pids = chan->readPmt(buf);
      if (pids)
      {
        for (int pid : *pids)
        {
          m_channel_pids[pid] = chan;
        }
        chan_count--;
      }
    }
    else if (pid == 0x11)
    {
      DEBUG("Processing SDT pkt");
      dvbpsi_packet_push(m_dvbpsi_sdt, buf);
    }
    // Decoded all PMTs and decoded the SDT
    if (chan_count == 0 && m_have_sdt)
    {
      dvbpsi_DetachDemux(m_dvbpsi_sdt);
      dvbpsi_delete(m_dvbpsi_sdt);
      m_dvbpsi_sdt = 0;
      break;
    }
  }
  INFO("Found %d channels", m_channel_ids.size());
}

void Segmenter::_write_channel_index()
{
  std::string fname(m_device.get_multiplex() + ".csv");
  std::ofstream chan_index(fname.c_str());
  for (auto& item : m_channel_ids)
  {
    Channel* chan = item.second;
    if (chan->enabled())
    {
      chan_index << chan->getName() << ',' << chan->index_file() << std::endl;
    }
  }
}

void Segmenter::run()
{
  const uint8_t num_packets = 20;
  uint8_t buf[TS_PACKET_SIZE * num_packets];
  uint8_t* pkt;
  uint16_t pid;
  size_t pkts;
  bool required_pids[MAX_REQUIRED_PID + 1] = { 0 };
  required_pids[0] = 1; // PAT
  required_pids[1] = 1; // CAT
  required_pids[16] = 1; // NIT
  required_pids[17] = 1; // SDT
  required_pids[18] = 1; // EIT
  required_pids[20] = 1; // TDT

  if (access(OUT_DIR, F_OK) < 0)
  {
    if (mkdir(OUT_DIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0)
    {
      throw DvbException
      (
        fmt("Failed to create output directory %s : %s") % OUT_DIR % strerror(errno)
      );
    }
  }
  chdir(OUT_DIR);

  _write_channel_index();

  while (!m_quit)
  {
    Channel::set_curr_time();
    pkts = m_device.read_card(buf, TS_PACKET_SIZE * num_packets);
    for (size_t i = 0; i < pkts; i++)
    {
      pkt = &buf[i*TS_PACKET_SIZE];
      pid = GET_PID(pkt);
      if (pid <= MAX_REQUIRED_PID && required_pids[pid])
      {
        // Write packet to all channels
        for (auto& item : m_channel_ids)
        {
          item.second->writePacket(pkt, pid);
        }
      }
      else
      {
        //uint16_t id = 0;
        if (pid != 0x1FFF) // Null pid
        {
          auto it = m_channel_pids.find(pid);
          if (it != m_channel_pids.end())
          {
            it->second->writePacket(pkt, pid);
            //id = it->second->id();
          }
        }
      }
    }
  }
}

Segmenter::~Segmenter()
{
  // Remove index file.
  remove((m_device.get_multiplex() + ".csv").c_str());
  for (auto& item : m_channel_ids)
  {
    delete item.second;
  }
  if (m_dvbpsi_sdt)
  {
    dvbpsi_DetachDemux(m_dvbpsi_sdt);
    dvbpsi_delete(m_dvbpsi_sdt);
    m_dvbpsi_sdt = 0;
  }
  if (m_dvbpsi_pat)
  {
    dvbpsi_pat_detach(m_dvbpsi_pat);
    dvbpsi_delete(m_dvbpsi_pat);
    m_dvbpsi_pat = 0;
  }
}
