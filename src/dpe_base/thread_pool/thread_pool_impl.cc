// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dpe_base/thread_pool/thread_pool_impl.h"

#include <string>

#include "third_party/chromium/base/atomicops.h"
#include "third_party/chromium/base/bind.h"
#include "third_party/chromium/base/compiler_specific.h"
#include "third_party/chromium/base/lazy_instance.h"
#include "third_party/chromium/base/message_loop/message_loop.h"
#include "third_party/chromium/base/message_loop/message_loop_proxy.h"
#include "third_party/chromium/base/threading/sequenced_worker_pool.h"
#include "third_party/chromium/base/threading/thread_restrictions.h"
#include "dpe_base/dpe_base.h"
#include "dpe_base/thread_pool_delegate.h"

#include "third_party/chromium/base/run_loop.h"
#include "third_party/chromium/base/at_exit.h"

namespace base {

namespace {

// Friendly names for the well-known threads.
static const char* g_browser_thread_names[ThreadPool::ID_COUNT] = {
  "",  // UI (name assembled in browser_main.cc).
  "DPE_DBThread",  // DB
  "DPE_FileThread",  // FILE
  "DPE_FileUserBlockingThread",  // FILE_USER_BLOCKING
  "DPE_ProcessLauncherThread",  // PROCESS_LAUNCHER
  "DPE_CacheThread",  // CACHE
  "DPE_IOThread",  // IO
  "DPE_ComputeThread",  // COMPUTE
};

// An implementation of MessageLoopProxy to be used in conjunction
// with ThreadPool.
class ThreadPoolMessageLoopProxy : public base::MessageLoopProxy {
 public:
  explicit ThreadPoolMessageLoopProxy(ThreadPool::ID identifier)
      : id_(identifier) {
  }

  // MessageLoopProxy implementation.
  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task, base::TimeDelta delay) OVERRIDE {
    return ThreadPool::PostDelayedTask(id_, from_here, task, delay);
  }

  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE {
    return ThreadPool::PostNonNestableDelayedTask(id_, from_here, task,
                                                     delay);
  }

  virtual bool RunsTasksOnCurrentThread() const OVERRIDE {
    return ThreadPool::CurrentlyOn(id_);
  }

 protected:
  virtual ~ThreadPoolMessageLoopProxy() {}

 private:
  ThreadPool::ID id_;
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolMessageLoopProxy);
};

// A separate helper is used just for the proxies, in order to avoid needing
// to initialize the globals to create a proxy.
struct ThreadPoolProxies {
  ThreadPoolProxies() {
    for (int i = 0; i < ThreadPool::ID_COUNT; ++i) {
      proxies[i] =
          new ThreadPoolMessageLoopProxy(static_cast<ThreadPool::ID>(i));
    }
  }

  scoped_refptr<base::MessageLoopProxy> proxies[ThreadPool::ID_COUNT];
};

base::LazyInstance<ThreadPoolProxies>::Leaky
    g_proxies = LAZY_INSTANCE_INITIALIZER;

struct ThreadPoolGlobals {
  ThreadPoolGlobals()
      : blocking_pool(new base::SequencedWorkerPool(
            base::get_blocking_pool_thread_number(), "BrowserBlocking")) {
    memset(threads, 0, ThreadPool::ID_COUNT * sizeof(threads[0]));
    memset(thread_delegates, 0,
           ThreadPool::ID_COUNT * sizeof(thread_delegates[0]));
  }

  // This lock protects |threads|. Do not read or modify that array
  // without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is protected by |lock|. The threads are not owned by this
  // array. Typically, the threads are owned on the UI thread by
  // BrowserMainLoop. ThreadPoolImpl objects remove themselves from this
  // array upon destruction.
  ThreadPoolImpl* threads[ThreadPool::ID_COUNT];

