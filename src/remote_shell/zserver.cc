#include "remote_shell/zserver.h"

namespace rs {
ZServer::ZServer(ZServerHandler* handler) :
  handler(handler),
  server_state_(ZSERVER_IDLE),
  weakptr_factory_(this) {
}

ZServer::~ZServer() {
  Stop();
}

bool ZServer::Start(const std::string& address) {
  if (server_state_ == ZSERVER_RUNNING) {
    return address == server_address_;
  }
  auto s = base::zmq_server();
  if (!s->StartServer(address, this)) {
    return false;
  }

  server_address_ = address;
  server_state_ = ZSERVER_RUNNING;

  return true;
}

bool ZServer::Start(uint32_t ip, int port) {
  return Start(base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

bool ZServer::Start(const std::string& ip, int port) {
  return Start(base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

bool ZServer::Stop() {
  if (server_state_ == ZSERVER_IDLE) return true;

  auto s = base::zmq_server();

  s->StopServer(this);

  std::string().swap(server_address_);
  server_state_ = ZSERVER_IDLE;
  return true;
}

std::string ZServer::handle_request(base::ServerContext& context) {
  LOG(INFO) << "Has request";
  Request req;
  req.ParseFromString(context.data_);

  LOG(INFO) << "\nZServer receives request:\n" << req.DebugString();

  Response rep;
  do {
    rep.set_error_code(-1);
    rep.set_name(req.name());
    rep.set_timestamp(base::Time::Now().ToInternalValue());
    rep.set_request_id(req.request_id());
    rep.set_request_timestamp(req.timestamp());

    // handle request
    handler->handleRequest(req, rep);
  } while (false);

  std::string ret;
  rep.SerializeToString(&ret);

  LOG(INFO) << "reply :\n" << rep.DebugString();
  return ret;
}

bool ZServer::pre_handle_request(base::ServerContext& context, std::string& reply) {
  Request req;
  req.ParseFromString(context.data_);

  Response rep;
  rep.set_error_code(-1);
  rep.set_name(req.name());
  rep.set_timestamp(base::Time::Now().ToInternalValue());
  rep.set_request_id(req.request_id());
  rep.set_request_timestamp(req.timestamp());

  // handle request
  if (handler->preHandleRequest(req, rep)) {

    LOG(INFO) << "Has request (prehandle stage)";
    LOG(INFO) << "\nZServer receives request:\n" << req.DebugString();

    std::string ret;
    rep.SerializeToString(&ret);

    LOG(INFO) << "reply :\n" << rep.DebugString();
    return true;
  } else {
    return false;
  }
}
}
