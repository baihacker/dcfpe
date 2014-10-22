#include "dpe_service/main/dpe_model/dpe_controller.h"

#include "dpe_service/main/dpe_service.h"

namespace ds
{
RemoteDPEService::RemoteDPEService(DPEController* ctrl) :
  ctrl_(ctrl),
  creating_(false)
{

}

RemoteDPEService::~RemoteDPEService()
{

}

bool RemoteDPEService::RequestNewDevice()
{
  if (creating_) return false;

  if (!zclient_) zclient_ = new ZServerClient(this, server_address_);
  if (zclient_->CreateDPEDevice())
  {
    creating_ = true;
    return true;
  }
  
  return false;
}

void RemoteDPEService::HandleResponse(base::ZMQResponse* rep, const std::string& data)
{
  base::Value* v = base::JSONReader::Read(data, base::JSON_ALLOW_TRAILING_COMMAS);

  do
  {
    if (!v)
    {
      LOG(ERROR) << "\nCan not parse response";
      break;
    }
    
    base::DictionaryValue* dv = NULL;
    if (!v->GetAsDictionary(&dv)) break;
    {
      std::string val;
      
      if (!dv->GetString("type", &val)) break;
      if (val != "rsc") break;
      
      if (!dv->GetString("src", &val)) break;
      
      if (!dv->GetString("dest", &val)) break;
      
      if (!dv->GetString("cookie", &val)) ;
      
      if (!dv->GetString("reply", &val)) break;
      
      if (val != "CreateDPEDeviceResponse") break;

      if (!dv->GetString("error_code", &val)) break;
      if (val != "0") break;

      std::string ra;
      std::string sa;

      if (!dv->GetString("receive_address", &ra)) break;
      if (!dv->GetString("send_address", &sa)) break;

      if (ra.empty()) break;
      if (sa.empty()) break;

      scoped_refptr<RemoteDPEDevice> d = new RemoteDPEDevice(ctrl_);
      if (!d->Start(sa, ra))
      {
        LOG(ERROR) << "can not start RemoteDPEDevice";
        break;
      }
      LOG(INFO) << "Create RemoteDPEDevice success";
      ctrl_->AddRemoteDPEDevice(d);
      device_list_.push_back(d);
    }
  } while (false);
  
  delete v;
  
  creating_ = false;
}

RemoteDPEDevice::RemoteDPEDevice(DPEController* ctrl) :
  ctrl_(ctrl),
  device_state_(STATE_IDLE),
  send_channel_(base::INVALID_CHANNEL_ID),
  receive_channel_(base::INVALID_CHANNEL_ID)
{

}

RemoteDPEDevice::~RemoteDPEDevice()
{

}

bool  RemoteDPEDevice::Start(const std::string& receive_address, const std::string& send_address)
{
  if (device_state_ != STATE_IDLE) return false;

  auto mc = base::zmq_message_center();

  send_address_ = send_address;
  receive_address_ = receive_address;

  std::list<std::function<void ()> > clean_resource;
  // build receive channel
  {
    receive_channel_ = mc->RegisterChannel(base::CHANNEL_TYPE_SUB,
        receive_address_, false);

    if (receive_channel_ == base::INVALID_CHANNEL_ID)
    {
      LOG(ERROR) << "Start: can not create receive channel";
      return false;
    }

    clean_resource.push_back([&](){
          mc->RemoveChannel(receive_channel_);
          receive_channel_ = base::INVALID_CHANNEL_ID;
        });
  }
  // build send channel
  {
    send_channel_ = mc->RegisterChannel(base::CHANNEL_TYPE_PUB,
      send_address_, false);

    if (send_channel_ == base::INVALID_CHANNEL_ID)
    {
      LOG(ERROR) << "Start: can not create send channel";
      for (auto& it: clean_resource) it();
      return false;
    }

    clean_resource.push_back([&](){
          mc->RemoveChannel(send_channel_);
          send_channel_ = base::INVALID_CHANNEL_ID;
        });
  }
  device_state_ = STATE_READY;
  return true;
}

bool  RemoteDPEDevice::Stop()
{
  return false;
}

bool RemoteDPEDevice::InitJob(const base::FilePath& source,
              int32_t language, const std::wstring& compiler_type)
{
  LOG(INFO) << "RemoteDPEDevice::InitJob";
  if (device_state_ != STATE_READY)
  {
    LOG(ERROR) << "RemoteDPEDevice::InitJob invalid state";
    return false;
  }

  std::string data;
  if (!base::ReadFileToString(source, &data))
  {
    LOG(ERROR) << "RemoteDPEDevice::InitJob ReadFileToString failed";
    return false;
  }

  std::string msg;
  {
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "InitJob");
    req.SetInteger("language", language);
    req.SetString("compiler_type", base::SysWideToUTF8(compiler_type));
    req.SetString("cookie", "cookie");
    req.SetString("worker_name", base::SysWideToUTF8(source.BaseName().value()));
    req.SetString("job_name", "test_job");
    req.SetString("data", data);
    if (!base::JSONWriter::Write(&req, &msg))
    {
      LOG(ERROR) << "can not write InitJob message";
      return false;
    }
  }

