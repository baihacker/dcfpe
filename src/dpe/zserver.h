#ifndef DPE_ZSERVER_H_
#define DPE_ZSERVER_H_

#include "dpe_base/dpe_base.h"
#include "dpe/proto/dpe.pb.h"

namespace dpe {
struct ZServerHandler {
  virtual int HandleRequest(const Request& req, Response& reply) = 0;
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

  std::string GetServerAddress() { return server_address_; }

 private:
  bool Start(const std::string& address);
  std::string handle_request(base::ServerContext& context) override;

 public:
  int32_t server_state_;
  std::string server_address_;
  ZServerHandler* handler_;

  base::WeakPtrFactory<ZServer> weakptr_factory_;
};

}  // namespace dpe
#endif