#include "process/process.h"

#include <process.h>

namespace process{

static ULONG_PTR kJobCompleteKey = 0;
static ULONG_PTR kStdOutCompleteKey = 1;
static ULONG_PTR kStdErrCompleteKey = 2;
static ULONG_PTR kStdInCompleteKey = 3;
static ULONG_PTR kForceStopKey = 4;
static ULONG_PTR kTimeOutKey = 5;

ProcessOption::ProcessOption() :
  inherit_env_var_(true),
  show_window_(false),
  redirect_std_inout_(false),
  treat_err_as_out_(true),
  create_sub_process_(true),
  leave_current_job_(true),
  allow_sub_process_breakaway_job_(false),
  output_buffer_size_(0),
  job_time_limit_(0),
  job_memory_limit_(0),
  process_memory_limit_(0),
  output_limit_(0)
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
  output_size_ = 0;
  time_usage_ = 0;
  time_usage_user_ = 0;
  time_usage_kernel_ = 0;
}

ProcessContext::~ProcessContext()
{
  DeinitContext();
}

bool ProcessContext::InitContext(ProcessOption& option)
{
  DeinitContext();
  
  job_ = CreateJobObject(NULL, NULL);
  {
    /*
      1. The sub process created by the destination process will be added into the job since
          we if allow_sub_process_breakaway_job_ = true. Otherwise, we give the permission
          of creating subprocess not in the job to the created process.
    
      2. The sub process will be killed if current process does not exist. (job handled is closed automatic)
      3. Do not display unhandled exception dialogue when exception happens.
    */
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
    jeli.BasicLimitInformation.LimitFlags =
                 JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                 JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    if (option.allow_sub_process_breakaway_job_)
    {
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    }
    if (option.job_time_limit_ > 0)
    {
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_TIME;
      jeli.BasicLimitInformation.PerJobUserTimeLimit.QuadPart = option.job_time_limit_ * 10000LL;
    }
    if (option.job_memory_limit_ > 0)
    {
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
      jeli.JobMemoryLimit = option.job_memory_limit_;
    }
    if (option.process_memory_limit_ > 0)
    {
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
      jeli.ProcessMemoryLimit = option.process_memory_limit_;
    }
    SetInformationJobObject(job_,JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
  }
  {
    /*
      UI restrictions
    */
    JOBOBJECT_BASIC_UI_RESTRICTIONS uir = {0};  
    uir.UIRestrictionsClass = 
                JOB_OBJECT_UILIMIT_EXITWINDOWS |
                JOB_OBJECT_UILIMIT_HANDLES |
                JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                JOB_OBJECT_UILIMIT_DISPLAYSETTINGS |
                JOB_OBJECT_UILIMIT_DESKTOP;
    SetInformationJobObject(job_, JobObjectBasicUIRestrictions, &uir, sizeof(uir));
  }
  {
    /*
      TLE notification
    */
    JOBOBJECT_END_OF_JOB_TIME_INFORMATION jl = {0};
    jl.EndOfJobTimeAction = JOB_OBJECT_POST_AT_END_OF_JOB;
    SetInformationJobObject(job_, JobObjectEndOfJobTimeInformation, &jl, sizeof(jl));
  }
  {
    /*
      Job notifications
    */
    job_cp_.CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    job_cp_.CompletionKey = reinterpret_cast<void*>(kJobCompleteKey);

    SetInformationJobObject(job_, JobObjectAssociateCompletionPortInformation, &job_cp_, sizeof(job_cp_));
  }
  
  if (option.redirect_std_inout_)
  {
    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 
  
    std_out_read_ = new base::PipeServer(
      base::PIPE_OPEN_MODE_INBOUND, base::PIPE_DATA_BYTE, option.output_buffer_size_);
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
      std_err_read_ = new base::PipeServer(
          base::PIPE_OPEN_MODE_INBOUND, base::PIPE_DATA_BYTE, option.output_buffer_size_);
      std_err_write_ = std_err_read_->CreateClientAndConnect(true, true);
      
      DCHECK(std_err_read_);
      DCHECK(std_err_write_);
      
      si_.hStdError = std_err_write_->Handle();
    }
    si_.dwFlags |= STARTF_USESTDHANDLES;
    
    if (std_out_read_)
    {
      CreateIoCompletionPort(std_out_read_->Handle(), job_cp_.CompletionPort,
        kStdOutCompleteKey, 1);
    }
    if (std_err_read_)
    {
      CreateIoCompletionPort(std_err_read_->Handle(), job_cp_.CompletionPort,
        kStdErrCompleteKey, 1);
    }
    if (std_in_write_)
    {
      CreateIoCompletionPort(std_in_write_->Handle(), job_cp_.CompletionPort,
        kStdInCompleteKey, 1);
    }
  }
  
  si_.dwFlags |= STARTF_USESHOWWINDOW;
  si_.wShowWindow = option.show_window_;
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
    ::CloseHandle(process_info_.hProcess);
  }
  memset(&si_, 0, sizeof (si_));
  si_.cb = sizeof (si_);
  memset(&job_cp_, 0, sizeof (job_cp_));
  memset(&process_info_, 0, sizeof(process_info_));
  
  exit_code_ = -1;
  exit_reason_ = EXIT_REASON_UNKNOWN;

  std_out_read_ = NULL;
  std_out_write_ = NULL;
  std_err_read_ = NULL;
  std_err_write_ = NULL;
  std_in_write_ = NULL;
  std_in_read_ = NULL;
  
  output_size_ = 0;
  time_usage_ = 0;
  time_usage_user_ = 0;
  time_usage_kernel_ = 0;
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
  context_ = NULL;
  ::CloseHandle(start_event_);
}

