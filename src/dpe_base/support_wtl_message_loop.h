#ifndef DPE_BASE_ATL_MESSAGE_LOOP_
#define DPE_BASE_ATL_MESSAGE_LOOP_

#include "dpe_base/dpe_base_export.h"
#include "dpe_base/chromium_base.h"

#include <atlbase.h>
#include <atlapp.h>

class DPE_BASE_EXPORT CChromiumMessageLoop : public base::MessagePumpDispatcher, public WTL::CMessageLoop
{
public:
  uint32_t Dispatch(const base::NativeEvent& msg) override
  {
    BOOL bDoIdle = FALSE;
    int nIdleCount = 0;
    if (!PreTranslateMessage(const_cast<MSG*>(&msg)))
    {
      ::TranslateMessage(const_cast<MSG*>(&msg));
      ::DispatchMessage(const_cast<MSG*>(&msg));
    }

    if (IsIdleMessage(const_cast<MSG*>(&msg)))
    {
      bDoIdle = TRUE;
      nIdleCount = 0;
    }
    
    while(bDoIdle && !::PeekMessage(&m_msg, NULL, 0, 0, PM_NOREMOVE))
    {
      if(!OnIdle(nIdleCount++))
        bDoIdle = FALSE;
    }

    return base::MessagePumpDispatcher::POST_DISPATCH_NONE;
  }
};

#endif