  // Only atomic operations are used on this array. The delegates are not owned
  // by this array, rather by whoever calls ThreadPool::SetDelegate.
  ThreadPoolDelegate* thread_delegates[ThreadPool::ID_COUNT];

  const scoped_refptr<base::SequencedWorkerPool> blocking_pool;
};

base::LazyInstance<ThreadPoolGlobals>::Leaky
    g_globals = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ThreadPoolImpl::ThreadPoolImpl(ID identifier)
    : Thread(g_browser_thread_names[identifier]),
      identifier_(identifier) {
  Initialize();
}

ThreadPoolImpl::ThreadPoolImpl(ID identifier,
                                     base::MessageLoop* message_loop)
    : Thread(message_loop->thread_name()), identifier_(identifier) {
  set_message_loop(message_loop);
  Initialize();
}

// static
void ThreadPoolImpl::ShutdownThreadPool() {
  // The goal is to make it impossible for chrome to 'infinite loop' during
  // shutdown, but to reasonably expect that all BLOCKING_SHUTDOWN tasks queued
  // during shutdown get run. There's nothing particularly scientific about the
  // number chosen.
  const int kMaxNewShutdownBlockingTasks = 1000;
  ThreadPoolGlobals& globals = g_globals.Get();
  globals.blocking_pool->Shutdown(kMaxNewShutdownBlockingTasks);
}

// static
#if 0
void ThreadPoolImpl::FlushThreadPoolHelper() {
  // We don't want to create a pool if none exists.
  if (g_globals == NULL)
    return;
  g_globals.Get().blocking_pool->FlushForTesting();
}
#endif

void ThreadPoolImpl::Init() {
  ThreadPoolGlobals& globals = g_globals.Get();

  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&globals.thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  ThreadPoolDelegate* delegate =
      reinterpret_cast<ThreadPoolDelegate*>(stored_pointer);
  if (delegate) {
    delegate->Init();
    message_loop()->PostTask(FROM_HERE,
                             base::Bind(&ThreadPoolDelegate::InitAsync,
                                        // Delegate is expected to exist for the
                                        // duration of the thread's lifetime
                                        base::Unretained(delegate)));
  }
}

// We disable optimizations for this block of functions so the compiler doesn't
// merge them all together.
MSVC_DISABLE_OPTIMIZE()
MSVC_PUSH_DISABLE_WARNING(4748)

