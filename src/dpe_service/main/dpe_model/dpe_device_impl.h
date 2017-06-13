#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_DEVICE_IMPL_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_DEVICE_IMPL_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/compiler/compiler.h"
#include "dpe_service/main/dpe_model/dpe_device.h"

namespace ds
{

enum
{
  DPE_DEVICE_IDLE,
  DPE_DEVICE_WAITING,
  DPE_DEVICE_INITIALIZING,
  DPE_DEVICE_RUNNING_IDLE,
  DPE_DEVICE_RUNNING,
  DPE_DEVICE_CLOSED,
  DPE_DEVICE_FAILED,
};

class DPEService;
class DPEDeviceImpl : public DPEDevice, public base::MessageHandler, public CompilerCallback, public process::ProcessHost
{
public:
  DPEDeviceImpl(DPEService* dpe);
  ~DPEDeviceImpl();

  std::string   GetReceiveAddress() override{return receive_address_;}
  std::string   GetSendAddress() override{return send_address_;}
  std::string   GetSession() override{return session_;}
  bool          OpenDevice(int32_t ip) override;
  bool          CloseDevice() override;
  void          SetHomePath(const base::FilePath& path) override{home_path_ = path;}

private:
  void          ScheduleCheckHeartBeat(int32_t delay = 0);
  static void   CheckHeartBeat(base::WeakPtr<DPEDeviceImpl> device);
  void          CheckHeartBeatImpl();
  
  int32_t       handle_message(void* handle, const std::string& data) override;
  
  void          HandleHeartBeatMessage(const std::string& smsg, base::DictionaryValue* message);
  void          HandleInitJobMessage(const std::string& smsg, base::DictionaryValue* message);
  void          HandleDoTaskMessage(const std::string& smsg, base::DictionaryValue* message);
  void          HandleCloseDeviceMessage(const std::string& smsg, base::DictionaryValue* message);

  scoped_refptr<Compiler> MakeNewCompiler(CompileJob* job);
  void          OnCompileFinished(CompileJob* job) override;

  void          OnStop(process::Process* p, process::ProcessContext* context) override;
  void          OnOutput(process::Process* p, bool is_std_out, const std::string& data) override;

private:
  DPEService*   dpe_;
  int32_t       device_state_;
  void*         receive_channel_;
  void*         send_channel_;
  std::string   receive_address_;
  std::string   send_address_;
  std::string   session_;

  base::FilePath home_path_;
  base::FilePath job_home_path_;
  base::FilePath worker_path_;
  ProgrammeLanguage   language_;
  std::wstring   compiler_type_;

  scoped_refptr<CompileJob>       cj_;
  scoped_refptr<Compiler>         compiler_;

  scoped_refptr<process::Process> worker_process_;
  std::string                     task_id_;
  std::string                     task_output_data_;

  base::FilePath                  image_path_;
  std::vector<NativeString>       argument_list_;
  
  base::Time                      last_heart_beat_time_;

  base::WeakPtrFactory<DPEDeviceImpl> weakptr_factory_;
};

}

#endif