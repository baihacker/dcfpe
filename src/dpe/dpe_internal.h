#ifndef DPE_DPE_INTERNAL_H_
#define DPE_DPE_INTERNAL_H_

#include "dpe/dpe.h"

#include <string>

namespace dpe {
struct Flags {
  int http_port = 80;
  std::string type = "server";
  std::string my_ip;
  std::string server_ip;
  int logging_level = 0;
  int server_port = 0;
  bool read_state = true;
  // The max task count in GetTaskRequest.
  // If the value is zero, use thread_number * 3.
  // If max task count is zero, the server use 1.
  int batch_size = 0;
  // Currently, thread number is forwarded to worker ans an argument.
  // This worker is not responsible for executing the tasks in parallel.
  int thread_number = 1;
};

Solver* GetSolver();
const Flags& GetFlags();
void WillExitDpe();
void RunDpe(Solver* solver, int argc, char* argv[]);
}  // namespace dpe

#endif
