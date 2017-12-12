#ifndef REMOTE_SHELL_LOCAL_SHELL_H_
#define REMOTE_SHELL_LOCAL_SHELL_H_

#include "remote_shell/local_server_node.h"

namespace rs {
class LocalShell : public LocalServerHost, public base::RefCounted<LocalShell> {
public:
  LocalShell();
  ~LocalShell();

  void start(const std::string& target);
  void executeCommand();
  
  void onConnectStatusChanged(int newStatus);
  void onCommandStatusChanged(int newStatus);
private:
  scoped_refptr<LocalServerNode> serverNode;
  base::WeakPtrFactory<LocalShell>                 weakptr_factory_;
};
}
#endif