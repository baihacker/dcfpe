#include "dpe_base/zmq_adapter.h"

#include <algorithm>

#include <process.h>

#include "dpe_base/thread_pool.h"

namespace base
{
int32_t AddressHelper::next_available_port_ = kMinPort;

std::string AddressHelper::MakeZMQTCPAddress(uint32_t ip, int32_t port)
{
  char address[64];
  sprintf(address, "tcp://%d.%d.%d.%d:%d",
      ip>>24, (ip>>16)&255, (ip>>8)&255, ip&255, port);
  return address;
}

std::string AddressHelper::MakeZMQTCPAddress(const std::string& ip, int32_t port)
{
  char address[64];
  sprintf(address, "tcp://%s:%d", ip.c_str(), port);
  return address;
}

std::string AddressHelper::FormatAddress(uint32_t ip, int32_t port)
{
  char address[64];
  sprintf(address, "%d.%d.%d.%d:%d",
      ip>>24, (ip>>16)&255, (ip>>8)&255, ip&255, port);
  return address;
}

int32_t AddressHelper::GetProcessIP()
{
  static const int32_t offset = ::GetCurrentProcessId() % 65535 + 1;
  static const int32_t basic_ip = MAKE_IP(127, 233, 0, 0) | offset;
  return basic_ip;
}

int32_t AddressHelper::GetCtrlIP()
{
  static const int32_t offset = ::GetCurrentProcessId() % 65535 + 1;
  static const int32_t basic_ip = MAKE_IP(127, 234, 0, 0) | offset;
  return basic_ip;
}

int32_t AddressHelper::GetNextAvailablePort()
{
  int32_t ret = next_available_port_;
  if (++next_available_port_ == kMaxPort)
  {
    next_available_port_ = kMinPort;
  }
  return ret;
}

}
