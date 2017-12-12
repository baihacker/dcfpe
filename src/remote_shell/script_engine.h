#ifndef REMOTE_SHELL_SCRIPT_ENGINE_H_
#define REMOTE_SHELL_SCRIPT_ENGINE_H_

#include "remote_shell/batch_execute_shell.h"
#include "remote_shell/proto/deploy.pb.h"

namespace rs {
enum InternalOperation {
  EXIT = -1,
};

struct BatchOperation {
  int id;
  std::string target;
  std::vector<std::string> commands;
  int onSucceed;
  int onFailed;
};

struct ScriptCompiler {
  virtual ~ScriptCompiler() {}
  virtual bool accept(const base::FilePath& absFilePath, const std::string& data) = 0;
  virtual bool compile(const std::string& action, std::vector<BatchOperation>& result) = 0;
};

class ScriptEngine : public BatchExecuteShellHost, public base::RefCounted<ScriptEngine> {
public:
  ScriptEngine();
  ~ScriptEngine();

  void executeCommand(const std::string& target, const std::vector<std::string>& cmdLines);
  void runScript(const std::string& filePath, const std::string& action);
  void onStop(bool succeed);

private:
  void willExecuteNextOperation();
  static void executeNextOperation(base::WeakPtr<ScriptEngine> pThis);
  void executeNextOperationImpl();

private:
  scoped_refptr<BatchExecuteShell> shell;

  std::vector<ScriptCompiler*> compilers;

  std::vector<BatchOperation> scriptOperations;
  int nextOperationId;
  int runningOperationIdx;

  base::WeakPtrFactory<ScriptEngine>                 weakptr_factory_;
};
}
#endif