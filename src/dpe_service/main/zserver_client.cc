#include "dpe_service\main\zserver_client.h"

namespace ds
{

ZServerClient::ZServerClient(const std::string& address) :
  server_address_(address)
{
}

ZServerClient::~ZServerClient()
{
}

bool  ZServerClient::CreateDPEDevice(base::ZMQCallBack callback)
{
  std::string request;
  {
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    AddPaAndTs(&req);
    req.SetString("request", "CreateDPEDevice");
    req.SetString("src", "");
    req.SetString("dest", server_address_);
    if (!base::JSONWriter::Write(&req, &request))
    {
      LOG(ERROR) << "can not write request json";
      return false;
    }
  }
  
  auto zc = base::zmq_client();
  
  zc->SendRequest(
      server_address_,
      request.c_str(), static_cast<int>(request.size()),
      callback,
      20*1000
    );
  
  return true;
}

bool  ZServerClient::SayHello(base::ZMQCallBack callback)
{
  std::string request;
  {
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    AddPaAndTs(&req);
    req.SetString("request", "Hello");
    req.SetString("src", "");
    req.SetString("dest", server_address_);
    if (!base::JSONWriter::Write(&req, &request))
    {
      LOG(ERROR) << "can not write request json";
      return false;
    }
  }
  
  auto zc = base::zmq_client();

  zc->SendRequest(
      server_address_,
      request.c_str(), static_cast<int>(request.size()),
      callback,
      20*1000
    );
  
  return true;
}

}