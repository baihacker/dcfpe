#ifndef DPE_HTTP_SERVER_H_
#define DPE_HTTP_SERVER_H_

#include <map>
#include <unordered_map>

#include <windows.h>

#include "dpe_base/dpe_base.h"

namespace http {
struct HttpRequest {
  std::string method;
  std::string fullPath;
  std::string path;
  std::string version;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> parameters;
};

class HttpResponse {
 public:
  void SetHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
  }
  void SetBody(const std::string& body) { this->body = body; }
  const std::map<std::string, std::string>& GetHeaders() const {
    return headers;
  }
  const std::string GetBody() const { return body; }

 private:
  std::map<std::string, std::string> headers;
  std::string body;
};

class HttpReqestHandler {
 public:
  virtual bool HandleRequest(const HttpRequest& reqest,
                             HttpResponse* response) = 0;
};

class HttpServer {
 public:
  HttpServer();
  ~HttpServer();

  void Start(int port);
  void Stop();

  void SetHandler(HttpReqestHandler* handler) { this->handler = handler; }

  static unsigned __stdcall ThreadMain(void* arg);
  unsigned Run();
  bool HandleRequestOnThread(const std::string& request_data,
                             std::string& response_data);
  HttpRequest ParseRequest(const std::string& request_data);
  void ParsePath(const std::string& full_path, std::string& path,
                 std::map<std::string, std::string>& parameters);
  std::string ResponseToString(const HttpResponse& reponse);

  static void HandleRequest(base::WeakPtr<HttpServer> self,
                            const HttpRequest& request, HttpResponse* response);
  void HandleRequestImpl(const HttpRequest& request, HttpResponse* response);

 private:
  int port;
  volatile int quit_flag_;
  HANDLE task_event_;
  HANDLE thread_handle_;
  HttpReqestHandler* handler;
  base::WeakPtrFactory<HttpServer> weakptr_factory_;
};
}  // namespace http
#endif