NativeString  Process::MakeCmdLine() const
{
  if (process_option_.image_path_.empty()) return L"";
  
  NativeString cmd = L"\"" + process_option_.image_path_.value() + L"\"";
  
  for (auto& item: process_option_.argument_list_)
  if (!item.empty())
  {
    cmd += L" \"" + item + L"\"";
  }
  
  for (auto& item: process_option_.argument_list_r_)
  if (!item.empty())
  {
    cmd += L" " + item;
  }
  
  return cmd;
}

std::vector<std::pair<NativeString, NativeString> >
Process::CurrentEnvironmentVariable() const
{
  std::vector<std::pair<NativeString, NativeString> > ret;
  wchar_t* v = ::GetEnvironmentStringsW();
  if (!v) return ret;
  
  for (wchar_t* i = v; *i; ++i)
  {
    int32_t len = static_cast<int>(wcslen(i));
    int eq = 0;
    while (eq < len && i[eq] != '=') ++eq;
    if (eq < len)
    {
      ret.push_back({NativeString(i, i+eq), NativeString(i+eq+1, i+len)});
    }
    i += len;
  }
  FreeEnvironmentStrings(v);
  return ret;
}

NativeString Process::MakeEnvironmentVariable()
{
  if (process_option_.inherit_env_var_ &&
      process_option_.env_var_keep_.empty() &&
      process_option_.env_var_merge_.empty() &&
      process_option_.env_var_replace_.empty())
  {
    return L"";
  }

  decltype(process_option_.env_var_keep_) temp;

  if (process_option_.inherit_env_var_)
  {
    auto ce = CurrentEnvironmentVariable();
    for (auto& iter: ce) if (!iter.first.empty()) temp.push_back(iter);
  }
  
  for (auto& iter: process_option_.env_var_keep_)
  {
    bool found = false;
    for (auto& it: temp)
    if (base::StringEqualCaseInsensitive(iter.first, it.first))
    {
      found = true;
      break;
    }
    if (!found)
    {
      temp.push_back(iter);
    }
  }
  
  for (auto& iter: process_option_.env_var_merge_)
  {
    bool found = false;
    for (auto& it: temp)
    if (base::StringEqualCaseInsensitive(iter.first, it.first))
    {
      it.second = iter.second + it.second;
      found = true;
      break;
    }
    if (!found)
    {
      temp.push_back(iter);
    }
  }

  for (auto& iter: process_option_.env_var_replace_)
  {
    bool found = false;
    for (auto& it: temp)
    if (base::StringEqualCaseInsensitive(iter.first, it.first))
    {
      it.second = iter.second;
      found = true;
      break;
    }
    if (!found)
    {
      temp.push_back(iter);
    }
  }

  NativeString ret;
  for (auto& iter: temp)
  {
    if (!ret.empty())
    {
      ret.append(1, L'\0');
    }
    ret.append(iter.first.begin(), iter.first.end());
    ret.append(1, L'=');
    ret.append(iter.second.begin(), iter.second.end());
  }
  ret.append(2, L'\0');
  return ret;
}

