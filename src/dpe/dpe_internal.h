#ifndef DPE_DPE_INTERNAL_H_
#define DPE_DPE_INTERNAL_H_

#include "dpe/dpe.h"

namespace dpe
{
Solver* getSolver();
void willExitDpe();

void* newCacheReader(const char* path);
void* newCacheWriter(const char* path, bool reset);
void* newDefaultCacheReader();
void* newDefaultCacheWriter();
void runDpe(Solver* solver, int argc, char* argv[]);
}

#endif
