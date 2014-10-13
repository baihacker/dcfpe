#include "process/process.h"

#include <process.h>
#include <iostream>
using namespace std;
namespace process{

ULONG_PTR  JOB_COMPLETE_KEY = 0;
ULONG_PTR  STD_OUT_COMPLETE_KEY = 1;
ULONG_PTR  STD_ERR_COMPLETE_KEY = 2;
ULONG_PTR  STD_IN_COMPLETE_KEY = 3;

ProcessOption::ProcessOption() :
  redirect_std_inout_(false),
  treat_err_as_out_(true)
{
}

ProcessOption::~ProcessOption()
{
}

ProcessContext::ProcessContext()
{
  job_ = NULL;
  memset(&si_, 0, sizeof (si_));
  si_.cb = sizeof (si_);
  memset(&job_cp_, 0, sizeof (job_cp_));
  memset(&process_info_, 0, sizeof (process_info_));
  exit_code_ = -1;
  exit_reason_ = EXIT_REASON_UNKNOWN;
}

ProcessContext::~ProcessContext()
{
  DeinitContext();
}

bool ProcessContext::InitContext(ProcessOption& option)
{
  DeinitContext();
  
  job_ = CreateJobObject(NULL, NULL);
  job_cp_.CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
  job_cp_.CompletionKey = reinterpret_cast<void*>(JOB_COMPLETE_KEY);
  SetInformationJobObject(job_, JobObjectAssociateCompletionPortInformation, &job_cp_, sizeof(job_cp_));
  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
  saAttr.bInheritHandle = TRUE; 
  saAttr.lpSecurityDescriptor = NULL; 
  
  if (option.redirect_std_inout_)
  {
    std_out_read_ = new base::PipeServer(base::PIPE_OPEN_MODE_INBOUND, base::PIPE_DATA_BYTE);
    std_out_write_ = std_out_read_->CreateClientAndConnect(true, true);
    
    DCHECK(std_out_read_);
    DCHECK(std_out_write_);
    
    std_in_write_ = new base::PipeServer(base::PIPE_OPEN_MODE_OUTBOUND, base::PIPE_DATA_BYTE);
    std_in_read_ = std_in_write_->CreateClientAndConnect(true, true);
    
    DCHECK(std_in_write_);
    DCHECK(std_in_read_);
    
    si_.hStdOutput = std_out_write_->Handle();
    si_.hStdInput = std_in_read_->Handle();

    if (option.treat_err_as_out_)
    {
      si_.hStdError = std_out_write_->Handle();
    }
    else
    {
      std_err_read_ = new base::PipeServer(base::PIPE_OPEN_MODE_INBOUND, base::PIPE_DATA_BYTE);
      std_err_write_ = std_err_read_->CreateClientAndConnect(true, true);
      
      DCHECK(std_err_read_);
      DCHECK(std_err_write_);
      
      si_.hStdError = std_err_write_->Handle();
    }
    si_.dwFlags |= STARTF_USESTDHANDLES;
    
    if (std_out_read_)
    {
      CreateIoCompletionPort(std_out_read_->Handle(), job_cp_.CompletionPort,
        STD_OUT_COMPLETE_KEY, 1);
    }
    if (std_err_read_)
    {
      CreateIoCompletionPort(std_err_read_->Handle(), job_cp_.CompletionPort,
        STD_ERR_COMPLETE_KEY, 1);
    }
    if (std_in_write_)
    {
      CreateIoCompletionPort(std_in_write_->Handle(), job_cp_.CompletionPort,
        STD_IN_COMPLETE_KEY, 1);
    }
  }
  
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
  memset(&si_, 0, sizeof (si_));
  si_.cb = sizeof (si_);
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
#if 0
ULONG_PTR  STD_OUT_COMPLETE_KEY = 1;
ULONG_PTR  STD_ERR_COMPLETE_KEY = 2;
ULONG_PTR  STD_IN_COMPLETE_KEY = 3;
#endif
unsigned      Process::Run()
{
  ::SetEvent(start_event_);
  
  if (context_->std_out_read_)
  {
    context_->std_out_read_->Read();
  }
  if (context_->std_err_read_) 
  {
    context_->std_err_read_->Read();
  }
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
      if (CompKey == STD_OUT_COMPLETE_KEY ||
          CompKey == STD_ERR_COMPLETE_KEY)
      {
        scoped_refptr<base::PipeServer> p = CompKey == STD_OUT_COMPLETE_KEY ?
                    context_->std_out_read_ : context_->std_err_read_;
        
        if (!p->WaitForPendingIO(0))
        {
          // a sync ReadFile invoke CompletionPort
          break;
        }
        auto& s = p->ReadBuffer();
        int32_t len = p->ReadSize();
        base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
            base::Bind(&Process::OnProcessOutput,
                    weakptr_factory_.GetWeakPtr(),
                    CompKey == STD_OUT_COMPLETE_KEY,
                    std::string(s.begin(), s.begin() + len)
                    )
                 );
        for (;;)
        {
          if (!p->Read()) break;
          if (!p->HasPendingIO())
          {
            break;
          }
          else
          {
            break;
          }
        }
        break;
      }
      if (CompKey == STD_IN_COMPLETE_KEY)
      {
        break;
      }
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
  
  
  context_ = new ProcessContext();
  if (!context_->InitContext(process_option_)) return false;
  
  std::list<std::function<void ()> > clean_resource;
  clean_resource.push_front([&](){context_ = NULL;});

  if (CreateProcess(NULL, (wchar_t*)cmdline.c_str(),
                    NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &context_->si_, &context_->process_info_) == 0)
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

void Process::OnProcessOutput(base::WeakPtr<Process> p, bool is_std_out, const std::string& buffer)
{
  if (Process* pThis = p.get())
  {
    pThis->OnProcessOutputImpl(is_std_out, buffer);
  }
}

void Process::OnProcessOutputImpl(bool is_std_out, const std::string& buffer)
{
  host_->OnOutput(is_std_out, buffer.c_str(), buffer.size());
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