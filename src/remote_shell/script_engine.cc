#include "remote_shell/script_engine.h"

#include "dpe_base/dpe_base.h"

#include <google/protobuf/text_format.h>

extern std::string get_iface_address();

namespace rs {
struct DeployScriptCompiler : public ScriptCompiler {
  DeployScriptCompiler(){}

  ~DeployScriptCompiler(){}

  bool accept(const base::FilePath& absFilePath, const std::string& data) {
    this->absFilePath = absFilePath;
    if (google::protobuf::TextFormat::ParseFromString(data, &package)) {
      printf("Found app package.\n");
      return true;
    }
    return false;
  }

  bool compile(const std::string& action, std::vector<BatchOperation>& result) {
    // If the resource dir is relative path, use this as base.
    auto baseDir = absFilePath.DirName();
    
    std::vector<BatchOperation>().swap(result);

    const std::string name = package.name();
    if (name.empty()) {
      printf("The package name is empty!");
      return false;
    }

    const Resource& resource = package.resource();

    base::FilePath resourcePath = base::FilePath(base::UTF8ToNative(resource.local_dir()));
    if (!resourcePath.IsAbsolute()) {
      resourcePath = base::MakeAbsoluteFilePath(baseDir.Append(base::UTF8ToNative(resource.local_dir())));
    }

    if (!base::PathExists(resourcePath)) {
      printf("Resource dir %s doesn't exist!\n", base::NativeToUTF8(resourcePath.value()).c_str());
      return false;
    }

    for (auto& iter: resource.files()) {
      auto path = base::MakeAbsoluteFilePath(resourcePath.Append(base::UTF8ToNative(iter)));
      if (!base::PathExists(path)) {
        printf("Resource path %s doesn't exist!\n", base::NativeToUTF8(path.value()).c_str());
        return false;
      }
    }

    int nextId = 0;
    if (action == "deploy") {
      for (const auto& deploy: package.deploys()) {
        std::vector<std::string> cmds;
        std::string targetDir = deploy.target_dir();
        if (!targetDir.empty()) {
          cmds.push_back("mkdir " + targetDir);
        }
        if (!targetDir.empty() && !EndsWith(targetDir, "/", false)) {
          targetDir += "/";
        }
        for (const auto& iter: resource.files()) {
          const std::string srcPath = base::NativeToUTF8(base::MakeAbsoluteFilePath(resourcePath.Append(base::UTF8ToNative(iter))).value());
          const std::string destPath = targetDir + iter;
          cmds.push_back("fst " + srcPath + " " + destPath);
        }

        for (const auto& iter: deploy.deploy()) {
          cmds.push_back(iter);
        }

        for (const auto& host: deploy.hosts()) {
          BatchOperation t;
          t.id = nextId++;
          t.target = host;
          t.commands = cmds;
          t.onSucceed = nextId;
          t.onFailed = InternalOperation::EXIT;
          result.push_back(t);
        }
      }
      result.back().onSucceed = EXIT;
      return true;
    } else if (action == "stop") {
      for (const auto& deploy: package.deploys()) {
        std::vector<std::string> cmds;
        for (const auto& iter: deploy.stop()) {
          cmds.push_back(iter);
        }

        for (const auto& host: deploy.hosts()) {
          BatchOperation t;
          t.id = nextId++;
          t.target = host;
          t.commands = cmds;
          t.onSucceed = InternalOperation::EXIT;
          t.onFailed = InternalOperation::EXIT;
          result.push_back(t);
        }
      }
      result.back().onSucceed = EXIT;
      return true;
    } else {
      printf("Unknown action!\n");
      return false;
    }
  }
private:
  rs::DeployPackage package;
  base::FilePath absFilePath;
};

ScriptEngine::ScriptEngine(): nextOperationId(0), weakptr_factory_(this) {
  compilers.push_back(new DeployScriptCompiler);
}

ScriptEngine::~ScriptEngine() {
  shell = NULL;
  for (auto* compiler: compilers) {
    delete compiler;
  }
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
  std::vector<BatchOperation>().swap(scriptOperations);

  BatchOperation t;
  t.id = 0;
  t.target = target;
  t.commands = cmdLines;
  t.onSucceed = 1;
  t.onFailed = 1;
  scriptOperations.push_back(t);

  nextOperationId = scriptOperations[0].id;
  willExecuteNextOperation();
}

void ScriptEngine::onStop(bool succeed) {
  if (succeed) {
    printf("Operation succeeds, id = %d.\n", nextOperationId);
    nextOperationId = scriptOperations[runningOperationIdx].onSucceed;
    willExecuteNextOperation();
  } else {
    printf("Operation failed, id = %d.\n", nextOperationId);
    nextOperationId = scriptOperations[runningOperationIdx].onFailed;
    willExecuteNextOperation();
  }
}

void ScriptEngine::runScript(const std::string& filePath, const std::string& action) {
  auto absFilePath = base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToNative(filePath)));

  if (!base::PathExists(absFilePath)) {
    printf("%s doesn't exist\n", base::NativeToUTF8(absFilePath.value()).c_str());
    willExit(this);
    return;
  }

  std::string data;
  if (!base::ReadFileToString(absFilePath, &data)) {
    printf("Cannot read %s\n", base::NativeToUTF8(absFilePath.value()).c_str());
    willExit(this);
    return;
  }

  for (auto* compiler: compilers) {
    std::vector<BatchOperation> result;
    if (compiler->accept(absFilePath, data)) {
      if (compiler->compile(action, result)) {
        printf("Script compiled.\n");
        scriptOperations = std::move(result);

        if (scriptOperations.empty()) {
          printf("Finish executing script!\n");
          willExit(this);
        } else {
          printf("Start executing script!\n");
          nextOperationId = scriptOperations[0].id;
          willExecuteNextOperation();
        }
      } else {
        printf("Failed to compile this script.");
        willExit(this);
      }
      return;
    }
  }

  printf("Unknown script.\n");
  willExit(this);
}

void ScriptEngine::willExecuteNextOperation() {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(ScriptEngine::executeNextOperation, weakptr_factory_.GetWeakPtr()));
}

void ScriptEngine::executeNextOperation(base::WeakPtr<ScriptEngine> pThis) {
  if (auto* self = pThis.get()) {
    self->executeNextOperationImpl();
  }
}

void ScriptEngine::executeNextOperationImpl() {
  if (nextOperationId == InternalOperation::EXIT) {
    printf("\nFinish executing script!\n");
    willExit(this);
  } else {
    printf("\n***********************************\n");
    printf("Execute operation, id = %d.\n", nextOperationId);
    runningOperationIdx = -1;
    const int size = scriptOperations.size();
    for (int i = 0; i < size; ++i) {
      if (scriptOperations[i].id == nextOperationId) {
        runningOperationIdx = i;
        break;
      }
    }
    if (runningOperationIdx == -1) {
      printf("Unknown operation, id = %d!\n", nextOperationId);
      printf("\nFinish executing script!\n");
      willExit(this);
      return;
    }
    shell = new BatchExecuteShell(this);
    shell->setShowPrompt(true);

    const int id = runningOperationIdx;
    printf("Running on target: %s\n", scriptOperations[id].target.c_str());
    shell->start(scriptOperations[id].target, scriptOperations[id].commands);
  }
}
}