NOINLINE void ThreadPoolImpl::UIThreadRun(base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void ThreadPoolImpl::DBThreadRun(base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void ThreadPoolImpl::FileThreadRun(
    base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void ThreadPoolImpl::FileUserBlockingThreadRun(
    base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void ThreadPoolImpl::ProcessLauncherThreadRun(
    base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void ThreadPoolImpl::CacheThreadRun(
    base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void ThreadPoolImpl::IOThreadRun(base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void ThreadPoolImpl::ComputeThreadRun(base::MessageLoop* message_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(message_loop);
  CHECK_GT(line_number, 0);
}

MSVC_POP_WARNING()
MSVC_ENABLE_OPTIMIZE();

void ThreadPoolImpl::Run(base::MessageLoop* message_loop) {
#if defined(OS_ANDROID)
  // Not to reset thread name to "Thread-???" by VM, attach VM with thread name.
  // Though it may create unnecessary VM thread objects, keeping thread name
  // gives more benefit in debugging in the platform.
  if (!thread_name().empty()) {
    base::android::AttachCurrentThreadWithName(thread_name());
  }
#endif

  ThreadPool::ID thread_id = ID_COUNT;
  if (!GetCurrentThreadIdentifier(&thread_id))
    return Thread::Run(message_loop);

  switch (thread_id) {
    case ThreadPool::UI:
      return UIThreadRun(message_loop);
    case ThreadPool::DB:
      return DBThreadRun(message_loop);
    case ThreadPool::FILE:
      return FileThreadRun(message_loop);
    case ThreadPool::FILE_USER_BLOCKING:
      return FileUserBlockingThreadRun(message_loop);
    case ThreadPool::PROCESS_LAUNCHER:
      return ProcessLauncherThreadRun(message_loop);
    case ThreadPool::CACHE:
      return CacheThreadRun(message_loop);
    case ThreadPool::IO:
      return IOThreadRun(message_loop);
    case ThreadPool::COMPUTE:
      return ComputeThreadRun(message_loop);
    case ThreadPool::ID_COUNT:
      CHECK(false);  // This shouldn't actually be reached!
      break;
  }
  Thread::Run(message_loop);
}

void ThreadPoolImpl::CleanUp() {
  ThreadPoolGlobals& globals = g_globals.Get();

  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&globals.thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  ThreadPoolDelegate* delegate =
      reinterpret_cast<ThreadPoolDelegate*>(stored_pointer);

  if (delegate)
    delegate->CleanUp();
}

void ThreadPoolImpl::Initialize() {
  ThreadPoolGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(globals.threads[identifier_] == NULL);
  globals.threads[identifier_] = this;
}

ThreadPoolImpl::~ThreadPoolImpl() {
  // All Thread subclasses must call Stop() in the destructor. This is
  // doubly important here as various bits of code check they are on
  // the right ThreadPool.
  Stop();

  ThreadPoolGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  globals.threads[identifier_] = NULL;
#ifndef NDEBUG
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!globals.threads[i]) <<
        "Threads must be listed in the reverse order that they die";
  }
#endif
}

// static
bool ThreadPoolImpl::PostTaskHelper(
    ThreadPool::ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    bool nestable) {
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the target thread
  // outlives current thread.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  ThreadPool::ID current_thread = ID_COUNT;
  bool target_thread_outlives_current =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  ThreadPoolGlobals& globals = g_globals.Get();
  if (!target_thread_outlives_current)
    globals.lock.Acquire();

  base::MessageLoop* message_loop =
      globals.threads[identifier] ? globals.threads[identifier]->message_loop()
                                  : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay);
    }
  }

  if (!target_thread_outlives_current)
    globals.lock.Release();

  return !!message_loop;
}

// static
bool ThreadPool::PostBlockingPoolTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return g_globals.Get().blocking_pool->PostWorkerTask(from_here, task);
}

// static
bool ThreadPool::PostBlockingPoolTaskAndReply(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    const base::Closure& reply) {
  return g_globals.Get().blocking_pool->PostTaskAndReply(
      from_here, task, reply);
}

// static
bool ThreadPool::PostBlockingPoolSequencedTask(
    const std::string& sequence_token_name,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return g_globals.Get().blocking_pool->PostNamedSequencedWorkerTask(
      sequence_token_name, from_here, task);
}

// static
base::SequencedWorkerPool* ThreadPool::GetBlockingPool() {
  return g_globals.Get().blocking_pool.get();
}

// static
bool ThreadPool::IsThreadInitialized(ID identifier) {
  if (g_globals == NULL)
    return false;

  ThreadPoolGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return globals.threads[identifier] != NULL;
}

// static
bool ThreadPool::CurrentlyOn(ID identifier) {
  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  ThreadPoolGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return globals.threads[identifier] &&
         globals.threads[identifier]->message_loop() ==
             base::MessageLoop::current();
}

static const char* GetThreadName(ThreadPool::ID thread) {
  if (ThreadPool::UI < thread && thread < ThreadPool::ID_COUNT)
    return g_browser_thread_names[thread];
  if (thread == ThreadPool::UI)
    return "Chrome_UIThread";
  return "Unknown Thread";
}

// static
std::string ThreadPool::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  const std::string& message_loop_name =
      base::MessageLoop::current()->thread_name();
  ID actual_browser_thread;
  const char* actual_name = "Unknown Thread";
  if (!message_loop_name.empty()) {
    actual_name = message_loop_name.c_str();
  } else if (GetCurrentThreadIdentifier(&actual_browser_thread)) {
    actual_name = GetThreadName(actual_browser_thread);
  }
  std::string result = "Must be called on ";
  result += GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool ThreadPool::IsMessageLoopValid(ID identifier) {
  if (g_globals == NULL)
    return false;

  ThreadPoolGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return globals.threads[identifier] &&
         globals.threads[identifier]->message_loop();
}

// static
bool ThreadPool::PostTask(ID identifier,
                             const tracked_objects::Location& from_here,
                             const base::Closure& task) {
  return ThreadPoolImpl::PostTaskHelper(
      identifier, from_here, task, base::TimeDelta(), true);
}

// static
bool ThreadPool::PostDelayedTask(ID identifier,
                                    const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  return ThreadPoolImpl::PostTaskHelper(
      identifier, from_here, task, delay, true);
}

// static
bool ThreadPool::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return ThreadPoolImpl::PostTaskHelper(
      identifier, from_here, task, base::TimeDelta(), false);
}

