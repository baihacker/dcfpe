#include "dpe_service/main/zserver.h"

#include "dpe_service/main/dpe_service.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

namespace ds
{
ZServer::ZServer(DPEService* dpe) :
  server_state_(ZSERVER_IDLE),
  dpe_(dpe),
  ip_(-1),
  weakptr_factory_(this)
{
  start_event_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  hello_event_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  stop_event_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
}

ZServer::~ZServer()
{
  Stop();
  ::CloseHandle(start_event_);
  ::CloseHandle(hello_event_);
  ::CloseHandle(stop_event_);
}

bool ZServer::Start(const std::string& address)
{
  if (server_state_ == ZSERVER_RUNNING) return address == server_address_;
  auto s = base::zmq_server();
  if (!s->StartServer(address, this))
  {
    return false;
  }

  server_address_ = address;
  server_state_ = ZSERVER_RUNNING;

  ::ResetEvent(start_event_);
  ::ResetEvent(hello_event_);
  unsigned id = 0;
  thread_handle_ = (HANDLE)_beginthreadex(NULL, 0,
      &ZServer::ThreadMain, (void*)this, 0, &id);
  if (!thread_handle_)
  {
    return false;
  }
  
  HANDLE handles[] = {thread_handle_, start_event_};
  DWORD result = ::WaitForMultipleObjects(2, handles, FALSE, -1);
  if (result != WAIT_OBJECT_0 + 1)
  {
    if (result != WAIT_OBJECT_0)
    {
      ::TerminateThread(thread_handle_, -1);
    }
    ::CloseHandle(thread_handle_);
    thread_handle_ = NULL;
    return false;
  }
  Advertise();
  Search();
  return true;
}

bool ZServer::Start(uint32_t ip)
{
  ip_ = ip;
  return Start(base::AddressHelper::MakeZMQTCPAddress(ip, kServerPort));
}

bool ZServer::Stop()
{
  if (server_state_ == ZSERVER_IDLE) return true;

  // set the event first.
  // after advertising, the server thread will receive the message and check the event
  ::SetEvent(stop_event_);
  GoodBye();
  
  auto s = base::zmq_server();

  s->StopServer(this);

  std::string().swap(server_address_);
  server_state_ = ZSERVER_IDLE;
  ip_ = -1;
  
  DWORD result = ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);
  
  if (result == WAIT_TIMEOUT)
  {
    ::TerminateThread(thread_handle_, -1);
    ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);
  }
  else if (result == WAIT_OBJECT_0)
  {
    //
  }
  ::CloseHandle(thread_handle_);
  thread_handle_ = NULL;
  
  return true;
}

bool ZServer::Multicast(const std::string& text)
{
  SOCKET advertise_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (advertise_socket == INVALID_SOCKET)
  {
    LOG(INFO) << "Create socket failed";
    return false;
  }
  
  u_char ttl = 4;
  if (-1 == setsockopt(advertise_socket, IPPROTO_IP, IP_MULTICAST_TTL,
    (const char*)&ttl, sizeof(ttl)))
  {
    shutdown(advertise_socket, SD_BOTH);
    LOG(INFO) << "Set ttl failed";
    return false;
  }
  
  uint32_t src = htonl(ip_);
  u_long val = 1;
  ioctlsocket(advertise_socket, FIONBIO, &val);
  if (-1 == setsockopt(advertise_socket, IPPROTO_IP, IP_MULTICAST_IF, (char *)&src, sizeof(src)))
  {
    shutdown(advertise_socket, SD_BOTH);
    LOG(INFO) << "Set interface failed";
    return false;
  }
  
  struct sockaddr_storage __ss_v4;
  struct sockaddr_in *destAddr4 = (struct sockaddr_in *)&__ss_v4;

  memset(&__ss_v4, 0, sizeof(__ss_v4));
  destAddr4->sin_family = AF_INET;
  destAddr4->sin_addr.S_un.S_addr = htonl(base::AddressHelper::GetSSDPIP());
  destAddr4->sin_port = htons(base::AddressHelper::GetSSDPPort());
  
  sendto(advertise_socket, text.c_str(), static_cast<int>(text.length()), 0, (struct sockaddr *)destAddr4, sizeof(struct sockaddr_in));
  
  shutdown(advertise_socket, SD_BOTH);
  return true;
}

