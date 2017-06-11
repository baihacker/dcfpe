// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DPE_BASE_THREAD_POOL_H_
#define DPE_BASE_THREAD_POOL_H_

#include <string>

#include "third_party/chromium/base/basictypes.h"
#include "third_party/chromium/base/callback.h"
#include "third_party/chromium/base/location.h"
#include "third_party/chromium/base/logging.h"
#include "third_party/chromium/base/message_loop/message_loop_proxy.h"
#include "third_party/chromium/base/task_runner_util.h"
#include "third_party/chromium/base/time/time.h"

#include "dpe_base/dpe_base_export.h"

namespace base {
class MessageLoop;
class SequencedWorkerPool;
class Thread;
}

namespace base {

class ThreadPoolDelegate;
class ThreadPoolImpl;

// Use DCHECK_CURRENTLY_ON(ThreadPool::ID) to assert that a function can only
// be called on the named ThreadPool.
#define DCHECK_CURRENTLY_ON(thread_identifier)                      \
  (DCHECK(::base::ThreadPool::CurrentlyOn(thread_identifier)) \
   << ::base::ThreadPool::GetDCheckCurrentlyOnErrorMessage(   \
          thread_identifier))

class DPE_BASE_EXPORT ThreadPool {
 public:
  enum ID {
    UI,
    DB,
    FILE,
    FILE_USER_BLOCKING,
    PROCESS_LAUNCHER,
    CACHE,
    IO,
    COMPUTE,
    ID_COUNT
  };

  static bool PostTask(ID identifier,
                       const tracked_objects::Location& from_here,
                       const base::Closure& task);
  static bool PostDelayedTask(ID identifier,
                              const tracked_objects::Location& from_here,
                              const base::Closure& task,
                              base::TimeDelta delay);
  static bool PostNonNestableTask(ID identifier,
                                  const tracked_objects::Location& from_here,
                                  const base::Closure& task);
  static bool PostNonNestableDelayedTask(
      ID identifier,
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay);

  static bool PostTaskAndReply(
      ID identifier,
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      const base::Closure& reply);

  template <typename ReturnType, typename ReplyArgType>
  static bool PostTaskAndReplyWithResult(
      ID identifier,
      const tracked_objects::Location& from_here,
      const base::Callback<ReturnType(void)>& task,
      const base::Callback<void(ReplyArgType)>& reply) {
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy =
        GetMessageLoopProxyForThread(identifier);
    return base::PostTaskAndReplyWithResult(
        message_loop_proxy.get(), from_here, task, reply);
  }

  template <class T>
  static bool DeleteSoon(ID identifier,
                         const tracked_objects::Location& from_here,
                         const T* object) {
    return GetMessageLoopProxyForThread(identifier)->DeleteSoon(
        from_here, object);
  }

  template <class T>
  static bool ReleaseSoon(ID identifier,
                          const tracked_objects::Location& from_here,
                          const T* object) {
    return GetMessageLoopProxyForThread(identifier)->ReleaseSoon(
        from_here, object);
  }


  static bool PostBlockingPoolTask(const tracked_objects::Location& from_here,
                                   const base::Closure& task);
  static bool PostBlockingPoolTaskAndReply(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      const base::Closure& reply);
  static bool PostBlockingPoolSequencedTask(
      const std::string& sequence_token_name,
      const tracked_objects::Location& from_here,
      const base::Closure& task);

  // Returns the thread pool used for blocking file I/O. Use this object to
  // perform random blocking operations such as file writes or querying the
  // Windows registry.
  static base::SequencedWorkerPool* GetBlockingPool() WARN_UNUSED_RESULT;

  // Callable on any thread.  Returns whether the given well-known thread is
  // initialized.
  static bool IsThreadInitialized(ID identifier) WARN_UNUSED_RESULT;

  // Callable on any thread.  Returns whether you're currently on a particular
  // thread.  To DCHECK this, use the DCHECK_CURRENTLY_ON() macro above.
  static bool CurrentlyOn(ID identifier) WARN_UNUSED_RESULT;

  // Callable on any thread.  Returns whether the threads message loop is valid.
  // If this returns false it means the thread is in the process of shutting
  // down.
  static bool IsMessageLoopValid(ID identifier) WARN_UNUSED_RESULT;

  // If the current message loop is one of the known threads, returns true and
  // sets identifier to its ID.  Otherwise returns false.
  static bool GetCurrentThreadIdentifier(ID* identifier) WARN_UNUSED_RESULT;

  // Callers can hold on to a refcounted MessageLoopProxy beyond the lifetime
  // of the thread.
  static scoped_refptr<base::MessageLoopProxy> GetMessageLoopProxyForThread(
      ID identifier);

  // Returns a pointer to the thread's message loop, which will become
  // invalid during shutdown, so you probably shouldn't hold onto it.
  //
  // This must not be called before the thread is started, or after
  // the thread is stopped, or it will DCHECK.
  //
  // Ownership remains with the BrowserThread implementation, so you
  // must not delete the pointer.
  static base::MessageLoop* UnsafeGetMessageLoopForThread(ID identifier);

  // Sets the delegate for the specified BrowserThread.
  //
  // Only one delegate may be registered at a time.  Delegates may be
  // unregistered by providing a NULL pointer.
  //
  // If the caller unregisters a delegate before CleanUp has been
  // called, it must perform its own locking to ensure the delegate is
  // not deleted while unregistering.
  static void SetDelegate(ID identifier, ThreadPoolDelegate* delegate);

  // Use these templates in conjuction with RefCountedThreadSafe when you want
  // to ensure that an object is deleted on a specific thread.  This is needed
  // when an object can hop between threads (i.e. IO -> FILE -> IO), and thread
  // switching delays can mean that the final IO tasks executes before the FILE
  // task's stack unwinds.  This would lead to the object destructing on the
  // FILE thread, which often is not what you want (i.e. to unregister from
  // NotificationService, to notify other objects on the creating thread etc).
  template<ID thread>
  struct DeleteOnThread {
    template<typename T>
    static void Destruct(const T* x) {
      if (CurrentlyOn(thread)) {
        delete x;
      } else {
        if (!DeleteSoon(thread, FROM_HERE, x)) {
#if defined(UNIT_TEST)
          // Only logged under unit testing because leaks at shutdown
          // are acceptable under normal circumstances.
          // LOG(ERROR) << "DeleteSoon failed on thread " << thread;
#endif  // UNIT_TEST
        }
      }
    }
  };


  struct DeleteOnUIThread : public DeleteOnThread<UI> { };
  struct DeleteOnIOThread : public DeleteOnThread<IO> { };
  struct DeleteOnFileThread : public DeleteOnThread<FILE> { };
  struct DeleteOnDBThread : public DeleteOnThread<DB> { };

  static std::string GetDCheckCurrentlyOnErrorMessage(ID expected);

  
  static void InitializeThreadPool();
  static int RunMainLoop(MessagePumpDispatcher* dispatcher = NULL);
  static void DeinitializeThreadPool();
  
 private:
  friend class ThreadPoolImpl;

  ThreadPool() {}
  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

}

#endif