// static
bool ThreadPool::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return ThreadPoolImpl::PostTaskHelper(
      identifier, from_here, task, delay, false);
}

// static
bool ThreadPool::PostTaskAndReply(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    const base::Closure& reply) {
  return GetMessageLoopProxyForThread(identifier)->PostTaskAndReply(from_here,
                                                                    task,
                                                                    reply);
}

// static
bool ThreadPool::GetCurrentThreadIdentifier(ID* identifier) {
  if (g_globals == NULL)
    return false;

  // We shouldn't use MessageLoop::current() since it uses LazyInstance which
  // may be deleted by ~AtExitManager when a WorkerPool thread calls this
  // function.
  // http://crbug.com/63678
  base::ThreadRestrictions::ScopedAllowSingleton allow_singleton;
  base::MessageLoop* cur_message_loop = base::MessageLoop::current();
  ThreadPoolGlobals& globals = g_globals.Get();
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.threads[i] &&
        globals.threads[i]->message_loop() == cur_message_loop) {
      *identifier = globals.threads[i]->identifier_;
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::MessageLoopProxy>
ThreadPool::GetMessageLoopProxyForThread(ID identifier) {
  return g_proxies.Get().proxies[identifier];
}

// static
base::MessageLoop* ThreadPool::UnsafeGetMessageLoopForThread(ID identifier) {
  if (g_globals == NULL)
    return NULL;

  ThreadPoolGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  base::Thread* thread = globals.threads[identifier];
  DCHECK(thread);
  base::MessageLoop* loop = thread->message_loop();
  return loop;
}

// static
void ThreadPool::SetDelegate(ID identifier,
                                ThreadPoolDelegate* delegate) {
  using base::subtle::AtomicWord;
  ThreadPoolGlobals& globals = g_globals.Get();
  AtomicWord* storage = reinterpret_cast<AtomicWord*>(
      &globals.thread_delegates[identifier]);
  AtomicWord old_pointer = base::subtle::NoBarrier_AtomicExchange(
      storage, reinterpret_cast<AtomicWord>(delegate));

  // This catches registration when previously registered.
  DCHECK(!delegate || !old_pointer);
}

/*
  see:
    content/??/browser_main_loop
    content/??/browser_main_runner
    content/??/browser_process_sub_thread
    base/run_loop

    1.MainMessageLoopStart
    2.CreateStartupTasks
    3.RunMainMessageLoopParts
    4.ShutdownThreadsAndCleanUp
*/
static AtExitManager*                exit_manager_;
static scoped_ptr<base::MessageLoop> main_message_loop_;
static scoped_ptr<ThreadPoolImpl> main_thread_;
static scoped_ptr<ThreadPoolImpl> db_thread_;
static scoped_ptr<ThreadPoolImpl> file_user_blocking_thread_;
static scoped_ptr<ThreadPoolImpl> file_thread_;
static scoped_ptr<ThreadPoolImpl> process_launcher_thread_;
static scoped_ptr<ThreadPoolImpl> cache_thread_;
static scoped_ptr<ThreadPoolImpl> io_thread_;
static scoped_ptr<ThreadPoolImpl> compute_thread_;

void ThreadPool::InitializeThreadPool()
{
  exit_manager_ = new AtExitManager();

  if (!base::MessageLoop::current()) {
    main_message_loop_.reset(new base::MessageLoopForUI);
  }
  main_thread_.reset(
      new ThreadPoolImpl(ThreadPool::UI, base::MessageLoop::current()));

  base::Thread::Options default_options;
  base::Thread::Options io_message_loop_options;
  io_message_loop_options.message_loop_type = base::MessageLoop::TYPE_IO;
  base::Thread::Options ui_message_loop_options;
  ui_message_loop_options.message_loop_type = base::MessageLoop::TYPE_UI;

  for (size_t thread_id = ThreadPool::UI + 1;
       thread_id < ThreadPool::ID_COUNT;
       ++thread_id) {
    scoped_ptr<ThreadPoolImpl>* thread_to_start = NULL;
    base::Thread::Options* options = &default_options;

    switch (thread_id) {
      case ThreadPool::DB:
        thread_to_start = &db_thread_;
        break;
      case ThreadPool::FILE_USER_BLOCKING:
        thread_to_start = &file_user_blocking_thread_;
        break;
      case ThreadPool::FILE:
        thread_to_start = &file_thread_;
        options = &ui_message_loop_options;
        break;
      case ThreadPool::PROCESS_LAUNCHER:
        thread_to_start = &process_launcher_thread_;
        break;
      case ThreadPool::CACHE:
        thread_to_start = &cache_thread_;
        options = &io_message_loop_options;
        break;
      case ThreadPool::IO:
        thread_to_start = &io_thread_;
        options = &io_message_loop_options;
        break;
      case ThreadPool::COMPUTE:
        thread_to_start = &compute_thread_;
        options = &ui_message_loop_options;
        break;
      case ThreadPool::UI:
      case ThreadPool::ID_COUNT:
      default:
        NOTREACHED();
        break;
    }

    ThreadPool::ID id = static_cast<ThreadPool::ID>(thread_id);

    if (thread_to_start) {
      (*thread_to_start).reset(new ThreadPoolImpl(id));
      (*thread_to_start)->StartWithOptions(*options);
    } else {
      NOTREACHED();
    }
  }
}

int ThreadPool::RunMainLoop(MessagePumpDispatcher* dispatcher)
{
  base::RunLoop run_loop(dispatcher);
  run_loop.Run();
  return 0;
}

void ThreadPool::DeinitializeThreadPool()
{
  for (size_t thread_id = ThreadPool::ID_COUNT - 1;
       thread_id >= (ThreadPool::UI + 1);
       --thread_id) {

    switch (thread_id) {
      case ThreadPool::DB: {
          db_thread_.reset();
        }
        break;
      case ThreadPool::FILE_USER_BLOCKING: {
          file_user_blocking_thread_.reset();
        }
        break;
      case ThreadPool::FILE: {
          file_thread_.reset();
        }
        break;
      case ThreadPool::PROCESS_LAUNCHER: {
          process_launcher_thread_.reset();
        }
        break;
      case ThreadPool::CACHE: {
          cache_thread_.reset();
        }
        break;
      case ThreadPool::IO: {
          io_thread_.reset();
        }
        break;
      case ThreadPool::COMPUTE: {
          compute_thread_.reset();
        }
        break;
      case ThreadPool::UI:
      case ThreadPool::ID_COUNT:
      default:
        NOTREACHED();
        break;
    }
  }

  main_thread_.reset();
  main_message_loop_.reset();
  delete exit_manager_;
  exit_manager_ = NULL;
}

}  // namespace content
