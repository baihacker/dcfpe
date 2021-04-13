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
  // The number of worker threads
  int thread_number = 1;
  // The expected size in Solver::Compute
  // size <= batch_size
  int batch_size = 1;
  // The argument forwarded to Solver::Compute
  int parallel_info = 0;
};

Solver* GetSolver();
std::string GetDpeModuleDir();
std::string GetExecutableDir();
const Flags& GetFlags();
void WillExitDpe();
void RunDpe(Solver* solver, int argc, char* argv[]);
}  // namespace dpe

#endif
