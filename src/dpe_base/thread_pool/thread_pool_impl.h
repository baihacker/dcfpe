// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DPE_BASE_THREAD_POOL_THREAD_POOL_IMPL_H_
#define DPE_BASE_THREAD_POOL_THREAD_POOL_IMPL_H_

#include "third_party/chromium/base/threading/thread.h"
#include "dpe_base/thread_pool.h"

namespace base {

class ThreadPoolImpl : public ThreadPool,
                                         public base::Thread {
 public:
  // Construct a ThreadPoolImpl with the supplied identifier.  It is an error
  // to construct a ThreadPoolImpl that already exists.
  explicit ThreadPoolImpl(ThreadPool::ID identifier);

  // Special constructor for the main (UI) thread and unittests. If a
  // |message_loop| is provied, we use a dummy thread here since the main
  // thread already exists.
  ThreadPoolImpl(ThreadPool::ID identifier,
                    base::MessageLoop* message_loop);
  virtual ~ThreadPoolImpl();

  static void ShutdownThreadPool();

 protected:
  virtual void Init() OVERRIDE;
  virtual void Run(base::MessageLoop* message_loop) OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  // We implement all the functionality of the public ThreadPool
  // functions, but state is stored in the ThreadPoolImpl to keep
  // the API cleaner. Therefore make ThreadPool a friend class.
  friend class ThreadPool;

  // The following are unique function names that makes it possible to tell
  // the thread id from the callstack alone in crash dumps.
  void UIThreadRun(base::MessageLoop* message_loop);
  void DBThreadRun(base::MessageLoop* message_loop);
  void FileThreadRun(base::MessageLoop* message_loop);
  void FileUserBlockingThreadRun(base::MessageLoop* message_loop);
  void ProcessLauncherThreadRun(base::MessageLoop* message_loop);
  void CacheThreadRun(base::MessageLoop* message_loop);
  void IOThreadRun(base::MessageLoop* message_loop);
  void ComputeThreadRun(base::MessageLoop* message_loop);

  static bool PostTaskHelper(
      ThreadPool::ID identifier,
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay,
      bool nestable);

  // Common initialization code for the constructors.
  void Initialize();

  // For testing.
  //friend class ContentTestSuiteBaseListener;
  //friend class TestThreadPoolBundle;
  //static void FlushThreadPoolHelper();

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
