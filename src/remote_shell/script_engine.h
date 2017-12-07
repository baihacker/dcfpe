#ifndef REMOTE_SHELL_SCRIPT_ENGINE_H_
#define REMOTE_SHELL_SCRIPT_ENGINE_H_

#include "remote_shell/batch_execute_shell.h"

namespace rs
{
class ScriptEngine : public BatchExecuteShellHost, public base::RefCounted<ScriptEngine> {
public:
  ScriptEngine();
  ~ScriptEngine();

  void executeCommand(const std::string& target, const std::vector<std::string>& cmdLines);
  void onStop(bool succeed);
private:
  scoped_refptr<BatchExecuteShell> shell;
  base::WeakPtrFactory<ScriptEngine>                 weakptr_factory_;
};
}
#endif