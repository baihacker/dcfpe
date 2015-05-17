#ifndef DPE_BASE_UTILITY_REPEATED_ACTION_H_
#define DPE_BASE_UTILITY_REPEATED_ACTION_H_

#include "dpe_base/dpe_base_export.h"
#include "dpe_base/chromium_base.h"

namespace base
{
class RepeatedAction : public base::RefCounted<RepeatedAction>
{
public:
RepeatedAction();
~RepeatedAction();
bool Start(const base::Closure& action, base::TimeDelta time_delay, base::TimeDelta time_interval, int32_t repeated_time);
bool Stop();

private:
bool                                                  is_running_;
base::Closure                                         action_;
base::TimeDelta                                       interval_;
int32_t                                               repeated_time_;
base::WeakPtrFactory<RepeatedAction>                  weakptr_factory_;
};
}

#endif