bool ZServer::Advertise()
{
  return Multicast(base::StringPrintf("action:advertise\r\naddress:%s\n", server_address_.c_str()));
}

bool ZServer::Search()
{
  return Multicast(base::StringPrintf("action:search\r\naddress:%s\n", server_address_.c_str()));
}

bool ZServer::GoodBye()
{
  return Multicast(base::StringPrintf("action:bye\r\naddress:%s\n", server_address_.c_str()));
}
/*
address: dpe:// physic address / instance address / network address
msg protocol:
{
  "type" : "ipc", # other value: "rsc" remote service communicate (rpc, rmi)
  "request" : "", # request, reply, message
  
  "pa" : "",
  "src" : "",
  "dest" : "",
  "session" : "", #optional
  "cookie" : "",  #optional
  "ts" : "",
  "ots" : "",
}
*/
std::string ZServer::handle_request(base::ServerContext& context)
{
  LOG(INFO) << "\nZServer receives request:\n" << context.data_;
  
  base::DictionaryValue rep;
  rep.SetString("type", "rsc");
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
    if (val != "rsc") break;

    // swap the src and dest and send them back
    if (!dv->GetString("src", &val)) break;
    rep.SetString("dest", val);
    
    if (!dv->GetString("dest", &val)) break;
    rep.SetString("src", val);
    
    // send cookie back
    if (dv->GetString("cookie", &val))
    {
      rep.SetString("cookie", val);
    }
    
    // original time stamp
    if (dv->GetString("ts", &val))
    {
      rep.SetString("ots", val);
    }

    if (!dv->GetString("request", &val)) break;

    // handle request
    if (val == "CreateDPEDevice")
    {
      HandleCreateDPEDeviceRequest(dv, &rep);
    }
    else if (val == "Hello")
    {
      HandleHelloRequest(dv, &rep);
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
  reply->SetString("reply", "CreateDPEDeviceResponse");
  
  auto s = dpe_->CreateDPEDevice(this, req);
  if (!s)
  {
    LOG(ERROR) << "can not create dpe device";
    reply->SetString("error_code", "-1");
    return;
  }

  // the information of DPEDevice
  reply->SetString("receive_address", s->GetReceiveAddress());
  reply->SetString("send_address", s->GetSendAddress());
  reply->SetString("session", s->GetSession());
  
  reply->SetString("error_code", "0");
}

void ZServer::HandleHelloRequest(
        base::DictionaryValue* req, base::DictionaryValue* reply)
{
  reply->SetString("reply", "HelloResponse");
  reply->SetString("error_code", "0");
}

unsigned __stdcall ZServer::ThreadMain(void * arg)
{
  if (ZServer* pThis = (ZServer*)arg)
  {
    pThis->Run();
  }
  return 0;
}

void ZServer::Run()
{
  SOCKET sniffer_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (sniffer_socket == INVALID_SOCKET)
  {
    shutdown(sniffer_socket, SD_BOTH);
    return;
  }

  // reuse address
  int onOff = 1;
  int ret = setsockopt(sniffer_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&onOff, sizeof(onOff));
  if (ret == -1)
  {
    shutdown(sniffer_socket, SD_BOTH);
    return;
  }

  // bind local address
  struct sockaddr_storage __ss;
  struct sockaddr_in *ssdpAddr4 = (struct sockaddr_in *)&__ss;

  memset(&__ss, 0, sizeof(__ss));
  ssdpAddr4->sin_family = AF_INET;
  ssdpAddr4->sin_addr.s_addr = htonl(INADDR_ANY);
  ssdpAddr4->sin_port = htons(base::AddressHelper::GetSSDPPort());
  ret = bind(sniffer_socket, (struct sockaddr *)ssdpAddr4, sizeof(*ssdpAddr4));
  if (ret == -1)
  {
    shutdown(sniffer_socket, SD_BOTH);
    return;
  }

  // add multicast group
  struct ip_mreq ssdpMcastAddr;
  memset((void *)&ssdpMcastAddr, 0, sizeof(struct ip_mreq));
  ssdpMcastAddr.imr_interface.s_addr = htonl(ip_);
  ssdpMcastAddr.imr_multiaddr.s_addr = htonl(base::AddressHelper::GetSSDPIP());
  ret = setsockopt(sniffer_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&ssdpMcastAddr, sizeof(struct ip_mreq));
  if (ret == -1)
  {
    shutdown(sniffer_socket, SD_BOTH);
    return;
  }

  // select interface
  struct in_addr addr;
  memset((void *)&addr, 0, sizeof(struct in_addr));
  addr.s_addr = htonl(ip_);
  ret = setsockopt(sniffer_socket, IPPROTO_IP, IP_MULTICAST_IF, (char *)&addr, sizeof addr);
  if (ret == -1)
  {
    shutdown(sniffer_socket, SD_BOTH);
    return;
  }

  // set ttl
  u_char ttl = (u_char)4;
  setsockopt(sniffer_socket, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
  onOff = 1;
  
  // allow broadcast
  ret = setsockopt(sniffer_socket, SOL_SOCKET, SO_BROADCAST, (char *)&onOff, sizeof(onOff));
  if (ret == -1)
  {
    shutdown(sniffer_socket, SD_BOTH);
    return;
  }
  
  base::WeakPtr<ZServer> pThis = weakptr_factory_.GetWeakPtr();
  ::SetEvent(start_event_);
  for (;WaitForSingleObject(stop_event_, 0) == WAIT_TIMEOUT;)
  {
    fd_set rdSet;
    FD_ZERO(&rdSet);
    FD_SET(sniffer_socket, &rdSet);
    timeval timeout = {0};
    timeout.tv_sec = 100;

    int ret = select(sniffer_socket + 1, &rdSet, NULL, NULL, &timeout);
    if (ret == SOCKET_ERROR)
    {
      continue;
    }
    static const int buffer_size = 1024 * 64;
    char ssdp_buffer[buffer_size];
    if (FD_ISSET(sniffer_socket, &rdSet))
    {
      struct sockaddr_storage __ss;
      socklen_t socklen = sizeof(__ss);
      int byteReceived = recvfrom(sniffer_socket, ssdp_buffer, buffer_size - 1, 0, (struct sockaddr *)&__ss, &socklen);
      if (byteReceived > 0)
      {
        ssdp_buffer[byteReceived] = 0;
        /*LOG(INFO) << "Sniffer result:\r\n";
        LOG(INFO) << "src : " << base::AddressHelper::FormatAddress(
            ntohl(((struct sockaddr_in *)&__ss)->sin_addr.s_addr),
            ntohs(((struct sockaddr_in *)&__ss)->sin_port));
        LOG(INFO) << ssdp_buffer;*/
        
        std::string data(ssdp_buffer, ssdp_buffer+byteReceived);
        uint32_t ip = ntohl(((struct sockaddr_in *)&__ss)->sin_addr.s_addr);
        int32_t port = ntohs(((struct sockaddr_in *)&__ss)->sin_port);
        base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
          base::Bind(&ZServer::HandleMulticast, pThis/*weakptr_factory_.GetWeakPtr()*/, ip, port, data)
            );
        /*
        SnifferResult* result = new SnifferResult();
        result->data.append(ssdp_buffer, byteReceived + 1);
        memcpy(&result->src, &__ss, sizeof(__ss));
        result->src_ip = inet_ntoa(((struct sockaddr_in *)&__ss)->sin_addr);
        result->port = ((struct sockaddr_in *)&__ss)->sin_port;
        result->is_search_result = 0;
        ::PostMessage(cookie.callback_wnd, SNIFFER_CALLBACK, (WPARAM)result, NULL);
        */
      }
    }
  }
  shutdown(sniffer_socket, SD_BOTH);
}

void  ZServer::HandleMulticast(base::WeakPtr<ZServer> self, uint32_t ip, int32_t port, const std::string& data)
{
  if (auto* pThis = self.get()) pThis->HandleMulticastImpl(ip, port, data);
}

void  ZServer::HandleMulticastImpl(uint32_t ip, int32_t port, const std::string& data)
{
  LOG(INFO) << "receive " << data;
  dpe_->HandleMulticast(ip_, ip, port, data);
}

}