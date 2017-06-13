#include "dpe_service/main/dpe_model/dpe_device_impl.h"

#include "dpe_service/main/dpe_service.h"

namespace ds
{

DPEDeviceImpl::DPEDeviceImpl(DPEService* dpe) :
  dpe_(dpe),
  device_state_(DPE_DEVICE_IDLE),
  receive_channel_(base::INVALID_CHANNEL_ID),
  send_channel_(base::INVALID_CHANNEL_ID),
  session_(base::StringPrintf("%p", this)),
  weakptr_factory_(this)
{
  
}

DPEDeviceImpl::~DPEDeviceImpl()
{
  CloseDevice();
}

bool DPEDeviceImpl::OpenDevice(int32_t ip)
{
  if (device_state_ != DPE_DEVICE_IDLE)
  {
    LOG(ERROR) << "Open device: device state error";
    return false;
  }

  auto mc = base::zmq_message_center();
  std::list<std::function<void ()> > clean_resource;
  // build receive channel
  {
    for (int i = 0; i < 3; ++i)
    {
      std::string address = base::AddressHelper::MakeZMQTCPAddress(
          ip, base::AddressHelper::GetNextAvailablePort()
        );

      receive_channel_ = mc->RegisterChannel(base::CHANNEL_TYPE_SUB,
        address, true);

      if (receive_channel_ != base::INVALID_CHANNEL_ID)
      {
        receive_address_ = address;
        break;
      }
    }

    if (receive_channel_ == base::INVALID_CHANNEL_ID)
    {
      LOG(ERROR) << "Open device: can not create receive channel";
      return false;
    }

    clean_resource.push_back([&](){
          mc->RemoveChannel(receive_channel_);
          receive_channel_ = base::INVALID_CHANNEL_ID;
        });
  }
  // build send channel
  {
    for (int i = 0; i < 3; ++i)
    {
      std::string address = base::AddressHelper::MakeZMQTCPAddress(
          ip, base::AddressHelper::GetNextAvailablePort()
        );

      send_channel_ = mc->RegisterChannel(base::CHANNEL_TYPE_PUB,
        address, true);

      if (send_channel_ != base::INVALID_CHANNEL_ID)
      {
        send_address_ = address;
        break;
      }
    }

    if (send_channel_ == base::INVALID_CHANNEL_ID)
    {
      LOG(ERROR) << "Open device: can not create send channel";
      for (auto& it: clean_resource) it();
      return false;
    }

    clean_resource.push_back([&](){
          mc->RemoveChannel(send_channel_);
          send_channel_ = base::INVALID_CHANNEL_ID;
        });
  }

  device_state_ = DPE_DEVICE_WAITING;
  
  last_heart_beat_time_ = base::Time::Now();
  ScheduleCheckHeartBeat();
  
  mc->AddMessageHandler((base::MessageHandler*)this);

  return true;
}

bool DPEDeviceImpl::CloseDevice()
{
  if (device_state_ == DPE_DEVICE_IDLE || device_state_ == DPE_DEVICE_CLOSED) return true;
  
  auto mc = base::zmq_message_center();

  mc->RemoveChannel(receive_channel_);
  receive_channel_ = base::INVALID_CHANNEL_ID;

  mc->RemoveChannel(send_channel_);
  send_channel_ = base::INVALID_CHANNEL_ID;

  mc->RemoveMessageHandler((base::MessageHandler*)this);
  
  device_state_ = DPE_DEVICE_CLOSED;
  
  return false;
}

void DPEDeviceImpl::ScheduleCheckHeartBeat(int32_t delay)
{
  if (delay < 0) delay = 0;

  if (delay == 0)
  {
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEDeviceImpl::CheckHeartBeat, weakptr_factory_.GetWeakPtr())
      );
  }
  else
  {
    base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEDeviceImpl::CheckHeartBeat, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay));
  }
}

void DPEDeviceImpl::CheckHeartBeat(base::WeakPtr<DPEDeviceImpl> device)
{
  if (DPEDeviceImpl* pThis = device.get())
  {
    pThis->CheckHeartBeatImpl();
  }
}

void DPEDeviceImpl::CheckHeartBeatImpl()
{
  auto curr = base::Time::Now();
  
  if ((curr-last_heart_beat_time_).InSeconds() > 120)
  {
    LOG(ERROR) << "DPEDeviceImpl : heart beat failed";
    CloseDevice();
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEService::RemoveDPEDevice, base::Unretained(dpe_), static_cast<DPEDevice*>(this))
      );
  }
  else
  {
    ScheduleCheckHeartBeat(10*1000);
  }
}

