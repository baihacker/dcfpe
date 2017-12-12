#ifndef RS_ZSERVER_H_
#define RS_ZSERVER_H_

#include "dpe_base/dpe_base.h"
#include "remote_shell/proto/rs.pb.h"

namespace rs {
struct ZServerHandler {
  virtual void handleRequest(const Request& req, Response& reply) = 0;
  virtual bool preHandleRequest(const Request& req, Response& reply) {
    return false;
  }
};

class ZServer : public base::RequestHandler, public base::RefCounted<ZServer> {
public:
  enum {
    ZSERVER_IDLE,
    ZSERVER_RUNNING,
  };
public:
  ZServer(ZServerHandler* handler);
  ~ZServer();

  bool Start(uint32_t ip, int port);
  bool Start(const std::string& ip, int port);
  bool Stop();

  std::string GetServerAddress(){return server_address_;}

private:

  bool Start(const std::string& address);
  std::string handle_request(base::ServerContext& context) override;
  bool pre_handle_request(base::ServerContext& context, std::string& reply) override;

public:
  // remote message handling: bind and receive and send
  int32_t     server_state_;
  std::string server_address_;
  ZServerHandler* handler;

  base::WeakPtrFactory<ZServer>                 weakptr_factory_;
};

}
#endif