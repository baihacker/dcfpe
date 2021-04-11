#include "dpe/http_server.h"

#include <iostream>

#include <winsock2.h>
#include <process.h>

namespace http {
HttpServer::HttpServer()
    : port(0),
      quit_flag_(0),
      thread_handle_(NULL),
      task_event_(NULL),
      handler(NULL),
      weakptr_factory_(this) {
  task_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
}

HttpServer::~HttpServer() { CloseHandle(task_event_); }

void HttpServer::Start(int port) {
  this->port = port;
  unsigned id = 0;
  thread_handle_ = (HANDLE)_beginthreadex(NULL, 0, &HttpServer::ThreadMain,
                                          (void*)this, 0, &id);
}

void HttpServer::Stop() {
  if (thread_handle_ == NULL) {
    return;
  }

  SOCKET client;
  struct sockaddr_in server;
  client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = inet_addr("127.0.0.1");

  quit_flag_ = 1;

  SetEvent(task_event_);
  DWORD result = ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 0);

  if (result == WAIT_TIMEOUT) {
    connect(client, (struct sockaddr*)&server, sizeof(server));
    const std::string data = "GET quit HTTP/1.0\n\n";
    send(client, data.c_str(), data.size(), 0);
  }

  closesocket(client);

  result = ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);

  if (result == WAIT_TIMEOUT) {
    ::TerminateThread(thread_handle_, -1);
    ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);
  } else if (result == WAIT_OBJECT_0) {
    //
  }
  ::CloseHandle(thread_handle_);
  thread_handle_ = NULL;
}

unsigned __stdcall HttpServer::ThreadMain(void* arg) {
  if (HttpServer* pThis = (HttpServer*)arg) {
    pThis->Run();
  }
  return 0;
}

unsigned HttpServer::Run() {
  SOCKET s;
  struct sockaddr_in tcpaddr;
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  tcpaddr.sin_family = AF_INET;
  tcpaddr.sin_port = htons(port);
  tcpaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  bind(s, (SOCKADDR*)&tcpaddr, sizeof(tcpaddr));

  listen(s, 5);

  const int buffLen = 1024 * 1024;
  char* buff = new char[buffLen];
  for (;;) {
    struct sockaddr addr;
    int addrLen = sizeof(addr);
    SOCKET c = accept(s, &addr, &addrLen);
    if (quit_flag_) {
      closesocket(c);
      break;
    }
    int len = recv(c, buff, buffLen, 0);
    if (len == SOCKET_ERROR) {
      closesocket(c);
      continue;
    }
    buff[len] = 0;
    std::string data;
    if (!HandleRequestOnThread(buff, data)) {
      closesocket(c);
      break;
    }
    VLOG(1) << "Http response:\n" << data;
    send(c, data.c_str(), data.size(), 0);
    closesocket(c);
  }
  delete[] buff;
  closesocket(s);
  return 0;
}

bool HttpServer::HandleRequestOnThread(const std::string& reqData,
                                       std::string& repData) {
  VLOG(1) << "Handle http request:\n" << reqData;
  auto request = ParseRequest(reqData);

  if (request.path == "quit") {
    return false;
  }

  HttpResponse response;
  base::ThreadPool::PostTask(
      base::ThreadPool::UI, FROM_HERE,
      base::Bind(HandleRequest, weakptr_factory_.GetWeakPtr(), request,
                 &response));

  DWORD result = WaitForSingleObject(task_event_, 10 * 1000);
  if (quit_flag_) {
    return false;
  }
  if (result != WAIT_OBJECT_0) {
    return false;
  }

  repData = std::move(ResponseToString(response));
  return true;
}

HttpRequest HttpServer::ParseRequest(const std::string& requestData) {
  std::vector<std::string> tokens;
  Tokenize(requestData, "\n", &tokens);
  HttpRequest request;
  const int size = static_cast<int>(tokens.size());
  if (size > 0) {
    std::vector<std::string> verb;
    Tokenize(tokens[0], " ", &verb);

    const int hs = static_cast<int>(verb.size());
    if (hs > 0) request.method = verb[0];
    if (hs > 1) {
      request.fullPath = verb[1];
      ParsePath(request.fullPath, request.path, request.parameters);
#if 0
      std::cerr << request.fullPath << std::endl;
      std::cerr << request.path << std::endl;
      for (auto& iter: request.parameters)
      {
        std::cerr << iter.first << " " << iter.second << std::endl;
      }
#endif
    }
    if (hs > 2) request.version = verb[2];
  }
  for (int i = 1; i < size; ++i) {
    std::string key;
    std::string value;

    const std::string header = tokens[i];
    const int len = static_cast<int>(header.size());
    int idx = 0;
    while (idx < len && header[idx] != ':') ++idx;

    key = header.substr(0, idx);
    if (idx < len) {
      ++idx;
      while (idx < len && header[idx] == ' ') ++idx;
      value = header.substr(idx);
    }
    request.headers[key] = value;
  }

  return request;
}

void HttpServer::ParsePath(const std::string& fullPath, std::string& path,
                           std::map<std::string, std::string>& parameters) {
  const int len = static_cast<int>(fullPath.size());
  auto i = std::find(fullPath.begin(), fullPath.end(), '?');
  if (i == fullPath.end()) {
    path = fullPath;
    return;
  }
  path = fullPath.substr(0, i - fullPath.begin());

  std::vector<std::string> tokens;
  Tokenize(fullPath.substr(i + 1 - fullPath.begin()), "&", &tokens);

  for (const auto& iter : tokens) {
    auto i = std::find(iter.begin(), iter.end(), '=');
    if (i != iter.end()) {
      parameters[iter.substr(0, i - iter.begin())] =
          iter.substr(i + 1 - iter.begin());
    } else {
      parameters[iter] = "";
    }
  }
}

std::string HttpServer::ResponseToString(const HttpResponse& response) {
  std::string result;
  result.append("HTTP/1.0 200 OK\n");
  result.append(
      base::StringPrintf("Content-Length: %u\n", response.GetBody().size()));
  for (const auto& iter : response.GetHeaders()) {
    result.append(base::StringPrintf("%s: %s\n", iter.first.c_str(),
                                     iter.second.c_str()));
  }
  result.append("\n");
  result.append(response.GetBody());

  return result;
}

void HttpServer::HandleRequest(base::WeakPtr<HttpServer> self,
                               const HttpRequest& request,
                               HttpResponse* response) {
  if (auto* pThis = self.get()) {
    pThis->HandleRequestImpl(request, response);
    SetEvent(pThis->task_event_);
  }
}

void HttpServer::HandleRequestImpl(const HttpRequest& request,
                                   HttpResponse* response) {
  if (handler) {
    handler->HandleRequest(request, response);
  }
}
}  // namespace http