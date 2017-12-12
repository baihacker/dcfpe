#include "remote_shell/message_sender.h"

namespace rs {
int ZMQClientContext::nextRequestId = 0;

MessageSender::MessageSender(const std::string& address): address(address), weakptr_factory_(this) {
}

MessageSender::~MessageSender() {
}

static void NullCallback(int32_t zmqError, const Response& rep) {
}

int MessageSender::sendRequest(Request& req, int timeout) {
  return sendRequest(req, NullCallback, timeout);
}

int MessageSender::sendRequest(Request& req, MessageCallback callback, int timeout) {
  const auto requestId = ZMQClientContext::getNextRequestId();
  req.set_request_id(requestId);
  req.set_timestamp(base::Time::Now().ToInternalValue());

  std::string val;
  req.SerializeToString(&val);

  ZMQClientContext::getClient()->SendRequest(
    address,
    val.c_str(),
    static_cast<int>(val.size()),
    base::Bind(&MessageSender::handleResponse, weakptr_factory_.GetWeakPtr(), callback),
    timeout);

  return requestId;
}

void  MessageSender::handleResponse(base::WeakPtr<MessageSender> self,
            MessageCallback callback,
            scoped_refptr<base::ZMQResponse> rep) {
  if (auto* sender = self.get()) {
    Response body;
    body.ParseFromString(rep->data_);
    callback(rep->error_code_, body);
  }
}
}
