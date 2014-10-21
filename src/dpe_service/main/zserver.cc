#include "dpe_service/main/zserver.h"

#include "dpe_service/main/dpe_service.h"

namespace ds
{
ZServer::ZServer(DPEService* dpe) :
  server_state_(ZSERVER_IDLE),
  dpe_(dpe),
  ip_(-1)
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
  ip_ = ip;
  return Start(base::AddressHelper::MakeZMQTCPAddress(ip, kServerPort));
}

bool ZServer::Stop()
{
  if (server_state_ == ZSERVER_IDLE) return true;

  auto s = base::zmq_server();

  s->StopServer(this);

  std::string().swap(server_address_);
  server_state_ = ZSERVER_IDLE;
  ip_ = -1;
  
  return true;
}
/*
address: dpe:// physic address / instance address / network address
msg protocol:
{
  "type" : "ipc", # other value: "rsc" remote service communicate (rpc, rmi)
  "request" : "", # request, reply, message
  "src" : "",
  "dest" : "",
  "send_time" : "",
  "receive_time" : "",
  "back_time" : "",
  "session" : "",
  "cookie" : "",
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
  LOG(INFO) << "\nrequest:\n" << context.data_;
  
  base::DictionaryValue rep;
  base::Value* v = base::JSONReader::Read(context.data_.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);
  rep.SetString("type", "rsc");
  rep.SetString("error_code", "-1");
  
  do
  {
    if (!v)
    {
      LOG(ERROR) << "\nCan not parse request";
      break;
    }
    
    base::DictionaryValue* dv = NULL;
    if (!v->GetAsDictionary(&dv)) break;
    {
      std::string val;
      
      if (!dv->GetString("type", &val)) break;
      if (val != "rsc") break;
      
      if (!dv->GetString("src", &val)) break;
      rep.SetString("dest", val);
      
      if (!dv->GetString("dest", &val)) break;
      rep.SetString("src", val);
      
      if (dv->GetString("cookie", &val))
      {
        rep.SetString("cookie", val);
      }
      
      if (!dv->GetString("request", &val)) break;
      
      if (val != "CreateDPEDevice") break;
      
      auto s = dpe_->CreateDPEDevice(this, dv);
      if (!s)
      {
        LOG(ERROR) << "can not create dpe device";
        break;
      }
      rep.SetString("reply", "CreateDPEDeviceResponse");
      rep.SetString("receive_address", s->GetReceiveAddress());
      rep.SetString("send_address", s->GetSendAddress());
      
      rep.SetString("error_code", "0");
    }
  } while (false);
  
  delete v;
  
  std::string ret;
  if (base::JSONWriter::Write(&rep, &ret))
  {
    LOG(INFO) << "\nreply:\n" << ret;
    return ret;
  }
  return "";
}
}