#ifndef DPE_DPE_INTERNAL_H_
#define DPE_DPE_INTERNAL_H_

#include "dpe/dpe.h"

namespace dpe
{
Solver* getSolver();
void willExitDpe();

CacheReader* newCacheReader(const char* path);
CacheWriter* newCacheWriter(const char* path, bool reset);
CacheReader* newDefaultCacheReader();
CacheWriter* newDefaultCacheWriter();
void runDpe(Solver* solver, int argc, char* argv[]);
}

#endif
