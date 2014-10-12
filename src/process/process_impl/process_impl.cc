#include "process/process.h"

#include <process.h>

namespace process{
ProcessContext::ProcessContext()
{
  job_ = NULL;
  memset(&job_cp_, 0, sizeof (job_cp_));
  memset(&process_info_, 0, sizeof (process_info_));
  exit_code_ = -1;
  exit_reason_ = EXIT_REASON_UNKNOWN;
}

ProcessContext::~ProcessContext()
{
  DeinitContext();
}

bool ProcessContext::InitContext()
{
  DeinitContext();
  
  job_ = CreateJobObject(NULL, NULL);
  job_cp_.CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  job_cp_.CompletionKey = 0;
  SetInformationJobObject(job_, JobObjectAssociateCompletionPortInformation, &job_cp_, sizeof(job_cp_));
  
  return true;
}

bool ProcessContext::DeinitContext()
{
  if (job_)
  {
    ::CloseHandle(job_);
    job_ = NULL;
  }
  if (job_cp_.CompletionPort)
  {
    ::CloseHandle(job_cp_.CompletionPort);
  }
  if (process_info_.hThread)
  {
    ::CloseHandle(process_info_.hThread);
  }
  if (process_info_.hProcess)
  {
    ::CloseHandle(process_info_.hThread);
  }
  memset(&job_cp_, 0, sizeof (job_cp_));
  memset(&process_info_, 0, sizeof(process_info_));
  exit_code_ = -1;
  exit_reason_ = EXIT_REASON_UNKNOWN;
  
  return true;
}

Process::Process(ProcessHost* host) :
  process_status_(PROCESS_STATUS_PREPARE),
  host_(host),
  thread_handle_(NULL),
  weakptr_factory_(this)
{
  DCHECK(host_);

  start_event_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
}

Process::~Process()
{
  Stop();
  ::CloseHandle(start_event_);
}

std::wstring  Process::MakeCmdLine()
{
  if (process_option_.image_path_.empty()) return L"";
  
  std::wstring cmd = L"\"" + process_option_.image_path_ + L"\"";
  
  if (!process_option_.argument_.empty())
  {
    cmd += L" " + process_option_.argument_;
  }
  
  for (auto& item: process_option_.argument_list_)
  {
    cmd += L" \"" + item + L"\"";
  }
  
  return cmd;
}

unsigned __stdcall Process::ThreadMain(void * arg)
{
  if (Process* pThis = (Process*)arg)
  {
    pThis->Run();
  }
  return 0;
}

unsigned      Process::Run()
{
  ::SetEvent(start_event_);
  ResumeThread(context_->process_info_.hThread);
  for(;;)
  {
    DWORD dwBytes;
    ULONG_PTR CompKey;
    LPOVERLAPPED po;
    GetQueuedCompletionStatus(context_->job_cp_.CompletionPort, &dwBytes, &CompKey, &po, INFINITE);
    
    bool should_terminate_process = false;
    bool should_exit = false;
    do
    {
      if (dwBytes == 0xffffffff)
      {
        should_terminate_process = true;
        context_->exit_reason_ = EXIT_REASON_TEIMINATED;    // todo: more reason details
        break;
      }
      switch (dwBytes)
      {
      case JOB_OBJECT_MSG_EXIT_PROCESS:
        {
          should_exit = true;
          context_->exit_reason_ = EXIT_REASON_EXIT;
          break;
        }
      case  JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
        {
          should_terminate_process = true;
          context_->exit_reason_ = EXIT_REASON_EXCEPTION;
          break;
        }
      }
    } while (false);
    
    if (should_terminate_process)
    {
       ::TerminateProcess(context_->process_info_.hProcess, -1);
       ::WaitForSingleObject(context_->process_info_.hProcess, 200);
       context_->exit_code_ = -1;
       
    }
    
    if (should_terminate_process || should_exit)
    {
      DWORD code = -1;
      if (GetExitCodeProcess(context_->process_info_.hProcess, &code))
      {
        context_->exit_code_ = static_cast<int32_t>(code);
      }
      else
      {
        context_->exit_code_ = -1;
      }
      break;
    }
  }
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(&Process::OnThreadExit, weakptr_factory_.GetWeakPtr()));
  return 0;
}

bool Process::Start()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  if (process_status_ != PROCESS_STATUS_PREPARE)
  {
    return false;
  }
  std::wstring cmdline = MakeCmdLine();
  
  if (cmdline.empty()) return false;
  
  std::list<std::function<void ()> > clean_resource;
  context_ = new ProcessContext();
  if (!context_->InitContext()) return false;
  clean_resource.push_front([&](){context_ = NULL;});

  STARTUPINFO si = { sizeof(si) };
  if (CreateProcess(NULL, (wchar_t*)cmdline.c_str(),
                    NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &context_->process_info_) == 0)
  {
    for (auto& it: clean_resource) it();
    return false;
  }

  AssignProcessToJobObject(context_->job_, context_->process_info_.hProcess);

  ::ResetEvent(start_event_);
  unsigned id = 0;
  thread_handle_ = (HANDLE)_beginthreadex(NULL, 0,
      &Process::ThreadMain, (void*)this, 0, &id);
  if (!thread_handle_)
  {
    for (auto& it: clean_resource) it();
    return false;
  }
  clean_resource.push_front(
    [&](){
      ::CloseHandle(thread_handle_);
      thread_handle_ = NULL;
    }
  );

  HANDLE handles[] = {thread_handle_, start_event_};
  DWORD result = ::WaitForMultipleObjects(2, handles, FALSE, -1);
  if (result != WAIT_OBJECT_0 + 1)
  {
    if (result != WAIT_OBJECT_0)
    {
      ::TerminateThread(thread_handle_, -1);
    }
    for (auto& it: clean_resource) it();
    return false;
  }
  
  process_status_ = PROCESS_STATUS_RUNNING;
  
  return true;
}

void   Process::OnThreadExit(base::WeakPtr<Process> p)
{
  if (Process* pThis = p.get())
  {
    pThis->OnThreadExitImpl();
  }
}

void   Process::OnThreadExitImpl()
{
  DWORD result = ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 100);
  
  if (result == WAIT_TIMEOUT)
  {
    ::TerminateThread(thread_handle_, -1);
    ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 100);
  }
  else if (result == WAIT_OBJECT_0)
  {
    //
  }
  
  ::CloseHandle(thread_handle_);
  thread_handle_ = NULL;
  process_status_ = PROCESS_STATUS_STOPPED;
  
  // todo notify host
  host_->OnStop(context_);
}

bool Process::Stop()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  if (process_status_ == PROCESS_STATUS_STOPPED)
  {
    return true;
  }
  if (process_status_ == PROCESS_STATUS_PREPARE)
  {
    return false;
  }
  
  PostQueuedCompletionStatus(context_->job_cp_.CompletionPort, 0xffffffff, NULL, NULL);
  DWORD result = ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);
  
  if (result == WAIT_TIMEOUT)
  {
    ::TerminateThread(thread_handle_, -1);
    ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);
    context_->exit_code_ = -1;
  }
  else if (result == WAIT_OBJECT_0)
  {
    //
  }
  
  ::CloseHandle(thread_handle_);
  thread_handle_ = NULL;

  process_status_ = PROCESS_STATUS_STOPPED;
  
  return true;
}

ProcessContext* Process::GetProcessContext()
{
  if (process_status_ == PROCESS_STATUS_PREPARE)
  {
    return NULL;
  }
  return context_;
}

}