  auto mc = base::zmq_message_center();
  LOG(INFO) << mc->SendMessage(send_channel_, msg.c_str(), msg.size());

  mc->AddMessageHandler((base::MessageHandler*)this);
  device_state_ = STATE_PREPARING;

  return true;
}

bool RemoteDPEDevice::DoTask(const std::string& task_id, const std::string& data)
{
  if (device_state_ != STATE_RUNNING_IDLE) return false;

  std::string msg;
  {
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    req.SetString("message", "DoTask");
    req.SetString("task_id", task_id);
    req.SetString("data", data);
    if (!base::JSONWriter::Write(&req, &msg))
    {
      LOG(ERROR) << "can not write DoTask message";
      return false;
    }
  }

  auto mc = base::zmq_message_center();
  mc->SendMessage(send_channel_, msg.c_str(), msg.size());

  curr_task_id_ = task_id;
  curr_task_input_ = data;
  std::string().swap(curr_task_output_);
  tries_ = 0;
  device_state_ = STATE_RUNNING;
  return true;
}

int32_t RemoteDPEDevice::handle_message(int32_t handle, const std::string& data)
{
  if (handle != receive_channel_) return 0;
  LOG(INFO) << "RemoteDPEDevice::handle_message:\n" << data;
  base::Value* v = base::JSONReader::Read(data.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);

  do
  {
    if (!v)
    {
      LOG(ERROR) << "\nCan not parse response";
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
      
      //if (!dv->GetString("cookie", &val)) ;
      

      if (!dv->GetString("message", &val)) break;
      
      if (val == "InitJob")
      {
        if (!dv->GetString("error_code", &val) || val != "0")
        {
          device_state_ = STATE_FAILED;
          break;
        }
        device_state_ = STATE_RUNNING_IDLE;
      }
      else if (val == "DoTask")
      {
        device_state_ = STATE_RUNNING_IDLE;
        if (!dv->GetString("error_code", &val) || val != "0")
        {
          ctrl_->OnTaskFailed(this);
          break;
        }
        std::string task_id;
        if (!dv->GetString("task_id", &task_id) || task_id != curr_task_id_)
        {
          ctrl_->OnTaskFailed(this);
          break;
        }
        if (!dv->GetString("task_output", &curr_task_output_))
        {
          ctrl_->OnTaskFailed(this);
          break;
        }
        ctrl_->OnTaskSucceed(this);
      }
      
    }
  } while (false);
  
  delete v;

  if (device_state_ == STATE_RUNNING_IDLE)
  {
    ctrl_->OnDeviceRunningIdle(this);
  }
  return 1;
}

DPEController::DPEController(DPEService* dpe) :
  dpe_(dpe),
  language_(PL_UNKNOWN),
  weakptr_factory_(this)
{

}

DPEController::~DPEController()
{

}

