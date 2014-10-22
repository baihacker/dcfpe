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
  LOG(INFO) << "\nZServer receives request:\n" << context.data_;
  
  base::DictionaryValue rep;
  rep.SetString("type", "rsc");
  rep.SetString("error_code", "-1");
  
  base::Value* v = base::JSONReader::Read(
          context.data_.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);

  do
  {
    if (!v)
    {
      LOG(ERROR) << "\n ZServer : can not parse request";
      break;
    }

    base::DictionaryValue* dv = NULL;
    if (!v->GetAsDictionary(&dv)) break;

    std::string val;

    if (!dv->GetString("type", &val)) break;
    if (val != "rsc") break;

    // swap the src and dest and send them back
    if (!dv->GetString("src", &val)) break;
    rep.SetString("dest", val);
    
    if (!dv->GetString("dest", &val)) break;
    rep.SetString("src", val);
    
    // we give cookie back
    if (dv->GetString("cookie", &val))
    {
      rep.SetString("cookie", val);
    }
    
    rep.SetString("receive_time",
      base::StringPrintf("%lld", base::Time::Now().ToInternalValue())
      );

    if (!dv->GetString("request", &val)) break;

    // handle request
    if (val == "CreateDPEDevice")
    {
      HandleCreateDPEDeviceRequest(dv, &rep);
    }
  } while (false);
  
  delete v;
  
  std::string ret;
  if (base::JSONWriter::Write(&rep, &ret))
  {
    LOG(INFO) << "\nZServer reply:\n" << ret;
    return ret;
  }
  return "";
}

void ZServer::HandleCreateDPEDeviceRequest(
      base::DictionaryValue* req, base::DictionaryValue* reply)
{
  auto s = dpe_->CreateDPEDevice(this, req);
  if (!s)
  {
    LOG(ERROR) << "can not create dpe device";
    reply->SetString("error_code", "-1");
    return;
  }
 
  reply->SetString("reply", "CreateDPEDeviceResponse");
  
  // the information of DPEDevice
  reply->SetString("receive_address", s->GetReceiveAddress());
  reply->SetString("send_address", s->GetSendAddress());
  reply->SetString("session", s->GetSession());
  reply->SetString("error_code", "0");
}

}