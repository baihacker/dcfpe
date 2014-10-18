#ifndef DPE_BASE_DPE_BASE_H_
#define DPE_BASE_DPE_BASE_H_

#include "dpe_base/dpe_base_export.h"
#include "dpe_base/interface_base.h"
#include "dpe_base/utility_interface.h"
#include "dpe_base/chromium_base.h"
#include "dpe_base/thread_pool.h"
#include "dpe_base/zmq_adapter.h"
#include "dpe_base/io_handler.h"
#include "dpe_base/pipe.h"

namespace base
{
DPE_BASE_EXPORT int32_t dpe_base_main(void (*logic_main)());
DPE_BASE_EXPORT void quit_main_loop();
DPE_BASE_EXPORT MessageCenter* zmq_message_center();
DPE_BASE_EXPORT ZMQServer* zmq_server();
DPE_BASE_EXPORT ZMQClient* zmq_client();
}

namespace base
{
DPE_BASE_EXPORT bool StringEqualCaseInsensitive(const std::wstring& x, const std::wstring& y);
DPE_BASE_EXPORT bool StringEqualCaseInsensitive(const std::string& x, const std::string& y);
}

#endif