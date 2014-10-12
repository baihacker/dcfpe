#ifndef DPE_BASE_DPE_BASE_H_
#define DPE_BASE_DPE_BASE_H_

#include "dpe_base/dpe_base_export.h"
#include "dpe_base/interface_base.h"
#include "dpe_base/utility_interface.h"
#include "dpe_base/chromium_base.h"
#include "dpe_base/thread_pool.h"

namespace base
{
DPE_BASE_EXPORT int32_t dpe_base_main(void (*logic_main)());
DPE_BASE_EXPORT void quit_main_loop();
}
#endif