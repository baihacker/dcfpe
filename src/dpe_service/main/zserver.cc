#include "dpe_service/main/zserver.h"

#include <iostream>
using namespace std;

namespace ds
{
ZServer::ZServer() :
  server_state_(ZSERVER_IDLE),
  server_handle_(base::INVALID_CHANNEL_ID)
{

}

ZServer::~ZServer()
{
  Stop();
}

bool ZServer::Start(const std::string& address)
{
  if (server_state_ == ZSERVER_RUNNING) return address == server_address_;
  base::MessageCenter* mc = base::zmq_message_center();

  server_handle_ = mc->RegisterChannel(address, false, true);

  if (server_handle_ == base::INVALID_CHANNEL_ID) return false;
  mc->AddMessageHandler(this);

  server_address_ = address;
  server_state_ = ZSERVER_RUNNING;

  return false;
}

bool ZServer::Start(int32_t ip)
{
  base::MessageCenter* mc = base::zmq_message_center();
  return Start(mc->GetAddress(ip, kServerPort));
}

bool ZServer::Stop()
{
  if (server_state_ == ZSERVER_IDLE) return true;

  base::MessageCenter* mc = base::zmq_message_center();

  mc->RemoveMessageHandler(this);
  mc->RemoveChannel(server_handle_);

  server_handle_ = base::INVALID_CHANNEL_ID;
  std::string().swap(server_address_);
  server_state_ = ZSERVER_IDLE;

  return true;
}
/*
address: dpe:// physic address / instance address / network address
msg protocol:
{
  "type" : "ipc", # other value: "rsr" remote service request (rpc, rmi)
  "request" : "", # api name
  "src" : "",
  "dest" : "",
  "send_time" : "",
  "receive_time" : "",
  "back_time" : "", 
}

request = "CreateDPEDevice"
arguments = 
{
  "":"",
}

reply = "CreateDPEDeviceResponse"
response = 
{
  "" : ""
}

*/
int32_t ZServer::handle_message(int32_t handle, const char* msg, int32_t length)
{
  if (handle != server_handle_) return 0;

  const char* test_text = R"({"key":"value"})";
  cerr << test_text << endl;
  base::Value* v = base::JSONReader::Read(test_text);
  if (!v) return 1;
  
  base::DictionaryValue* dv = NULL;
  if (v->GetAsDictionary(&dv))
  {
    std::string val;
    if (dv->GetString("key", &val))
    {
      cerr << val << endl;
    }
  }
  return 1;
}
}