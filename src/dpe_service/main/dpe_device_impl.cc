#include "dpe_service/main/dpe_device_impl.h"

#include "dpe_service/main/dpe_service.h"

namespace ds
{

DPEDeviceImpl::DPEDeviceImpl(DPEService* dpe) :
  dpe_(dpe),
  device_state_(DPE_DEVICE_IDLE),
  receive_channel_(base::INVALID_CHANNEL_ID),
  send_channel_(base::INVALID_CHANNEL_ID),
  weakptr_factory_(this)
{

}

DPEDeviceImpl::~DPEDeviceImpl()
{
LOG(INFO) << "DPEDeviceImpl::~DPEDeviceImpl";
}

std::string DPEDeviceImpl::GetReceiveAddress()
{
  return receive_address_;
}

std::string DPEDeviceImpl::GetSendAddress()
{
  return send_address_;
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
  base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEDeviceImpl::CheckTimeOut, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(60*1000));

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

  device_state_ = DPE_DEVICE_CLOSED;
  return false;
}

void DPEDeviceImpl::CheckTimeOut(base::WeakPtr<DPEDeviceImpl> device)
{
  if (DPEDeviceImpl* pThis = device.get())
  {
    pThis->CheckTimeOutImpl();
  }
}

void DPEDeviceImpl::CheckTimeOutImpl()
{
  if (device_state_ == DPE_DEVICE_WAITING)
  {
    LOG(ERROR) << "DPEDeviceImpl::CheckTimeOutImpl time out";
    CloseDevice();
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEService::RemoveDPEDevice, base::Unretained(dpe_), static_cast<DPEDevice*>(this))
      );
  }
}

int32_t DPEDeviceImpl::handle_message(int32_t handle, const std::string& data)
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
      
      //if (!dv->GetString("session", &val)) break;
      //if (val != session_) break;

      //if (dv->GetString("cookie", &val)) break;
      
      if (!dv->GetString("message", &val)) break;
      
      if (val == "HeartBeat")
      {
        HandleHeartBeatMessage(dv);
      }
      else if (val == "InitJob")
      {
        HandleInitJobMessage(dv);
      }
      else if (val == "DoTask")
      {
        HandleDoTaskMessage(dv);
      }
    }
  } while (false);

  delete v;

  return 1;
}

void DPEDeviceImpl::HandleHeartBeatMessage(base::DictionaryValue* message)
{

}

void DPEDeviceImpl::HandleInitJobMessage(base::DictionaryValue* message)
{
  std::string val;
  int32_t language = PL_UNKNOWN;
  std::string compiler_type;
  std::string worker_name;
  std::string data;
  std::string job_name;
  message->GetInteger("language", &language);
  message->GetString("compiler_type", &compiler_type);
  message->GetString("worker_name", &worker_name);
  message->GetString("data", &data);
  message->GetString("job_name", &job_name);

  if (job_name.empty() || worker_name.empty() || data.empty())
  {
    goto send_failed_message;
  }

  job_home_path_ = home_path_.Append(base::SysUTF8ToWide(job_name));
  worker_path_ = job_home_path_.Append(base::SysUTF8ToWide(worker_name));

  base::CreateDirectory(job_home_path_);
  base::WriteFile(worker_path_, data.c_str(), data.size());

  language_ = language;
  compiler_type_ = base::SysUTF8ToWide(compiler_type);

  cj_ = new CompileJob();
  cj_->language_ = language_;
  cj_->compiler_type_ = compiler_type_;
  cj_->current_directory_ = job_home_path_;
  cj_->source_files_.clear();
  cj_->source_files_.push_back(worker_path_);
  cj_->output_file_ = base::FilePath(L"worker.exe");
  cj_->callback_ = this;

  compiler_ = MakeNewCompiler(cj_);

  if (!compiler_)
  {
    LOG(ERROR) << "can not create compiler:";
    goto send_failed_message;
  }

  if (!compiler_->StartCompile(cj_))
  {
    LOG(ERROR) << "can not start compile source";
    goto send_failed_message;
  }

  device_state_ = DPE_DEVICE_COMPILING;
  return;
send_failed_message:
  {
    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "InitJob");
    req.SetString("error_code", "-1");
    base::JSONWriter::Write(&req, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());
    device_state_ = DPE_DEVICE_FAILED;
    return;
  }
}

void DPEDeviceImpl::HandleDoTaskMessage(base::DictionaryValue* message)
{
  int32_t task_id;
  std::string data;

  message->GetInteger("task_id", &task_id);
  message->GetString("data", &data);

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
    goto send_failed_message;
  }

  auto context = worker_process_->GetProcessContext();
  //context->std_in_write_->Write("1\n", 2);
  //context->std_in_write_->WaitForPendingIO(-1);

  context->std_in_write_->Write(data.c_str(), data.size());

  task_id_ = task_id;
  device_state_ = DPE_DEVICE_RUNNING;
  return;
send_failed_message:
  {
    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "InitJob");
    req.SetString("error_code", "-1");
    base::JSONWriter::Write(&req, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());
    device_state_ = DPE_DEVICE_FAILED;
    return;
  }
}

scoped_refptr<CompilerResource> DPEDeviceImpl::MakeNewCompiler(CompileJob* job)
{
  scoped_refptr<CompilerResource> cr =
    dpe_->CreateCompiler(compiler_type_, L"", ARCH_UNKNOWN, language_, job->source_files_);
  return cr;
}

void  DPEDeviceImpl::OnCompileFinished(CompileJob* job)
{
  if (cj_->compile_process_->GetProcessContext()->exit_code_ != 0 ||
      cj_->compile_process_->GetProcessContext()->exit_reason_ != process::EXIT_REASON_EXIT)
  {
    device_state_ = DPE_DEVICE_FAILED;
    LOG(ERROR) << "compile error:\n" << cj_->compiler_output_;
    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "InitJob");
    req.SetString("error_code", "-1");
    base::JSONWriter::Write(&req, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());
    return;
  }

  image_path_ =  job->image_path_.IsAbsolute() ?
                       job->image_path_ :
                       job->current_directory_.Append(cj_->image_path_);
  argument_list_ = job->arguments_;

  {
    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "InitJob");
    req.SetString("error_code", "0");
    base::JSONWriter::Write(&req, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());
    device_state_ = DPE_DEVICE_RUNNING_IDLE;
  }
}

void DPEDeviceImpl::OnStop(process::Process* p, process::ProcessContext* context)
{
  if (p->GetProcessContext()->exit_code_ != 0 ||
      p->GetProcessContext()->exit_reason_ != process::EXIT_REASON_EXIT)
  {
    device_state_ = DPE_DEVICE_RUNNING_IDLE;
    LOG(ERROR) << "compile error:\n" << cj_->compiler_output_;
    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "DoTask");
    req.SetString("error_code", "-1");
    base::JSONWriter::Write(&req, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());
    return;
  }

  {
    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "DoTask");
    req.SetString("error_code", "0");
    req.SetInteger("task_id", task_id_);
    req.SetString("task_output", task_output_data_);
    base::JSONWriter::Write(&req, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());
    device_state_ = DPE_DEVICE_RUNNING_IDLE;
    return;
  }
}

void DPEDeviceImpl::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (is_std_out)
  {
    task_output_data_.append(data.begin(), data.end());
  }
}

}