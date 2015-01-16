#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_CONTROLLER_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_CONTROLLER_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/dpe_model/dpe_device.h"
#include "dpe_service/main/dpe_model/dpe_project.h"
#include "dpe_service/main/dpe_model/dpe_compiler.h"
#include "dpe_service/main/zserver_client.h"

namespace ds
{

class DPEController;
class DPEService;
class RemoteDPEDevice;
class RemoteDPEDeviceManager;
class DPEScheduler;

struct RemoteDPEDeviceCreator : public base::RefCounted<RemoteDPEDeviceCreator>
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
  RemoteDPEDeviceManager*  host_;
  bool            is_local_;
  std::string     server_address_;
  bool            creating_;

  scoped_refptr<ZServerClient>                  zclient_;
  std::vector<scoped_refptr<RemoteDPEDevice> >  device_list_;

  base::WeakPtrFactory<RemoteDPEDeviceCreator>  weakptr_factory_;
};

class RemoteDPEDevice : public base::RefCounted<RemoteDPEDevice>, public base::MessageHandler
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
  bool        DoTask(int64_t task_id, int32_t task_idx, const std::string& data);

private:
  void        ScheduleCheckHeartBeat(int32_t delay = 0);
  static void CheckHeartBeat(base::WeakPtr<RemoteDPEDevice> dev);
  void        CheckHeartBeatImpl();

  int32_t     handle_message(int32_t handle, const std::string& data) override;

private:
  RemoteDPEDeviceManager*                 host_;
  int32_t                                 device_state_;

  std::string                             session_;
  std::string                             send_address_;
  std::string                             receive_address_;

  int32_t                                 send_channel_;
  int32_t                                 receive_channel_;

  std::string                             curr_task_id_;
  int32_t                                 curr_task_idx_;
  std::string                             curr_task_input_;
  std::string                             curr_task_output_;
  int32_t                                 tries_;

  int32_t                                 heart_beat_id_;
  bool                                    has_reply_;
  base::Time                              send_time_;

  base::WeakPtrFactory<RemoteDPEDevice>   weakptr_factory_;
};

enum
{
  DPE_JOB_STATE_PREPARE,
  DPE_JOB_STATE_COMPILING,
  DPE_JOB_STATE_RUNNING,
  DPE_JOB_STATE_FINISH,                 // The same as prepare, but we have output_data
  DPE_JOB_STATE_FAILED,
};

class RemoteDPEDeviceManager : public base::RefCounted<RemoteDPEDeviceManager>
{
public:
  RemoteDPEDeviceManager(DPEScheduler* host);
  ~RemoteDPEDeviceManager();

  bool  AddRemoteDPEService(bool is_local, const std::string& server_address);
  void  AddRemoteDPEDevice(scoped_refptr<RemoteDPEDevice> device);

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

class DPEScheduler : public base::RefCounted<DPEScheduler>, public process::ProcessHost
{
  friend class RemoteDPEDeviceManager;
  friend class DPEController;
public:
  DPEScheduler(DPEController* host);
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

  void  SaveState(){if (dpe_project_ && project_state_) dpe_project_->SaveProject(project_state_);}
private:
  void  WillReduceResult();
  static void ReduceResult(base::WeakPtr<DPEScheduler> self);
  void  ReduceResultImpl();
  
private:
  DPEController*                                host_;
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

class DPEController : public ResourceBase, public DPECompilerHost
{
  friend class DPECompiler;
  friend class DPEScheduler;
public:
  DPEController(DPEService* dpe);
  ~DPEController();

  bool  AddRemoteDPEService(bool is_local, int32_t ip, int32_t port);
  bool  AddRemoteDPEService(bool is_local, const std::string& server_address);

  bool  Start(const base::FilePath& job_path);

  bool  Stop();

private:
  void  OnCompileError();
  void  OnCompileSuccess();
  void  OnRunningFailed();
  void  OnRunningSuccess();

private:
  DPEService*                                   dpe_;

  std::vector<std::pair<bool, std::string> >    servers_;
  scoped_refptr<DPEProject>                     dpe_project_;
  scoped_refptr<DPECompiler>                    dpe_compiler_;
  scoped_refptr<DPEScheduler>                   dpe_scheduler_;

  int32_t                                       job_state_;

  base::WeakPtrFactory<DPEController>           weakptr_factory_;
};
}
#endif