#include "remote_shell/script_engine.h"
#include "remote_shell/proto/rs.pb.h"
#include "dpe_base/dpe_base.h"

extern std::string get_iface_address();

namespace rs
{
ScriptEngine::ScriptEngine(): weakptr_factory_(this) {
}

ScriptEngine::~ScriptEngine() {
  shell = NULL;
}

static void exitImpl(ScriptEngine* engine) {
  delete engine;
  base::will_quit_main_loop();
}

static void willExit(ScriptEngine* engine) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(exitImpl, engine));
}

void ScriptEngine::executeCommand(const std::string& target, const std::vector<std::string>& cmdLines) {
  shell = new BatchExecuteShell(this);
  shell->setShowPrompt(true);
  shell->start(target, cmdLines);
}

void ScriptEngine::onStop(bool succeed) {
  printf(succeed ? "Succeed" : "Failed");
  willExit(this);
}

}
