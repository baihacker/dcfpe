#ifndef PROCESS_PROCESS_H
#define PROCESS_PROCESS_H

#include <vector>
#include <string>

#include <windows.h>

#include "dpe_base/dpe_base.h"

/*
  Process is owned by its host. Process has a background thread
    which will receive message from completion port
*/
namespace process{

enum
{
  PROCESS_STATUS_PREPARE = 1,
  PROCESS_STATUS_RUNNING = 2,
  PROCESS_STATUS_STOPPED = 3,
};

enum
{
  EXIT_REASON_UNKNOWN             = -1,
  EXIT_REASON_EXIT                = 0,
  EXIT_REASON_TEIMINATED          = 1,
  EXIT_REASON_EXCEPTION           = 2,
  EXIT_REASON_TLE                 = 3,    // General time out,
  EXIT_REASON_XTLE                = 4,    // User time time out
  EXIT_REASON_MLE                 = 5,    // job mle
  EXIT_REASON_PMLE                = 6,    // process mle
  EXIT_REASON_OLE                 = 7,
  EXIT_REASON_CREATE_NEW_PROCESS  = 8,
};

struct ProcessOption
{
  ProcessOption();
  ~ProcessOption();
  std::wstring                    image_path_;
  // cmd = "image_path_" argument "argument_list_[0]" "argument_list_[1]" ...
  std::wstring                    argument_;
  std::vector<std::wstring>       argument_list_;
  std::wstring                    current_directory_;

  bool                            redirect_std_inout_;
  bool                            treat_err_as_out_;
  bool                            create_sub_process_;
  
  int32_t                         output_buffer_size_;    // Byte
  int64_t                         job_time_limit_;        // ms, job time limits
  int64_t                         job_memory_limit_;      // Byte
  int64_t                         process_memory_limit_;  // Byte
  int64_t                         output_limit_;          // Byte, check stdout, redirect_std_inout_ == true
};

class ProcessContext : public base::RefCounted<ProcessContext>
{
public:
  ProcessContext();
  ~ProcessContext();
  bool InitContext(ProcessOption& option);
  bool DeinitContext();
  
  STARTUPINFO                           si_;
  PROCESS_INFORMATION                   process_info_;
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT   job_cp_;
  HANDLE                                job_;
  int32_t                               exit_code_;
  int32_t                               exit_reason_;
  
  scoped_refptr<base::PipeServer>       std_out_read_;
  scoped_refptr<base::IOHandler>        std_out_write_;
  scoped_refptr<base::PipeServer>       std_err_read_;
  scoped_refptr<base::IOHandler>        std_err_write_;
  scoped_refptr<base::PipeServer>       std_in_write_;
  scoped_refptr<base::IOHandler>        std_in_read_;
  
  int64_t                               output_size_;
  int64_t                               time_usage_;
  int64_t                               time_usage_user_;
  int64_t                               time_usage_kernel_;
};

class ProcessHost
{
public:
  virtual ~ProcessHost(){};
  virtual void OnStop(ProcessContext* exit_code) = 0;
  virtual void OnOutput(bool is_std_out, const char* buffer, int32_t size) = 0;
};

class Process : public base::RefCounted<Process>
{
public:
  Process(ProcessHost* host);
  ~Process();

  virtual bool Start();
  virtual bool Stop();

  ProcessOption&  GetProcessOption() {return process_option_;}
  ProcessContext* GetProcessContext();
  
private:
  std::wstring  MakeCmdLine();
  static unsigned __stdcall ThreadMain(void * arg);
  unsigned      Run();
  
  static void   OnThreadExit(base::WeakPtr<Process> p);
  void          OnThreadExitImpl();
  static void   OnProcessOutput(base::WeakPtr<Process> p, bool is_std_out, const std::string& buffer);
  void          OnProcessOutputImpl(bool is_std_out, const std::string& buffer);
  static void   OnProcessTimeOut(base::WeakPtr<Process> p);
  void          OnProcessTimeOutImpl();

private:
  ProcessHost*                          host_;
  int32_t                               process_status_;

  // configuration of process
  ProcessOption                         process_option_;
  
  // available when running or stopped
  scoped_refptr<ProcessContext>         context_;
  
  HANDLE                                start_event_;
  HANDLE                                thread_handle_;

  base::WeakPtrFactory<Process>         weakptr_factory_;
};

}

#endif