#ifndef REMOTE_SHELL_SCRIPT_ENGINE_H_
#define REMOTE_SHELL_SCRIPT_ENGINE_H_

#include "remote_shell/batch_execute_shell.h"
#include "remote_shell/proto/deploy.pb.h"
namespace rs
{
class ScriptEngine : public BatchExecuteShellHost, public base::RefCounted<ScriptEngine> {
public:
  ScriptEngine();
  ~ScriptEngine();

  void executeCommand(const std::string& target, const std::vector<std::string>& cmdLines);
  void runScript(const std::string& filePath, const std::string& action);
  void onStop(bool succeed);
  
private:
  void runAppPackage(const rs::AppPackage& package, const std::string& action);
  
  void willRunOnNextTarget();
  static void runOnNextTarget(base::WeakPtr<ScriptEngine> pThis);
  void runOnNextTargetImpl();
private:
  scoped_refptr<BatchExecuteShell> shell;
  std::vector<std::string> targets;
  std::vector<std::vector<std::string>> commands;
  int nextTargetIndex;
  base::WeakPtrFactory<ScriptEngine>                 weakptr_factory_;
};
}
#endif