#ifndef DPE_DPE_INTERNAL_H_
#define DPE_DPE_INTERNAL_H_

#include "dpe/dpe.h"

namespace dpe {
Solver* GetSolver();
int GetThreadNumber();
int GetBatchSize();
void WillExitDpe();
void RunDpe(Solver* solver, int argc, char* argv[]);
}  // namespace dpe

#endif