unsigned __stdcall Process::ThreadMain(void * arg)
{
  unsigned ret = -1;
  if (Process* pThis = (Process*)arg)
  {
    ret = pThis->Run();
  }
  return ret;
}

static int64_t FileTimeToQuadWord(PFILETIME pft) 
{
  return (Int64ShllMod32(pft->dwHighDateTime, 32) | pft->dwLowDateTime);
}

unsigned  Process::Run()
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
  DWORD start_time = ::GetTickCount();
  for(;;)
  {
    DWORD dwBytes;
    ULONG_PTR CompKey;
    LPOVERLAPPED po;
    GetQueuedCompletionStatus(
      context_->job_cp_.CompletionPort,
        &dwBytes,
        &CompKey,
        &po,
        INFINITE);
    
    bool should_exit = false;

    if (CompKey == kStdOutCompleteKey ||
        CompKey == kStdErrCompleteKey)
    {
      scoped_refptr<base::PipeServer> pio = 
            CompKey == kStdOutCompleteKey ?
              context_->std_out_read_ :
              context_->std_err_read_;
      
      if (!pio->WaitForPendingIO(0))
      {
        // a sync ReadFile invoke CompletionPort
        ;
      }
      else
      {
        auto& s = pio->ReadBuffer();
        int32_t len = static_cast<int>(pio->ReadSize());
        
        if (CompKey == kStdOutCompleteKey)
        {
          context_->output_size_ += len;
        }
        if (process_option_.output_limit_ > 0 &&
          context_->output_size_ > process_option_.output_limit_)
        {
          should_exit = true;
          context_->exit_reason_ = EXIT_REASON_OLE;
        }
        else
        {
          base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
              base::Bind(&Process::OnProcessOutput,
                      weakptr_factory_.GetWeakPtr(),
                      CompKey == kStdOutCompleteKey,
                      std::string(s.begin(), s.begin() + len)
                      )
                   );
          for (;;)
          {
            if (!pio->Read()) break;
            if (!pio->HasPendingIO())
            {
              break;
            }
            else
            {
              break;
            }
          }
        }
      }
    }
    else if (CompKey == kStdInCompleteKey)
    {
      ;
    }
    else if (CompKey == kForceStopKey)
    {
      should_exit = true;
      context_->exit_reason_ = EXIT_REASON_TEIMINATED;
    }
    else if (CompKey == kTimeOutKey)
    {
      should_exit = true;
      context_->exit_reason_ = EXIT_REASON_TLE;
    }
    else if (CompKey == kJobCompleteKey)
    {
      switch (dwBytes)
      {
      case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
        {
          DWORD id = (DWORD)po;
          if (id == context_->process_info_.dwProcessId)
          {
            should_exit = true;
            context_->exit_reason_ = EXIT_REASON_EXCEPTION;
          }
          break;
        }
      case JOB_OBJECT_MSG_EXIT_PROCESS:
        {
          DWORD id = (DWORD)po;
          if (id == context_->process_info_.dwProcessId)
          {
            should_exit = true;
            context_->exit_reason_ = EXIT_REASON_EXIT;
          }
          break;
        }
      case JOB_OBJECT_MSG_NEW_PROCESS:
        {
          if (!process_option_.create_sub_process_)
          {
            should_exit = true;
            context_->exit_reason_ = EXIT_REASON_CREATE_NEW_PROCESS;
          }
          break;
        }
      case JOB_OBJECT_MSG_END_OF_JOB_TIME:
        {
          should_exit = true;
          context_->exit_reason_ = EXIT_REASON_XTLE;
          break;
        }
      case JOB_OBJECT_MSG_JOB_MEMORY_LIMIT:
        {
          should_exit = true;
          context_->exit_reason_ = EXIT_REASON_MLE;
          break;
        }
      case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT:
        {
          should_exit = true;
          context_->exit_reason_ = EXIT_REASON_PMLE;
          break;
        }
      }
    }

    if (should_exit)
    {
      
      ::CloseHandle(context_->job_);
      context_->job_ = NULL;
      ::WaitForSingleObject(context_->process_info_.hProcess, -1);
      {
        context_->time_usage_ = ::GetTickCount() - start_time;
        FILETIME ct, et, kt, ut;
        ::GetProcessTimes(context_->process_info_.hProcess, &ct, &et, &kt, &ut);
        context_->time_usage_user_ = FileTimeToQuadWord(&ut) / 10000;
        context_->time_usage_kernel_ = FileTimeToQuadWord(&kt) / 10000;
      }
      DWORD code = -1;
      if (GetExitCodeProcess(context_->process_info_.hProcess, &code))
      {
        context_->exit_code_ = static_cast<int32_t>(code);
      }
      else
      {
        context_->exit_code_ = -331012005;
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
  NativeString cmdline = MakeCmdLine();
  
  if (cmdline.empty()) return false;
  
  context_ = new ProcessContext();
  if (!context_->InitContext(process_option_)) return false;
  
  std::list<std::function<void ()> > clean_resource;
  clean_resource.push_front([&](){context_ = NULL;});

  DWORD cf = CREATE_SUSPENDED;
  if (process_option_.leave_current_job_)
  {
    // Remove the child process from the job current process associated with if necessary.
    // We always succeed because of the security policy. Unfortunately, it may fail.
    cf |= CREATE_BREAKAWAY_FROM_JOB;
  }

  NativeString ev = MakeEnvironmentVariable();
  NativeString cd = process_option_.current_directory_.value();

  if (!ev.empty())
  {
    cf |= CREATE_UNICODE_ENVIRONMENT;
  }
  context_->cmd_line_ = cmdline;
  if (CreateProcess(NULL,
                    (wchar_t*)cmdline.c_str(),
                    NULL,
                    NULL,
                    // Handle inherit, because we will redirect stdin, stdout, stderr
                    TRUE,
                    cf,
                    ev.empty() ? NULL : (void*)ev.c_str(), 
                    cd.empty() ? NULL : cd.c_str(),
                    &context_->si_,
                    &context_->process_info_) == 0)
  {
    PLOG(ERROR) << "Can not create process";
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
  
  if (process_option_.job_time_limit_ > 0)
  {
    base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&Process::OnProcessTimeOut, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(process_option_.job_time_limit_));
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
  host_->OnStop(this, context_);
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
  host_->OnOutput(this, is_std_out, buffer);
}

void Process::OnProcessTimeOut(base::WeakPtr<Process> p)
{
  if (Process* pThis = p.get())
  {
    pThis->OnProcessTimeOutImpl();
  }
}

void Process::OnProcessTimeOutImpl()
{
  if (process_status_ == PROCESS_STATUS_RUNNING)
  {
    PostQueuedCompletionStatus(
      context_->job_cp_.CompletionPort, 0, kTimeOutKey, NULL);
  }
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
  
  PostQueuedCompletionStatus(
    context_->job_cp_.CompletionPort, 0, kForceStopKey, NULL);
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