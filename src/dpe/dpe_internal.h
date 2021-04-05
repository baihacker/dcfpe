#ifndef DPE_DPE_INTERNAL_H_
#define DPE_DPE_INTERNAL_H_

#include "dpe/dpe.h"

namespace dpe {
Solver* getSolver();
void willExitDpe();
void runDpe(Solver* solver, int argc, char* argv[]);
}  // namespace dpe

#endif
