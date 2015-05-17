#include "repeated_action.h"

namespace base
{

RepeatedAction::RepeatedAction(RepeatedActionHost* host) :
  host_(host), weakptr_factory_(this), is_running_(false), current_cookie_(0), repeated_time_(-1)
{
}

RepeatedAction::~RepeatedAction()
{
}

bool RepeatedAction::Start(const base::Closure& action,
        base::TimeDelta time_delay, base::TimeDelta time_interval, int32_t repeated_time)
{
  if (time_delay.ToInternalValue() < 0) time_delay = base::TimeDelta::FromMicroseconds(0);
  is_running_ = true;
  ++current_cookie_;
  action_ = action;
  repeated_time_ = repeated_time;
  interval_ = time_interval;
  if (time_delay.ToInternalValue() == 0)
  {
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&RepeatedAction::DoAction, weakptr_factory_.GetWeakPtr(), current_cookie_)
        );
  }
  else
  {
    base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&RepeatedAction::DoAction, weakptr_factory_.GetWeakPtr(), current_cookie_),
      time_delay
        );
  }
  return true;
}

bool RepeatedAction::Stop()
{
  is_running_ = false;
  ++current_cookie_;
  return true;
}

void RepeatedAction::DoAction(base::WeakPtr<RepeatedAction> self, int32_t cookie)
{
  if (auto* pThis = self.get()) pThis->DoActionImpl(cookie);
}

void RepeatedAction::DoActionImpl(int32_t cookie)
{
  if (is_running_ && cookie == current_cookie_)
  {
    if (repeated_time_ == 0)
    {
      if (host_)
      {
        host_->OnRepeatedActionFinish(this);
      }
    }
    else if (repeated_time_ < 0 || repeated_time_--)
    {
      action_.Run();
      base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
        base::Bind(&RepeatedAction::DoAction, weakptr_factory_.GetWeakPtr(), current_cookie_),
        interval_
        );
    }
  }
}

}