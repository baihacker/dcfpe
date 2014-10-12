// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DPE_BASE_THREAD_POOL_DELEGATE_H_
#define DPE_BASE_THREAD_POOL_DELEGATE_H_

namespace base {

class ThreadPoolDelegate {
 public:
  virtual ~ThreadPoolDelegate() {}

  // Called prior to starting the message loop
  virtual void Init() = 0;

  // Called as the first task on the thread's message loop.
  virtual void InitAsync() = 0;

  // Called just after the message loop ends.
  virtual void CleanUp() = 0;
};

}

#endif
