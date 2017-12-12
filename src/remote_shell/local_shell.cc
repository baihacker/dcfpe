#include "remote_shell/local_shell.h"
#include "remote_shell/proto/rs.pb.h"
#include "dpe_base/dpe_base.h"

extern std::string get_iface_address();

namespace rs {
LocalShell::LocalShell(): weakptr_factory_(this) {
}

LocalShell::~LocalShell() {
  serverNode = NULL;
}

static void exitImpl(LocalShell* node) {
  delete node;
  base::will_quit_main_loop();
}

static void willExit(LocalShell* shell) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(exitImpl, shell));
}

static void runNextCommandImpl(LocalShell* shell) {
  shell->executeCommand();
}

static void willRunNextCommand(LocalShell* shell) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(runNextCommandImpl, shell));
}

void LocalShell::start(const std::string& target) {
  serverNode = new rs::LocalServerNode(this, get_iface_address());
  if (!serverNode->start()) {
    LOG(ERROR) << "Cannot start local server node.";
    serverNode = NULL;
    willExit(this);
  } else {
    printf("Connecting to %s \n", target.c_str());
    serverNode->connectToTarget(target);
  }
}

void LocalShell::onConnectStatusChanged(int newStatus) {
  if (newStatus == ServerStatus::TIMEOUT) {
    printf("Time out!\n");
    willExit(this);
  } else if (newStatus == ServerStatus::FAILED) {
    printf("Cannot connect to %s!\n", serverNode->target().c_str());
    willExit(this);
  } else if (newStatus == ServerStatus::CONNECTED) {
    printf("Connected!\n");
    willRunNextCommand(this);
  }
}

void LocalShell::onCommandStatusChanged(int newStatus) {
  if (newStatus == ServerStatus::TIMEOUT) {
    printf("Command time out!\n");
  } else if (newStatus == ServerStatus::FAILED) {
    printf("Command failed!\n");
  }

  willRunNextCommand(this);
}

char line[1<<16];
void LocalShell::executeCommand() {
  for (;;) {
    printf("rs>");
    gets(line);
    if (serverNode->executeCommandRemotely(line)) {
      // An "exit" or "q" command is executed.
      if (serverNode->canExitNow()) {
        willExit(this);
      }
      break;
    }
  }
}
}
