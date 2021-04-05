#include "dpe/http_server.h"

#include <iostream>

#include <winsock2.h>
#include <process.h>

namespace http {
HttpServer::HttpServer()
    : port(0),
      quitFlag(0),
      threadHandle(NULL),
      taskEvent(NULL),
      handler(NULL),
      weakptr_factory_(this) {
  taskEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

HttpServer::~HttpServer() { CloseHandle(taskEvent); }

void HttpServer::Start(int port) {
  this->port = port;
  unsigned id = 0;
  threadHandle = (HANDLE)_beginthreadex(NULL, 0, &HttpServer::ThreadMain,
                                        (void*)this, 0, &id);
}

void HttpServer::Stop() {
  if (threadHandle == NULL) {
    return;
  }

  SOCKET client;
  struct sockaddr_in server;
  client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = inet_addr("127.0.0.1");

  quitFlag = 1;

  SetEvent(taskEvent);
  DWORD result = ::WaitForMultipleObjects(1, &threadHandle, FALSE, 0);

  if (result == WAIT_TIMEOUT) {
    connect(client, (struct sockaddr*)&server, sizeof(server));
    const std::string data = "GET quit HTTP/1.0\n\n";
    send(client, data.c_str(), data.size(), 0);
  }

  closesocket(client);

  result = ::WaitForMultipleObjects(1, &threadHandle, FALSE, 3000);

  if (result == WAIT_TIMEOUT) {
    ::TerminateThread(threadHandle, -1);
    ::WaitForMultipleObjects(1, &threadHandle, FALSE, 3000);
  } else if (result == WAIT_OBJECT_0) {
    //
  }
  ::CloseHandle(threadHandle);
  threadHandle = NULL;
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
    if (quitFlag) {
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
    LOG(INFO) << "Http response:\n" << data;
    send(c, data.c_str(), data.size(), 0);
    closesocket(c);
  }
  delete[] buff;
  closesocket(s);
  return 0;
}

bool HttpServer::HandleRequestOnThread(const std::string& reqData,
                                       std::string& repData) {
  LOG(INFO) << "Handle http request:\n" << reqData;
  auto req = ParseRequest(reqData);

  if (req.path == "quit") {
    return false;
  }

  HttpResponse rep;
  base::ThreadPool::PostTask(
      base::ThreadPool::UI, FROM_HERE,
      base::Bind(HandleRequest, weakptr_factory_.GetWeakPtr(), req, &rep));

  DWORD result = WaitForSingleObject(taskEvent, 10 * 1000);
  if (quitFlag) {
    return false;
  }
  if (result != WAIT_OBJECT_0) {
    return false;
  }

  repData = std::move(ResponseToString(rep));
  return true;
}

HttpRequest HttpServer::ParseRequest(const std::string& requestData) {
  std::vector<std::string> tokens;
  Tokenize(requestData, "\n", &tokens);
  HttpRequest req;
  const int size = static_cast<int>(tokens.size());
  if (size > 0) {
    std::vector<std::string> verb;
    Tokenize(tokens[0], " ", &verb);

    const int hs = static_cast<int>(verb.size());
    if (hs > 0) req.method = verb[0];
    if (hs > 1) {
      req.fullPath = verb[1];
      ParsePath(req.fullPath, req.path, req.parameters);
#if 0
      std::cerr << req.fullPath << std::endl;
      std::cerr << req.path << std::endl;
      for (auto& iter: req.parameters)
      {
        std::cerr << iter.first << " " << iter.second << std::endl;
      }
#endif
    }
    if (hs > 2) req.version = verb[2];
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
    req.headers[key] = value;
  }

  return req;
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

std::string HttpServer::ResponseToString(const HttpResponse& rep) {
  std::string result;
  result.append("HTTP/1.0 200 OK\n");
  result.append(
      base::StringPrintf("Content-Length: %u\n", rep.GetBody().size()));
  for (const auto& iter : rep.GetHeaders()) {
    result.append(base::StringPrintf("%s: %s\n", iter.first.c_str(),
                                     iter.second.c_str()));
  }
  result.append("\n");
  result.append(rep.GetBody());

  return result;
}

void HttpServer::HandleRequest(base::WeakPtr<HttpServer> self,
                               const HttpRequest& req, HttpResponse* rep) {
  if (auto* pThis = self.get()) {
    pThis->HandleRequestImpl(req, rep);
    SetEvent(pThis->taskEvent);
  }
}

void HttpServer::HandleRequestImpl(const HttpRequest& req, HttpResponse* rep) {
  if (handler) {
    handler->HandleRequest(req, rep);
  }
}
}  // namespace http