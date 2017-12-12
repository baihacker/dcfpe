#ifndef REMOTE_SHELL_BATCH_EXECUTE_SHELL_H_
#define REMOTE_SHELL_BATCH_EXECUTE_SHELL_H_

#include "remote_shell/local_server_node.h"

namespace rs {
struct BatchExecuteShellHost {
  virtual ~BatchExecuteShellHost() {}
  virtual void onStop(bool succeed) = 0;
};

class BatchExecuteShell : public LocalServerHost, public base::RefCounted<BatchExecuteShell> {
public:
  BatchExecuteShell(BatchExecuteShellHost* host);
  ~BatchExecuteShell();

  void start(const std::string& target, const std::vector<std::string>& cmdLines);

  void onConnectStatusChanged(int newStatus);
  void onCommandStatusChanged(int newStatus);

  void willStop(bool succeed);
  static void stop(base::WeakPtr<BatchExecuteShell> pThis, bool succeed);
  void stopImpl(bool succeed);

  void willRunNextCommand();
  static void runNextCommand(base::WeakPtr<BatchExecuteShell> pThis);
  void runNextCommandImpl();

  BatchExecuteShell& setShowPrompt(bool showPrompt) {
    this->showPrompt = showPrompt;
    return *this;
  }

  void outputPrompt(const std::string& info) {
    if (showPrompt) {
      printf("%s", info.c_str());
    }
  }
private:
  scoped_refptr<LocalServerNode> serverNode;
  BatchExecuteShellHost* host;

  std::vector<std::string> cmdLines;
  int nextCommandIndex;
  bool showPrompt;

  base::WeakPtrFactory<BatchExecuteShell>                 weakptr_factory_;
};
}
#endif