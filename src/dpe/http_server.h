#ifndef DPE_HTTP_SERVER_H_
#define DPE_HTTP_SERVER_H_

#include <map>
#include <unordered_map>

#include <windows.h>

#include "dpe_base/dpe_base.h"

namespace http
{
struct HttpRequest
{
std::string method;
std::string fullPath;
std::string path;
std::string version;
std::map<std::string, std::string> headers;
std::map<std::string, std::string> parameters;
};

class HttpResponse
{
public:
  void setHeader(const std::string& key, const std::string& value)
  {
    headers[key] = value;
  }
  void setBody(const std::string& body)
  {
    this->body = body;
  }
  const std::map<std::string, std::string>& getHeaders() const
  {
    return headers;
  }
  const std::string getBody() const
  {
    return body;
  }
private:
std::map<std::string, std::string> headers;
std::string body;
};

class HttpReqestHandler
{
public:
  virtual bool handleRequest(const HttpRequest& req, HttpResponse* rep) = 0;
};

class HttpServer
{
public:
  HttpServer();
  ~HttpServer();

  void start(int port);
  void stop();
  
  void setHandler(HttpReqestHandler* handler)
  {
    this->handler = handler;
  }
  
  static unsigned __stdcall threadMain(void * arg);
  unsigned      run();
  bool handleRequestOnThread(const std::string& reqData, std::string& repData);
  HttpRequest parseRequest(const std::string& requestData);
  void parsePath(const std::string& fullPath, std::string& path, std::map<std::string, std::string>& parameters);
  std::string responseToString(const HttpResponse& rep);
  
  static void handleRequest(base::WeakPtr<HttpServer> self, const HttpRequest& req,
    HttpResponse* rep);
  void handleRequestImpl(const HttpRequest& req, HttpResponse* rep);
private:
  int port;
  HANDLE taskEvent;
  HANDLE threadHandle;
  HttpReqestHandler* handler;
  base::WeakPtrFactory<HttpServer>                 weakptr_factory_;
};
}
#endif