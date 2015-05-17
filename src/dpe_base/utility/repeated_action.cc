#include "repeated_action.h"

namespace base
{
RepeatedAction::RepeatedAction() : weakptr_factory_(this), is_running_(false)
{
}

RepeatedAction::~RepeatedAction()
{
}

bool RepeatedAction::Start(const base::Closure& action,
        base::TimeDelta time_delay, base::TimeDelta time_interval, int32_t repeated_time)
{
  return false;
}

bool RepeatedAction::Stop()
{
  return false;
}


}