bool DPEController::AddRemoteDPEService(bool is_local, int32_t ip, int32_t port)
{
  return AddRemoteDPEService(is_local, base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

bool  DPEController::AddRemoteDPEService(bool is_local, const std::string& server_address)
{
  for (auto it : dpe_list_)
  {
    if (it->server_address_ == server_address)
      return true;
  }

  auto s = new RemoteDPEService(this);
  s->is_local_ = is_local;
  s->server_address_ = server_address;

  dpe_list_.push_back(s);

  return true;
}

void  DPEController::AddRemoteDPEDevice(scoped_refptr<RemoteDPEDevice> device)
{
  if (!device) return;
  device_list_.push_back(device);
  device->InitJob(worker_path_, language_, compiler_type_);
}

bool DPEController::Start()
{
  if (job_state_ == DPE_JOB_STATE_FAILED || DPE_JOB_STATE_FINISH)
    job_state_ = DPE_JOB_STATE_PREPARE;
  if (job_state_ != DPE_JOB_STATE_PREPARE) return false;

  job_home_path_ = home_path_.Append(job_name_);

  if (!base::CreateDirectory(job_home_path_))
  {
    LOG(ERROR) << "can not create job home path:" << base::NativeToUTF8(job_home_path_.value());
    job_state_ = DPE_JOB_STATE_FAILED;
    return false;
  }

  output_file_path_ = job_home_path_.Append(L"output.txt");
  output_temp_file_path_ = job_home_path_.Append(L"output_temp.txt");

  cj_ = new CompileJob();
  cj_->language_ = language_;
  cj_->compiler_type_ = compiler_type_;
  cj_->current_directory_ = job_home_path_;
  cj_->source_files_.clear();
  cj_->source_files_.push_back(source_path_);
  cj_->output_file_ = base::FilePath(L"source.exe");
  cj_->callback_ = this;

  compiler_ = MakeNewCompiler(cj_);

  if (!compiler_)
  {
    LOG(ERROR) << "can not create compiler:";
    job_state_ = DPE_JOB_STATE_FAILED;
    return false;
  }

  if (!compiler_->StartCompile(cj_))
  {
    LOG(ERROR) << "can not start compile source";
    job_state_ = DPE_JOB_STATE_FAILED;
    return false;
  }

  source_process_ = new process::Process(this);
  sink_process_ = new process::Process(this);

  job_state_ = DPE_JOB_STATE_COMPILING_SOURCE;

  return true;
}

void  DPEController::OnTaskSucceed(RemoteDPEDevice* device)
{
  int32_t id = atoi(device->curr_task_id_.c_str());
  
  if (id >= 0 && id < static_cast<int32_t>(output_lines_.size()))
  {
    output_lines_[id] = std::move(device->curr_task_output_);
  }
}

void  DPEController::OnTaskFailed(RemoteDPEDevice* device)
{
  int32_t id = atoi(device->curr_task_id_.c_str());
  
  task_queue_.push(id);
}

void  DPEController::OnDeviceRunningIdle(RemoteDPEDevice* device)
{
  if (!task_queue_.empty())
  {
    int curr = task_queue_.front();
    task_queue_.pop();
    std::string orz = input_lines_[curr];
    orz.append(1, '\n');
    device->DoTask(base::StringPrintf("%d", curr), orz);
  }
  else
  {
    auto context = sink_process_->GetProcessContext();
    for (auto& it : output_lines_)
    {
      context->std_in_write_->Write(it.c_str(), it.size());
      context->std_in_write_->WaitForPendingIO(-1);
    }
  }
}

scoped_refptr<Compiler> DPEController::MakeNewCompiler(CompileJob* job)
{
  scoped_refptr<Compiler> cr =
    dpe_->CreateCompiler(compiler_type_, L"", ARCH_UNKNOWN, language_, job->source_files_);
  return cr;
}

void DPEController::OnCompileFinished(CompileJob* job)
{
  if (cj_->compile_process_->GetProcessContext()->exit_code_ != 0 ||
      cj_->compile_process_->GetProcessContext()->exit_reason_ != process::EXIT_REASON_EXIT)
  {
    job_state_ = DPE_JOB_STATE_FAILED;
    LOG(ERROR) << "compile error:\n" << cj_->compiler_output_;
    return;
  }

  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEController::ScheduleNextStep,
            weakptr_factory_.GetWeakPtr()));
}

void  DPEController::ScheduleNextStep(base::WeakPtr<DPEController> ctrl)
{
  if (DPEController* pThis = ctrl.get())
  {
    pThis->ScheduleNextStepImpl();
  }
}