int32_t DPEDeviceImpl::handle_message(void* handle, const std::string& data)
{
  if (handle != receive_channel_) return 0;

  LOG(INFO) << "DPEDeviceImpl::handle_message:\n" << data;
  base::Value* v = base::JSONReader::Read(data.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);
  do
  {
    if (!v)
    {
      LOG(ERROR) << "\nCan not parse request";
      break;
    }
    
    base::DictionaryValue* dv = NULL;
    if (!v->GetAsDictionary(&dv)) break;
    {
      std::string val;
      
      if (!dv->GetString("type", &val)) break;
      if (val != "rsc") break;
      
      //if (!dv->GetString("src", &val)) break;
      
      //if (!dv->GetString("dest", &val)) break;
      
      if (!dv->GetString("session", &val)) break;
      if (val != session_) break;

      //if (dv->GetString("cookie", &val)) break;
      
      if (!dv->GetString("message", &val)) break;
      
      if (val == "HeartBeat")
      {
        HandleHeartBeatMessage(data, dv);
      }
      else if (val == "InitJob")
      {
        HandleInitJobMessage(data, dv);
      }
      else if (val == "DoTask")
      {
        HandleDoTaskMessage(data, dv);
      }
      else if (val == "CloseDevice")
      {
        HandleCloseDeviceMessage(data, dv);
      }
    }
  } while (false);

  delete v;

  return 1;
}

