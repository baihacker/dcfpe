#include "remote_shell/script_engine.h"
#include "remote_shell/proto/rs.pb.h"
#include "dpe_base/dpe_base.h"
#include <google/protobuf/text_format.h>

extern std::string get_iface_address();

namespace rs
{
ScriptEngine::ScriptEngine(): nextTargetIndex(0), weakptr_factory_(this) {
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
  targets.push_back(target);
  commands.push_back(cmdLines);

  willRunOnNextTarget();
}

void ScriptEngine::onStop(bool succeed) {
  printf("stoped\n");
  if (succeed) {
    willRunOnNextTarget();
  } else {
    printf("Failed\n");
    willExit(this);
  }
}

void ScriptEngine::runScript(const std::string& filePath, const std::string& action) {
  if (!base::PathExists(base::FilePath(base::UTF8ToNative(filePath)))) {
    printf("%s doesn't exist\n", filePath.c_str());
    willExit(this);
    return;
  }

  std::string data;
  if (!base::ReadFileToString(base::FilePath(base::UTF8ToNative(filePath)), &data)) {
    printf("Cannot read %s\n", filePath.c_str());
    willExit(this);
    return;
  }

  rs::AppPackage package;
  if (google::protobuf::TextFormat::ParseFromString(data, &package)) {
    printf("Found app package.\n");
    runAppPackage(package, action);
    return;
  }

  printf("Unknown script.\n");
}

void ScriptEngine::runAppPackage(const rs::AppPackage& package, const std::string& action) {
  const std::string name = package.name();
  if (name.empty()) {
    printf("The name is empty!");
    willExit(this);
    return;
  }

  const Resource& resource = package.resource();
  std::string resourceDir = resource.local_dir();
  if (!resourceDir.empty() && !EndsWith(resourceDir, "/", false)) {
    resourceDir += "/";
  }

  if (!resourceDir.empty() && !base::PathExists(base::FilePath(base::UTF8ToNative(resourceDir)))) {
    printf("%s doesn't exist!", resourceDir.c_str());
    willExit(this);
    return;
  }


  for (auto& iter: resource.files()) {
    const std::string path = resourceDir + iter;
    if (!base::PathExists(base::FilePath(base::UTF8ToNative(path)))) {
      printf("%s doesn't exist!", iter.c_str());
      willExit(this);
      return;
    }
  }

  if (action == "deploy") {
    for (const auto& deploy: package.deploys()) {
      std::vector<std::string> cmds;
      std::string targetDir = deploy.target_dir();
      if (!targetDir.empty()) {
        cmds.push_back("mkdir " + targetDir);
      }
      if (!targetDir.empty() && !EndsWith(resourceDir, "/", false)) {
        targetDir += "/";
      }
      for (const auto& iter: resource.files()) {
        const std::string srcPath = resourceDir + iter;
        const std::string destPath = targetDir + iter;
        cmds.push_back("fst " + srcPath + " " + destPath);
      }

      for (const auto& iter: deploy.deploy()) {
        cmds.push_back(iter);
      }
      
      for (const auto& host: deploy.hosts()) {
        targets.push_back(host);
        commands.push_back(cmds);
      }
    }

    willRunOnNextTarget();
  } else if (action == "stop") {
    // TODO(baihacker): implement it
  } else {
    printf("Unknown action!\n");
  }
}

void ScriptEngine::willRunOnNextTarget() {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(ScriptEngine::runOnNextTarget, weakptr_factory_.GetWeakPtr()));
}

void ScriptEngine::runOnNextTarget(base::WeakPtr<ScriptEngine> pThis) {
  if (auto* self = pThis.get()) {
    self->runOnNextTargetImpl();
  }
}

void ScriptEngine::runOnNextTargetImpl() {
  if (nextTargetIndex == targets.size()) {
    printf("Script finished!\n");
    willExit(this);
  } else {
    shell = new BatchExecuteShell(this);
    shell->setShowPrompt(true);
    shell->setShowCommandOutput(false);
    shell->setShowCommandErrorOutput(false);
    const int id = nextTargetIndex++;
    printf("\nRun script on target: %s\n", targets[id].c_str());
    shell->start(targets[id], commands[id]);
  }
}
}