void  DPEController::ScheduleNextStepImpl()
{
  switch (job_state_)
  {
    case DPE_JOB_STATE_COMPILING_SOURCE:
    {
      auto& po = source_process_->GetProcessOption();

      po.image_path_ = cj_->image_path_.IsAbsolute() ?
                       cj_->image_path_ :
                       cj_->current_directory_.Append(cj_->image_path_);
      po.argument_list_ = cj_->arguments_;
      po.current_directory_ = cj_->current_directory_;
      po.redirect_std_inout_ = true;
      po.treat_err_as_out_ = false;
     // po.output_buffer_size_ = 64*1024;

      cj_->source_files_.clear();
      cj_->source_files_.push_back(worker_path_);
      cj_->output_file_ = base::FilePath(L"worker.exe");

      compiler_ = MakeNewCompiler(cj_);

      if (!compiler_->StartCompile(cj_))
      {
        LOG(ERROR) << "can not start compile worker";
        job_state_ = DPE_JOB_STATE_FAILED;
        return;
      }

      job_state_ = DPE_JOB_STATE_COMPILING_WORKER;
      break;
    }
    case DPE_JOB_STATE_COMPILING_WORKER:
    {
      cj_->source_files_.clear();
      cj_->source_files_.push_back(sink_path_);
      cj_->output_file_ = base::FilePath(L"sink.exe");

      compiler_ = MakeNewCompiler(cj_);

      if (!compiler_->StartCompile(cj_))
      {
        LOG(ERROR) << "can not start compile sink";
        job_state_ = DPE_JOB_STATE_FAILED;
        return;
      }

      job_state_ = DPE_JOB_STATE_COMPILING_SINK;
      break;
    }
    case DPE_JOB_STATE_COMPILING_SINK:
    {
      auto& po = sink_process_->GetProcessOption();

      po.image_path_ = cj_->image_path_.IsAbsolute() ?
                       cj_->image_path_ :
                       cj_->current_directory_.Append(cj_->image_path_);
      po.argument_list_ = cj_->arguments_;
      po.current_directory_ = cj_->current_directory_;
      po.redirect_std_inout_ = true;
      po.treat_err_as_out_ = false;

      std::string().swap(input_data_);
      if (!source_process_->Start())
      {
        LOG(ERROR) << "can not start source process";
        job_state_ = DPE_JOB_STATE_FAILED;
        break;
      }
      job_state_ = DPE_JOB_STATE_GENERATING_INPUT;
      break;
    }
    case DPE_JOB_STATE_GENERATING_INPUT:
    {
      std::vector<std::string>().swap(input_lines_);
      Tokenize(input_data_, "\r\n", &input_lines_);
      
      const int n = input_lines_.size();
      std::queue<int32_t>().swap(task_queue_);
      std::vector<std::string>(n).swap(output_lines_);
      for (int32_t i = 0; i < n; ++i)
      {
        task_queue_.push(i);
      }

      std::string().swap(input_data_);

      if (!sink_process_->Start())
      {
        LOG(ERROR) << "can not start sink process";
        job_state_ = DPE_JOB_STATE_FAILED;
        break;
      }

      base::FilePath output = job_home_path_.Append(L"output_temp.txt");
      base::WriteFile(output, "", 0);
      std::string().swap(output_data_);
      {
        char buff[64];
        sprintf(buff, "%d\n", input_lines_.size());
        auto po = sink_process_->GetProcessContext();
        po->std_in_write_->Write(buff, strlen(buff));
        po->std_in_write_->WaitForPendingIO(-1);
        LOG(INFO) << "sink process ready";
      }
      
      job_state_ = DPE_JOB_STATE_RUNNING;
      ScheduleRefreshRunningState();
    }
  }
}

void  DPEController::ScheduleRefreshRunningState(int32_t delay)
{
  if (delay < 0) delay = 0;
  LOG(INFO) << "ScheduleRefreshRunningState" << delay;
  if (delay == 0)
  {
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEController::RefreshRunningState, weakptr_factory_.GetWeakPtr())
      );
  }
  else
  {
    base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEController::RefreshRunningState, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay));
  }
}

void  DPEController::RefreshRunningState(base::WeakPtr<DPEController> ctrl)
{
  LOG(INFO) << "RefreshRunningState";
  if (DPEController* pThis = ctrl.get())
  {
    pThis->RefreshRunningStateImpl();
  }
}

void  DPEController::RefreshRunningStateImpl()
{
  if (job_state_ != DPE_JOB_STATE_RUNNING) return;

  for (auto it : dpe_list_)
  {
    if (!it->creating_)
    {
      LOG(INFO) << "request new device";
      it->RequestNewDevice();
    }
  }
}

void DPEController::OnStop(process::Process* p, process::ProcessContext* context)
{
  if (p == source_process_)
  {
    if (context->exit_code_ != 0 || context->exit_reason_ != process::EXIT_REASON_EXIT)
    {
      job_state_ = DPE_JOB_STATE_FAILED;
      return;
    }
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEController::ScheduleNextStep,
          weakptr_factory_.GetWeakPtr()));
  }
  else if (p == sink_process_)
  {
    base::WriteFile(output_file_path_, output_data_.c_str(), output_data_.size());
  }
}

void DPEController::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (!is_std_out) return;
  if (p == source_process_)
  {
    input_data_.append(data.begin(), data.end());
  }
  else if (p == sink_process_)
  {
    output_data_.append(data.begin(), data.end());
    base::AppendToFile(output_temp_file_path_, data.c_str(), data.size());
  }
}

}