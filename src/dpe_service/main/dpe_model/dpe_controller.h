#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_CONTROLLER_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_CONTROLLER_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/compiler/compiler.h"
#include "dpe_service/main/dpe_model/dpe_device.h"
#include "dpe_service/main/zserver_client.h"

namespace ds
{

class DPEController;
class DPEService;
class RemoteDPEDevice;

struct RemoteDPEDeviceCreator : public base::RefCounted<RemoteDPEDeviceCreator>
{
  friend class DPEController;

public:
  RemoteDPEDeviceCreator(DPEController* ctrl);
  ~RemoteDPEDeviceCreator();
  
  bool  RequestNewDevice();
  
private:
  static void  HandleResponse(base::WeakPtr<RemoteDPEDeviceCreator> self,
                scoped_refptr<base::ZMQResponse> rep);
  void  HandleResponseImpl(scoped_refptr<base::ZMQResponse> rep);
  
private:
  DPEController*  ctrl_;
  bool            is_local_;
  std::string     server_address_;
  bool            creating_;
  
  scoped_refptr<ZServerClient>                  zclient_;
  std::vector<scoped_refptr<RemoteDPEDevice> >  device_list_;
  
  base::WeakPtrFactory<RemoteDPEDeviceCreator>  weakptr_factory_;
};

class RemoteDPEDevice : public base::RefCounted<RemoteDPEDevice>, public base::MessageHandler
{
  friend class DPEController;
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
  
  RemoteDPEDevice(DPEController* ctrl);
  ~RemoteDPEDevice();

  bool        Start(const std::string& receive_address,
                    const std::string& send_address, const std::string& session);
  bool        Stop();
  bool        InitJob(const std::string& job_name, const ProgrammeLanguage& language,
                const base::FilePath& source, const std::string& compiler_type);
  bool        DoTask(const std::string& task_id, const std::string& data);
  
private:
  void        ScheduleCheckHeartBeat(int32_t delay = 0);
  static void CheckHeartBeat(base::WeakPtr<RemoteDPEDevice> dev);
  void        CheckHeartBeatImpl();
  
  int32_t     handle_message(int32_t handle, const std::string& data) override;
  
private:
  DPEController* ctrl_;
  int32_t     device_state_;
  
  std::string session_;
  std::string send_address_;
  std::string receive_address_;
  
  int32_t     send_channel_;
  int32_t     receive_channel_;
  
  std::string curr_task_id_;
  std::string curr_task_input_;
  std::string curr_task_output_;
  int32_t     tries_;
  
  int32_t     heart_beat_id_;
  bool        has_reply_;
  base::Time  send_time_;

  base::WeakPtrFactory<RemoteDPEDevice>         weakptr_factory_;
};

enum
{
  DPE_JOB_STATE_PREPARE,
  DPE_JOB_STATE_PREPROCESSING,
  DPE_JOB_STATE_RUNNING,
  DPE_JOB_STATE_FINISH,                 // The same as prepare, but we have output_data
  DPE_JOB_STATE_FAILED,
};

class DPEProject : public base::RefCounted<DPEProject>
{
public:
  static scoped_refptr<DPEProject>  FromFile(const base::FilePath& job_path);

  std::wstring                    job_name_;
  base::FilePath                  job_home_path_;
  base::FilePath                  job_file_path_;

  std::wstring                    compiler_type_;
  ProgrammeLanguage               language_;

  base::FilePath                  source_path_;
  std::string                     source_data_;
  base::FilePath                  worker_path_;
  std::string                     worker_data_;
  base::FilePath                  sink_path_;
  std::string                     sink_data_;
};

class DPEController;
class DPEPreprocessor : public base::RefCounted<DPEPreprocessor>, public CompilerCallback, public process::ProcessHost
{
friend class DPEController;
  enum
  {
    PREPROCESS_STATE_IDLE,
    PREPROCESS_STATE_COMPILING_SOURCE,
    PREPROCESS_STATE_COMPILING_WORKER,
    PREPROCESS_STATE_COMPILING_SINK,
    PREPROCESS_STATE_GENERATING_INPUT,
    PREPROCESS_STATE_SUCCESS,
    PREPROCESS_STATE_EEROR,
  };
public:
  DPEPreprocessor(DPEController* host);
  ~DPEPreprocessor();
  
  bool  StartPreprocess(scoped_refptr<DPEProject> dpe_project);

private:
  scoped_refptr<Compiler> MakeNewCompiler(CompileJob* job);
  void  OnCompileFinished(CompileJob* job) override;

  void  OnStop(process::Process* p, process::ProcessContext* context) override;
  void  OnOutput(process::Process* p, bool is_std_out, const std::string& data) override;

  static void  ScheduleNextStep(base::WeakPtr<DPEPreprocessor> ctrl);
  void  ScheduleNextStepImpl();

private:
  DPEController*                  host_;
  int                             state_;
  scoped_refptr<DPEProject>       dpe_project_;
  base::FilePath                  compile_home_path_;
  
  scoped_refptr<process::Process> source_process_;
  scoped_refptr<process::Process> sink_process_;
  
  std::string                     input_data_;

  scoped_refptr<CompileJob>       cj_;
  scoped_refptr<Compiler>         compiler_;

  base::WeakPtrFactory<DPEPreprocessor>         weakptr_factory_;
};

class DPEController : public ResourceBase, public process::ProcessHost
{
friend class DPEPreprocessor;
public:
  DPEController(DPEService* dpe);
  ~DPEController();

  bool  AddRemoteDPEService(bool is_local, int32_t ip, int32_t port);
  bool  AddRemoteDPEService(bool is_local, const std::string& server_address);
  void  AddRemoteDPEDevice(scoped_refptr<RemoteDPEDevice> device);
  
  bool  Start(const base::FilePath& job_path);

  bool  Stop();
  
  void  OnTaskSucceed(RemoteDPEDevice* device);
  void  OnTaskFailed(RemoteDPEDevice* device);
  void  OnDeviceRunningIdle(RemoteDPEDevice* device);
  void  OnDeviceLose(RemoteDPEDevice* device);
  
private:
  void  OnPreprocessError();
  void  OnPreprocessSuccess();
  
  void  ScheduleRefreshRunningState(int32_t delay = 0);
  static void  RefreshRunningState(base::WeakPtr<DPEController> ctrl);
  void  RefreshRunningStateImpl();
  
  void  OnStop(process::Process* p, process::ProcessContext* context) override;
  void  OnOutput(process::Process* p, bool is_std_out, const std::string& data) override;

private:
  DPEService*                     dpe_;
  
  std::vector<scoped_refptr<RemoteDPEDeviceCreator> >  dpe_list_;
  std::vector<scoped_refptr<RemoteDPEDevice> >   device_list_;
  // dpe job
  scoped_refptr<DPEProject>       dpe_project_;
  scoped_refptr<DPEPreprocessor>  dpe_preprocessor_;
  
  base::FilePath                  output_file_path_;
  base::FilePath                  output_temp_file_path_;

  int32_t                         job_state_;

  scoped_refptr<process::Process> source_process_;
  scoped_refptr<process::Process> sink_process_;
  
  std::string                     output_data_;
  std::string                     input_data_;
  std::vector<std::string>        input_lines_;
  std::queue<int32_t>             task_queue_;
  std::vector<std::string>        output_lines_;

  base::WeakPtrFactory<DPEController>         weakptr_factory_;
};
}
#endif