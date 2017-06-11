#include "dpe/zserver.h"

namespace dpe
{
ZServer::ZServer(ZServerHandler* handler) :
  handler(handler),
  server_state_(ZSERVER_IDLE),
  weakptr_factory_(this)
{
}

ZServer::~ZServer()
{
  Stop();
}

bool ZServer::Start(const std::string& address)
{
  if (server_state_ == ZSERVER_RUNNING)
  {
    return address == server_address_;
  }
  auto s = base::zmq_server();
  if (!s->StartServer(address, this))
  {
    return false;
  }

  server_address_ = address;
  server_state_ = ZSERVER_RUNNING;

  return true;
}

bool ZServer::Start(uint32_t ip, int port)
{
  return Start(base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

bool ZServer::Start(const std::string& ip, int port)
{
  return Start(base::AddressHelper::MakeZMQTCPAddress(ip, port));
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
  "type" : "", # request, reply
  "action": "",
  "error_code": "",
  "cookie": "",
}
*/
std::string ZServer::handle_request(base::ServerContext& context)
{
  LOG(INFO) << "\nZServer receives request:\n" << context.data_;
  
  base::DictionaryValue rep;
  rep.SetString("type", "reply");
  rep.SetString("action", "reply");
  rep.SetString("error_code", "-1");
  base::AddPaAndTs(&rep);

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
    if (val != "request") break;

    // send cookie back
    if (dv->GetString("cookie", &val))
    {
      rep.SetString("cookie", val);
    }
    
    // send request id back
    if (dv->GetString("request_id", &val))
    {
      rep.SetString("request_id", val);
    }

    if (!dv->GetString("action", &val)) break;

    // handle request
    rep.SetString("action", val);
    if (val == "connect")
    {
      HandleConnectRequest(dv, &rep);
    }
    else if (val == "disconnect")
    {
      HandleDisconnectRequest(dv, &rep);
    }
    else
    {
      handler->handleRequest(dv, &rep);
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

void ZServer::HandleConnectRequest(
     base::DictionaryValue* req, base::DictionaryValue* reply)
{
  std::string val;
  if (req->GetString("address", &val))
  {
    int connectionId = handler->handleConnectRequest(val);
    if (connectionId > 0)
    {
      reply->SetString("connection_id", base::StringPrintf("%d", connectionId));
      reply->SetString("error_code", "0");
      return;
    }
  }
  reply->SetString("error_code", "-1");
}

void ZServer::HandleDisconnectRequest(
     base::DictionaryValue* req, base::DictionaryValue* reply)
{
  std::string val;
  if (req->GetString("address", &val))
  {
    handler->handleDisconnectRequest(val);
  }
  reply->SetString("error_code", "0");
}
}
