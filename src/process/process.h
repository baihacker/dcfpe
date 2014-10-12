#ifndef PROCESS_PROCESS_H
#define PROCESS_PROCESS_H

#include "dpe_base/dpe_base.h"

#include <vector>
#include <string>

#include <windows.h>
/*
  Process is owned by its host. Process has a background thread
    which will receive message from completion port
*/
namespace process{

const int IPC_PORT = 10231;
const int MAX_IP_ADDRESS = 127 << 24 | 255 << 16 | 255 << 8 | 254;  // 127.255.255.254
enum
{
  PROCESS_STATUS_PREPARE = 1,
  PROCESS_STATUS_RUNNING = 2,
  PROCESS_STATUS_STOPPED = 3,
};

enum
{
  EXIT_REASON_UNKNOWN = -1,
  EXIT_REASON_EXIT = 0,
  EXIT_REASON_TEIMINATED = 1,
  EXIT_REASON_EXCEPTION = 2,
  EXIT_REASON_TLE = 3,
  EXIT_REASON_MLE = 4,
  EXIT_REASON_OLE = 5,
};

struct ProcessOption
{
  // cmd = "image_path_" argument "argument_list_[0]" "argument_list_[1]" ...
  std::wstring                          image_path_;
  std::wstring                          argument_;
  std::vector<std::wstring>             argument_list_;
};

class ProcessContext : public base::RefCounted<ProcessContext>
{
public:
  ProcessContext();
  ~ProcessContext();
  bool InitContext();
  bool DeinitContext();
  
  PROCESS_INFORMATION                   process_info_;
  JOBOBJECT_ASSOCIATE_COMPLETION_PORT   job_cp_;
  HANDLE                                job_;
  int32_t                               exit_code_;
  int32_t                               exit_reason_;
};

class ProcessHost
{
public:
  virtual ~ProcessHost(){};
  virtual void OnStop(ProcessContext* exit_code) = 0;
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