void DPEDeviceImpl::HandleHeartBeatMessage(const std::string& smsg, base::DictionaryValue* message)
{
  std::string cookie;
  if (!message->GetString("cookie", &cookie)) return;
  last_heart_beat_time_ = base::Time::Now();

  {
    std::string val;
    std::string msg;
    base::DictionaryValue rep;
    rep.SetString("type", "rsc");
    rep.SetString("pa", base::PhysicalAddress());
    rep.SetString("ts", base::StringPrintf("%lld", base::Time::Now().ToInternalValue()));
    rep.SetString("message", "HeartBeat");
    rep.SetString("cookie", cookie);
    rep.SetString("session", session_);
    rep.SetString("error_code", "0");
    base::JSONWriter::Write(&rep, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
  }
}

void DPEDeviceImpl::HandleInitJobMessage(const std::string& smsg, base::DictionaryValue* message)
{
  auto quit_with_state = [&](int32_t state) {
    std::string msg;
    base::DictionaryValue rep;
    rep.SetString("type", "rsc");
    rep.SetString("pa", base::PhysicalAddress());
    rep.SetString("ts", base::StringPrintf("%lld", base::Time::Now().ToInternalValue()));
    rep.SetString("message", "InitJob");
    rep.SetString("error_code", "-1");
    base::JSONWriter::Write(&rep, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
    device_state_ = state;
  };

  if (device_state_ != DPE_DEVICE_WAITING)
  {
    quit_with_state(device_state_);
    return;
  }

  std::string val;
  std::string language;
  std::string compiler_type;
  std::string worker_name;
  std::string data;
  std::string job_name;
  std::string job_id;
  std::string rpa;

  message->GetString("pa", &rpa);
  message->GetString("job_name", &job_name);

  message->GetString("language", &language);
  message->GetString("compiler_type", &compiler_type);
  message->GetString("worker_name", &worker_name);
  message->GetString("data", &data);
  

  if (rpa.empty() || job_name.empty() || worker_name.empty() || data.empty())
  {
    quit_with_state(device_state_);
    return;
  }

  job_home_path_ = home_path_.Append(base::SysUTF8ToWide(job_name+"@"+rpa));
  worker_path_ = job_home_path_.Append(base::SysUTF8ToWide(worker_name));

  base::CreateDirectory(job_home_path_);
  base::WriteFile(worker_path_, data.c_str(), static_cast<int>(data.size()));

  language_ = ProgrammeLanguage::FromUTF8(language);
  compiler_type_ = base::SysUTF8ToWide(compiler_type);

  cj_ = new CompileJob();
  cj_->language_ = language_;
  cj_->compiler_type_ = compiler_type_;
  cj_->current_directory_ = job_home_path_;
  cj_->source_files_.clear();
  cj_->source_files_.push_back(worker_path_);
  cj_->callback_ = this;

  compiler_ = MakeNewCompiler(cj_);

  if (!compiler_)
  {
    LOG(ERROR) << "can not create compiler:";
    quit_with_state(device_state_);
    return;
  }

  if (compiler_->GenerateCmdline(cj_))
  {
    image_path_ =  cj_->image_path_.IsAbsolute() ?
                       cj_->image_path_ :
                       cj_->current_directory_.Append(cj_->image_path_);
    argument_list_ = cj_->arguments_;
  }

  base::FilePath job_desc_path = job_home_path_.Append(L"job_desc.json");
  // todo : make sure we need not compile
  if (base::PathExists(image_path_) && base::PathExists(job_desc_path))
  {
    std::string msg;
    base::DictionaryValue rep;
    rep.SetString("type", "rsc");
    rep.SetString("message", "InitJob");
    rep.SetString("pa", base::PhysicalAddress());
    rep.SetString("ts", base::StringPrintf("%lld", base::Time::Now().ToInternalValue()));
    rep.SetString("error_code", "0");
    device_state_ = DPE_DEVICE_RUNNING_IDLE;
    base::JSONWriter::Write(&rep, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
    return;
  }

  if (!compiler_->StartCompile(cj_))
  {
    LOG(ERROR) << "can not start compile source";
    quit_with_state(device_state_);
    return;
  }

  base::WriteFile(job_desc_path, smsg.c_str(), static_cast<int>(smsg.size()));

  device_state_ = DPE_DEVICE_INITIALIZING;
}

void DPEDeviceImpl::HandleDoTaskMessage(const std::string& smsg, base::DictionaryValue* message)
{
  auto quit_with_state = [&](int32_t state) {
    std::string msg;
    base::DictionaryValue rep;
    rep.SetString("type", "rsc");
    base::AddPaAndTs(&rep);
    rep.SetString("pa", base::PhysicalAddress());
    rep.SetString("ts", base::StringPrintf("%lld", base::Time::Now().ToInternalValue()));
    rep.SetString("message", "DoTask");
    rep.SetString("error_code", "-1");
    base::JSONWriter::Write(&rep, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
    device_state_ = state;
  };

  std::string task_id;
  std::string data;

  if (device_state_ != DPE_DEVICE_RUNNING_IDLE)
  {
    quit_with_state(device_state_);
    return;
  }

  message->GetString("task_id", &task_id);
  message->GetString("task_input", &data);

  worker_process_ = new process::Process(this);
  std::string().swap(task_output_data_);
  auto& po = worker_process_->GetProcessOption();
  
  po.image_path_ = image_path_;
  po.argument_list_ = argument_list_;
  po.current_directory_ = job_home_path_;
  po.redirect_std_inout_ = true;
  po.treat_err_as_out_ = false;

  if (!worker_process_->Start())
  {
    LOG(ERROR) << "can not start worker process";
    quit_with_state(device_state_);
    return;
  }

  auto context = worker_process_->GetProcessContext();

  context->std_in_write_->Write(data.c_str(), static_cast<int>(data.size()));

  task_id_ = task_id;
  device_state_ = DPE_DEVICE_RUNNING;
}

void DPEDeviceImpl::HandleCloseDeviceMessage(const std::string& smsg, base::DictionaryValue* message)
{
  CloseDevice();
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(&DPEService::RemoveDPEDevice, base::Unretained(dpe_), static_cast<DPEDevice*>(this))
    );
}

scoped_refptr<Compiler> DPEDeviceImpl::MakeNewCompiler(CompileJob* job)
{
  scoped_refptr<Compiler> cr =
    dpe_->CreateCompiler(compiler_type_, L"", ARCH_UNKNOWN, language_, job->source_files_);
  return cr;
}

void  DPEDeviceImpl::OnCompileFinished(CompileJob* job)
{
  std::string msg;
  base::DictionaryValue rep;
  rep.SetString("type", "rsc");
  rep.SetString("message", "InitJob");
  base::AddPaAndTs(&rep);
  rep.SetString("error_code", "-1");

  if (cj_->compile_process_->GetProcessContext()->exit_code_ != 0 ||
      cj_->compile_process_->GetProcessContext()->exit_reason_ != process::EXIT_REASON_EXIT)
  {
    LOG(ERROR) << "compile error:\n" << cj_->compiler_output_;
    rep.SetString("error_code", "-1");
    device_state_ = DPE_DEVICE_FAILED;
  }
  else
  {
    image_path_ =  job->image_path_.IsAbsolute() ?
                       job->image_path_ :
                       job->current_directory_.Append(cj_->image_path_);
    argument_list_ = job->arguments_;
    rep.SetString("error_code", "0");
    device_state_ = DPE_DEVICE_RUNNING_IDLE;
  }

  base::JSONWriter::Write(&rep, &msg);
  auto mc = base::zmq_message_center();
  mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
}

void DPEDeviceImpl::OnStop(process::Process* p, process::ProcessContext* context)
{
  std::string msg;
  base::DictionaryValue rep;
  rep.SetString("type", "rsc");
  base::AddPaAndTs(&rep);
  rep.SetString("message", "DoTask");
  rep.SetString("task_id", task_id_);
  rep.SetString("error_code", "-1");

  if (p->GetProcessContext()->exit_code_ != 0 ||
      p->GetProcessContext()->exit_reason_ != process::EXIT_REASON_EXIT)
  {
    LOG(ERROR) << "compile error:\n" << cj_->compiler_output_;
    device_state_ = DPE_DEVICE_RUNNING_IDLE;
  }
  else
  {
    rep.SetString("error_code", "0");
    rep.SetString("task_output", task_output_data_);
    device_state_ = DPE_DEVICE_RUNNING_IDLE;
  }

  base::JSONWriter::Write(&rep, &msg);
  auto mc = base::zmq_message_center();
  mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
}

void DPEDeviceImpl::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (is_std_out)
  {
    task_output_data_.append(data.begin(), data.end());
  }
}

}