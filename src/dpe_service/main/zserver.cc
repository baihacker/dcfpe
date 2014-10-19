#include "dpe_service/main/zserver.h"

#include <iostream>
using namespace std;

namespace ds
{
ZServer::ZServer() :
  server_state_(ZSERVER_IDLE)
{

}

ZServer::~ZServer()
{
  Stop();
}

bool ZServer::Start(const std::string& address)
{
  if (server_state_ == ZSERVER_RUNNING) return address == server_address_;
  auto s = base::zmq_server();

  if (!s->StartServer(address, this)) return false;

  server_address_ = address;
  server_state_ = ZSERVER_RUNNING;

  return true;
}

bool ZServer::Start(int32_t ip)
{
  return Start(base::AddressHelper::MakeZMQTCPAddress(ip, kServerPort));
}

bool ZServer::Stop()
{
  if (server_state_ == ZSERVER_IDLE) return true;

  auto s = base::zmq_server();

  s->StopServer(this);

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
std::string ZServer::handle_request(base::ServerContext& context)
{
  const char* test_text = R"({"key":"value"})";
  cerr << test_text << endl;
  base::Value* v = base::JSONReader::Read(test_text);
  if (!v) return "";
  
  base::DictionaryValue* dv = NULL;
  if (v->GetAsDictionary(&dv))
  {
    std::string val;
    if (dv->GetString("key", &val))
    {
      cerr << val << endl;
    }
  }
  return "";
}
}