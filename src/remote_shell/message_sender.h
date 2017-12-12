#ifndef REMOTE_SHELL_MESSAGE_SENDER_H_
#define REMOTE_SHELL_MESSAGE_SENDER_H_

#include "dpe_base/dpe_base.h"
#include "remote_shell/proto/rs.pb.h"

namespace rs {
typedef std::function<void (int32_t zmqError, const Response&)> MessageCallback;
class ZMQClientContext {
public:
  static base::ZMQClient* getClient() {return base::zmq_client();}
  static int getNextRequestId() { return ++nextRequestId;}
public:
  static int nextRequestId;
};

class MessageSender : public base::RefCounted<MessageSender> {
public:
  MessageSender(const std::string& address);
  ~MessageSender();
  int sendRequest(Request& req, int timeout);
  int sendRequest(Request& req, MessageCallback callback, int timeout);
  static void  handleResponse(base::WeakPtr<MessageSender> self,
            MessageCallback callback,
            scoped_refptr<base::ZMQResponse> rep);
protected:
  std::string address;
  base::WeakPtrFactory<MessageSender>                 weakptr_factory_;
};
}
#endif