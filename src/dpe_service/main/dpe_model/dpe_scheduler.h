#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_SCHEDULER_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_SCHEDULER_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/zserver_client.h"
#include "dpe_service/main/dpe_model/dpe_device.h"
#include "dpe_service/main/dpe_model/dpe_project.h"
#include "dpe_service/main/dpe_model/dpe_compiler.h"

namespace ds
{
enum
{
  DPE_JOB_STATE_PREPARE,
  DPE_JOB_STATE_COMPILING,
  DPE_JOB_STATE_COMPILED,
  DPE_JOB_STATE_COMPILE_ERROR,
  DPE_JOB_STATE_RUNNING,
  DPE_JOB_STATE_FINISH,                 // The same as prepare, but we have output_data
  DPE_JOB_STATE_FAILED,
};

class DPEService;
class RemoteDPEDevice;
class RemoteDPEDeviceManager;
class DPEProject;
class DPECompiler;

struct RemoteDPEDeviceCreator :
  public base::RefCounted<RemoteDPEDeviceCreator>
{
  friend class RemoteDPEDeviceManager;

public:
  RemoteDPEDeviceCreator(RemoteDPEDeviceManager* host);
  ~RemoteDPEDeviceCreator();

  bool  RequestNewDevice();

private:
  static void  HandleResponse(base::WeakPtr<RemoteDPEDeviceCreator> self,
                scoped_refptr<base::ZMQResponse> rep);
  void  HandleResponseImpl(scoped_refptr<base::ZMQResponse> rep);

private:
  RemoteDPEDeviceManager*                       host_;
  bool                                          is_local_;
  std::string                                   server_address_;
  bool                                          creating_;

  scoped_refptr<ZServerClient>                  zclient_;
  std::vector<scoped_refptr<RemoteDPEDevice> >  device_list_;

  base::WeakPtrFactory<RemoteDPEDeviceCreator>  weakptr_factory_;
};

class RemoteDPEDevice :
  public base::RefCounted<RemoteDPEDevice>,
  public base::MessageHandler,
  public base::RepeatedActionHost
{
  friend class RemoteDPEDeviceManager;
  friend class DPEScheduler;
public:
  enum
  {
    STATE_IDLE,
    STATE_READY,
    STATE_INITIALIZING,
    STATE_RUNNING_IDLE,
    STATE_RUNNING,
    STATE_FAILED,
  };

  RemoteDPEDevice(RemoteDPEDeviceManager* host);
  ~RemoteDPEDevice();

  bool        Start(const std::string& receive_address,
                    const std::string& send_address, const std::string& session);
  bool        Stop();
  bool        InitJob(const std::string& job_name, const ProgrammeLanguage& language,
                const base::FilePath& source, const std::string& compiler_type);
  static void TrySendInitJobMessage(base::WeakPtr<RemoteDPEDevice> self,
                const std::string& job_name, const ProgrammeLanguage& language,
                const std::string& worker_name, const std::string& source_data, const std::string& compiler_type);
  void        TrySendInitJobMessageImpl(const std::string& job_name, const ProgrammeLanguage& language,
                const std::string& worker_name, const std::string& source_data, const std::string& compiler_type);
  void        OnRepeatedActionFinish(base::RepeatedAction* ra);
  bool        DoTask(int64_t task_id, int32_t task_idx, const std::string& data);

private:
  void        ScheduleCheckHeartBeat(int32_t delay = 0);
  static void CheckHeartBeat(base::WeakPtr<RemoteDPEDevice> dev);
  void        CheckHeartBeatImpl();

  int32_t     handle_message(void* handle, const std::string& data) override;

private:
  RemoteDPEDeviceManager*                 host_;
  int32_t                                 device_state_;

  std::string                             session_;
  std::string                             send_address_;
  std::string                             receive_address_;

  void*                                   send_channel_;
  void*                                   receive_channel_;

  std::string                             curr_task_id_;
  int32_t                                 curr_task_idx_;
  std::string                             curr_task_input_;
  std::string                             curr_task_output_;
  int32_t                                 tries_;

  int32_t                                 heart_beat_id_;
  bool                                    has_reply_;
  base::Time                              send_time_;

  scoped_refptr<base::RepeatedAction>     do_init_job_;
  
  base::WeakPtrFactory<RemoteDPEDevice>   weakptr_factory_;
};

class RemoteDPEDeviceManager :
  public base::RefCounted<RemoteDPEDeviceManager>
{
public:
  RemoteDPEDeviceManager(DPEScheduler* host);
  ~RemoteDPEDeviceManager();

  bool  AddRemoteDPEService(bool is_local, const std::string& server_address);
  void  OnCreateRemoteDPEDeviceSucceed(RemoteDPEDeviceCreator* creator, scoped_refptr<RemoteDPEDevice> device);
  void  OnCreateRemoteDPEDeviceFailed(RemoteDPEDeviceCreator* creator);

  bool  AddTask(int64_t task_id, int32_t task_idx, const std::string& task_input);
  int32_t AvailableWorkerCount();
  RemoteDPEDevice*  GetWorker();

  void  OnInitJobFinish(RemoteDPEDevice* device);
  void  OnTaskSucceed(RemoteDPEDevice* device);
  void  OnTaskFailed(RemoteDPEDevice* device);
  void  OnDeviceLose(RemoteDPEDevice* device, int32_t state);

  void  ScheduleRefreshDeviceState(int32_t delay = 0);
  static void  RefreshDeviceState(base::WeakPtr<RemoteDPEDeviceManager> ctrl);
  void  RefreshDeviceStateImpl();

private:
  DPEScheduler*                                         host_;
  std::vector<scoped_refptr<RemoteDPEDeviceCreator> >   dpe_list_;
  std::vector<scoped_refptr<RemoteDPEDevice> >          device_list_;
  base::WeakPtrFactory<RemoteDPEDeviceManager>          weakptr_factory_;
};

class DPESchedulerHost
{
public:
virtual ~DPESchedulerHost(){}
virtual void  OnRunningError() = 0;
virtual void  OnRunningSuccess() = 0;
};

class DPEScheduler :
  public base::RefCounted<DPEScheduler>,
  public process::ProcessHost
{
  friend class RemoteDPEDeviceManager;
public:
  DPEScheduler(DPESchedulerHost* host);
  ~DPEScheduler();

  bool  AddRemoteDPEService(bool is_local, const std::string& server_address);

  bool  Run(const std::vector<std::pair<bool, std::string> >& servers,
            DPEProject* project, DPECompiler* compiler);
  bool  Stop();

  void  OnTaskSucceed(RemoteDPEDevice* device);
  void  OnTaskFailed(RemoteDPEDevice* device);
  void  OnDeviceLose(RemoteDPEDevice* device, int32_t state);
  void  OnDeviceAvailable();

  void  OnStop(process::Process* p, process::ProcessContext* context) override;
  void  OnOutput(process::Process* p, bool is_std_out, const std::string& data) override;

private:
  void  WillReduceResult();
  static void ReduceResult(base::WeakPtr<DPEScheduler> self);
  void  ReduceResultImpl();
  
private:
  DPESchedulerHost*                             host_;
  scoped_refptr<DPEProject>                     dpe_project_;
  scoped_refptr<DPEProjectState>                project_state_;
  scoped_refptr<RemoteDPEDeviceManager>         dpe_worker_manager_;

  scoped_refptr<process::Process>               source_process_;
  std::string                                   input_data_;

  std::queue<std::pair<int64_t, int32_t> >      task_queue_;
  std::map<int64_t, int32_t>                    running_queue_;


  scoped_refptr<process::Process>               sink_process_;
  base::FilePath                                output_file_path_;
  base::FilePath                                output_temp_file_path_;
  std::string                                   output_data_;
  
  base::WeakPtrFactory<DPEScheduler>            weakptr_factory_;
};
}
#endif