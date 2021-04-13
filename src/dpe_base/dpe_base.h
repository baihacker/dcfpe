#ifndef DPE_BASE_DPE_BASE_H_
#define DPE_BASE_DPE_BASE_H_

#include "dpe_base/build_config.h"
#include "dpe_base/support_string.h"

#include "dpe_base/dpe_base_export.h"
#include "dpe_base/interface_base.h"
#include "dpe_base/utility_interface.h"
#include "dpe_base/chromium_base.h"
#include "dpe_base/thread_pool.h"
#include "dpe_base/zmq_adapter.h"
#include "dpe_base/io_handler.h"
#include "dpe_base/pipe.h"
#include "dpe_base/utility/repeated_action.h"

namespace base
{
DPE_BASE_EXPORT int32_t dpe_base_main(void (*logic_main)(), base::MessagePumpDispatcher* dispatcher = NULL, int loggingLevel = 1);
DPE_BASE_EXPORT void will_quit_main_loop();
DPE_BASE_EXPORT MessageCenter* zmq_message_center();
DPE_BASE_EXPORT ZMQServer* zmq_server();
DPE_BASE_EXPORT ZMQClient* zmq_client();
DPE_BASE_EXPORT void set_blocking_pool_thread_number(int thread_number);
DPE_BASE_EXPORT int get_blocking_pool_thread_number();
}

namespace base
{
DPE_BASE_EXPORT bool StringEqualCaseInsensitive(const std::wstring& x, const std::wstring& y);
DPE_BASE_EXPORT bool StringEqualCaseInsensitive(const std::string& x, const std::string& y);

//DPE_BASE_EXPORT std::wstring NativeToWide(const NativeString& x);
DPE_BASE_EXPORT std::string NativeToUTF8(const NativeString& x);
//DPE_BASE_EXPORT NativeString WideToNative(const std::wstring& x);
DPE_BASE_EXPORT NativeString UTF8ToNative(const std::string& x);

DPE_BASE_EXPORT std::string PhysicalAddress();

DPE_BASE_EXPORT void AddPaAndTs(base::DictionaryValue* value);
}

#endif