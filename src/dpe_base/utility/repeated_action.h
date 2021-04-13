#ifndef DPE_BASE_UTILITY_REPEATED_ACTION_H_
#define DPE_BASE_UTILITY_REPEATED_ACTION_H_

#include "dpe_base/dpe_base_export.h"
#include "dpe_base/chromium_base.h"
#include "dpe_base/thread_pool.h"

namespace base {
class RepeatedAction;
class RepeatedActionHost {
 public:
  virtual void OnRepeatedActionFinish(RepeatedAction* ra) = 0;
  virtual ~RepeatedActionHost() {}
};

class DPE_BASE_EXPORT RepeatedAction : public base::RefCounted<RepeatedAction> {
 public:
  RepeatedAction(RepeatedActionHost* host);
  ~RepeatedAction();

  bool Start(std::function<void()> callback, int delay, int period,
             int repeated_time);
  bool Start(const base::Closure& action, base::TimeDelta time_delay,
             base::TimeDelta time_interval, int32_t repeated_time);
  bool Stop();
  bool Restart(base::TimeDelta time_delay, base::TimeDelta time_interval,
               int32_t repeated_time);

  static void DoAction(base::WeakPtr<RepeatedAction> self, int32_t cookie);
  void DoActionImpl(int32_t cookie);

  bool IsRunning() const { return is_running_; }
  base::TimeDelta Interval() const { return interval_; }

 private:
  RepeatedActionHost* host_;
  bool is_running_;
  int32_t current_cookie_;
  base::Closure action_;
  base::TimeDelta interval_;
  int32_t repeated_time_;
  base::WeakPtrFactory<RepeatedAction> weakptr_factory_;
};
}  // namespace base

#endif