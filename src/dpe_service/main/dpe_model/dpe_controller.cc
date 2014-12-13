#include "dpe_service/main/dpe_model/dpe_controller.h"

#include "dpe_service/main/dpe_service.h"

namespace ds
{
RemoteDPEDeviceCreator::RemoteDPEDeviceCreator(RemoteDPEDeviceManager* host) :
  host_(host),
  creating_(false),
  weakptr_factory_(this)
{

}

RemoteDPEDeviceCreator::~RemoteDPEDeviceCreator()
{

}

bool RemoteDPEDeviceCreator::RequestNewDevice()
{
  if (creating_) return false;

  if (!zclient_) zclient_ = new ZServerClient(server_address_);
  if (zclient_->CreateDPEDevice(
      base::Bind(&RemoteDPEDeviceCreator::HandleResponse, weakptr_factory_.GetWeakPtr())
      ))
  {
    creating_ = true;
    return true;
  }

  return false;
}

void  RemoteDPEDeviceCreator::HandleResponse(base::WeakPtr<RemoteDPEDeviceCreator> self,
            scoped_refptr<base::ZMQResponse> rep)
{
  if (RemoteDPEDeviceCreator* pThis = self.get())
  {
    pThis->HandleResponseImpl(rep);
  }
}

void  RemoteDPEDeviceCreator::HandleResponseImpl(scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ != 0)
  {
    LOG(INFO) << "RemoteDPEDeviceCreator : handle response " << rep->error_code_;
    creating_ = false;
    return;
  }

  base::Value* v = base::JSONReader::Read(rep->data_, base::JSON_ALLOW_TRAILING_COMMAS);

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
      std::string cookie;
      if (!dv->GetString("type", &val)) break;
      if (val != "rsc") break;

      if (!dv->GetString("src", &val)) break;

      if (!dv->GetString("dest", &val)) break;

      if (dv->GetString("cookie", &val))
      {
        cookie = std::move(val);
      }

      if (!dv->GetString("reply", &val)) break;

      if (val != "CreateDPEDeviceResponse") break;

      if (!dv->GetString("error_code", &val)) break;
      if (val != "0") break;

      std::string ra;
      std::string sa;
      std::string session;

      if (!dv->GetString("receive_address", &ra)) break;
      if (!dv->GetString("send_address", &sa)) break;
      if (!dv->GetString("session", &session)) break;

      if (ra.empty()) break;
      if (sa.empty()) break;

      scoped_refptr<RemoteDPEDevice> d = new RemoteDPEDevice(host_);

      if (!d->Start(sa, ra, session))
      {
        LOG(ERROR) << "can not start RemoteDPEDevice";
        break;
      }
      LOG(INFO) << "Create RemoteDPEDevice success";
      host_->AddRemoteDPEDevice(d);
      device_list_.push_back(d);
    }
  } while (false);

  delete v;

  creating_ = false;
}

RemoteDPEDevice::RemoteDPEDevice(RemoteDPEDeviceManager* host) :
  host_(host),
  device_state_(STATE_IDLE),
  send_channel_(base::INVALID_CHANNEL_ID),
  receive_channel_(base::INVALID_CHANNEL_ID),
  heart_beat_id_(0),
  has_reply_(true),
  weakptr_factory_(this)
{

}

RemoteDPEDevice::~RemoteDPEDevice()
{
  Stop();
}

bool  RemoteDPEDevice::Start(const std::string& receive_address,
            const std::string& send_address, const std::string& session)
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
  session_ = session;

  mc->AddMessageHandler((base::MessageHandler*)this);
  ScheduleCheckHeartBeat();

  return true;
}

bool  RemoteDPEDevice::Stop()
{
  if (send_channel_ != base::INVALID_CHANNEL_ID)
  {
    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    base::AddPaAndTs(&req);
    req.SetString("message", "CloseDevice");

    req.SetString("dest", send_address_);
    req.SetString("session", session_);

    base::JSONWriter::Write(&req, &msg);
    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());
  }

  auto mc = base::zmq_message_center();

  mc->RemoveChannel(receive_channel_);
  receive_channel_ = base::INVALID_CHANNEL_ID;

  mc->RemoveChannel(send_channel_);
  send_channel_ = base::INVALID_CHANNEL_ID;

  mc->RemoveMessageHandler((base::MessageHandler*)this);

  device_state_ = STATE_IDLE;

  return true;
}

bool RemoteDPEDevice::InitJob(
                const std::string& job_name,
                const ProgrammeLanguage& language,
                const base::FilePath& source,
                const std::string& compiler_type)
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
    base::AddPaAndTs(&req);
    req.SetString("message", "InitJob");
    req.SetString("job_name", job_name);
    req.SetString("language", language.ToUTF8());
    req.SetString("compiler_type", compiler_type);
    req.SetString("worker_name", base::SysWideToUTF8(source.BaseName().value()));

    req.SetString("data", data);

    req.SetString("dest", send_address_);
    req.SetString("session", session_);

    if (!base::JSONWriter::Write(&req, &msg))
    {
      LOG(ERROR) << "can not write InitJob message";
      return false;
    }
  }

  auto mc = base::zmq_message_center();
  mc->SendMessage(send_channel_, msg.c_str(), msg.size());
  device_state_ = STATE_INITIALIZING;

  return true;
}

bool RemoteDPEDevice::DoTask(int64_t task_id, int32_t task_idx, const std::string& data)
{
  if (device_state_ != STATE_RUNNING_IDLE) return false;

  std::string msg;
  {
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    base::AddPaAndTs(&req);
    req.SetString("message", "DoTask");
    req.SetString("task_id", base::StringPrintf("%I64d", task_id));
    req.SetString("task_input", data);

    req.SetString("dest", send_address_);
    req.SetString("session", session_);
    if (!base::JSONWriter::Write(&req, &msg))
    {
      LOG(ERROR) << "can not write DoTask message";
      return false;
    }
  }

  auto mc = base::zmq_message_center();
  mc->SendMessage(send_channel_, msg.c_str(), msg.size());

  curr_task_id_ = base::StringPrintf("%I64d", task_id);
  curr_task_idx_ = task_idx;
  curr_task_input_ = data;
  std::string().swap(curr_task_output_);
  tries_ = 0;
  device_state_ = STATE_RUNNING;
  return true;
}

void RemoteDPEDevice::ScheduleCheckHeartBeat(int32_t delay)
{
  if (delay < 0) delay = 0;

  if (delay == 0)
  {
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&RemoteDPEDevice::CheckHeartBeat, weakptr_factory_.GetWeakPtr())
      );
  }
  else
  {
    base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&RemoteDPEDevice::CheckHeartBeat, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay));
  }
}

void RemoteDPEDevice::CheckHeartBeat(base::WeakPtr<RemoteDPEDevice> self)
{
  if (RemoteDPEDevice* pThis = self.get())
  {
    self->CheckHeartBeatImpl();
  }
}

void RemoteDPEDevice::CheckHeartBeatImpl()
{
  if (has_reply_)
  {
    ++heart_beat_id_;

    std::string msg;
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    base::AddPaAndTs(&req);
    req.SetString("message", "HeartBeat");

    req.SetString("cookie", base::StringPrintf("%d", heart_beat_id_));
    req.SetString("dest", send_address_);
    req.SetString("session", session_);

    if (!base::JSONWriter::Write(&req, &msg))
    {
      LOG(ERROR) << "can not write Heart Beat message";
      return;
    }

    send_time_ = base::Time::Now();
    has_reply_ = false;

    auto mc = base::zmq_message_center();
    mc->SendMessage(send_channel_, msg.c_str(), msg.size());

    ScheduleCheckHeartBeat(10*1000);
  }
  else
  {
    auto curr = base::Time::Now();
    if ((curr - send_time_).InSeconds() > 60)
    {
      LOG(ERROR) << "RemoteDPEDevice : heart beat failed";
      int32_t old_state = device_state_;
      device_state_ = STATE_FAILED;
      host_->OnDeviceLose(this, old_state);
    }
    else
    {
      ScheduleCheckHeartBeat(10*1000);
    }
  }
}

int32_t RemoteDPEDevice::handle_message(int32_t handle, const std::string& data)
{
  if (handle != receive_channel_) return 0;

  if (device_state_ == STATE_FAILED || device_state_ == STATE_IDLE) return 1;

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
          host_->OnInitJobFinish(this);
          break;
        }
        device_state_ = STATE_RUNNING_IDLE;
        host_->OnInitJobFinish(this);
      }
      else if (val == "DoTask")
      {
        device_state_ = STATE_RUNNING_IDLE;
        if (!dv->GetString("error_code", &val) || val != "0")
        {
          host_->OnTaskFailed(this);
          break;
        }
        std::string task_id;
        if (!dv->GetString("task_id", &task_id) || task_id != curr_task_id_)
        {
          host_->OnTaskFailed(this);
          break;
        }
        if (!dv->GetString("task_output", &curr_task_output_))
        {
          host_->OnTaskFailed(this);
          break;
        }
        host_->OnTaskSucceed(this);
      }
      else if (val == "HeartBeat")
      {
        if (!dv->GetString("cookie", &val)) break;
        if (atoi(val.c_str()) == heart_beat_id_)
        {
          has_reply_ = true;
        }
      }
    }
  } while (false);

  delete v;

  return 1;
}

bool  DPEProject::AddInputData(const std::string& input_data)
{
  std::vector<std::string> input_lines;
  Tokenize(input_data, "\r\n", &input_lines);
  
  const int n = input_lines.size();
  std::vector<DPETask>(n).swap(task_data_);
  
  for (int32_t i = 0; i < n; ++i)
  {
    std::string& str = input_lines[i];
    
    const int len  = str.size();
    int pos = 0;
    while (pos < len && str[pos] != ':') ++pos;
    
    if (pos < len)
    {
      task_data_[i].input_ = str.substr(pos+1);
      task_data_[i].id_ = atoll(str.substr(0, pos).c_str());
      task_data_[i].state_ = TASK_STATE_PENDING;
      id2idx_[task_data_[i].id_] = i;
    }
    else
    {
      task_data_[i].state_ = TASK_STATE_ERROR;
    }
  }
  return true;
}

bool  DPEProject::MakeTaskQueue(std::queue<std::pair<int64_t, int32_t> >& task_queue)
{
  std::queue<std::pair<int64_t, int32_t> >().swap(task_queue);

  const int n = task_data_.size();
  for (int32_t i = 0; i < n; ++i)
  {
    if (task_data_[i].state_ == TASK_STATE_PENDING)
    {
      task_queue.push({task_data_[i].id_, i});
    }
  }
  return true;
}

DPEProject::DPETask*  DPEProject::GetTask(int64_t id, int32_t idx)
{
  const int n = task_data_.size();
  if (idx < 0 || idx >= n)
  {
    auto where = id2idx_.find(id);
    if (where != id2idx_.end())
    {
      idx = where->second;
    }
  }
  if (idx < 0 || idx >= n) return NULL;
  if (task_data_[idx].id_ != id) return NULL;

  return &task_data_[idx];
}

scoped_refptr<DPEProject> DPEProject::FromFile(const base::FilePath& job_path)
{
  std::string data;
  if (!base::ReadFileToString(job_path, &data))
  {
    LOG(ERROR) << "DPEProject::Start ReadFileToString failed";
    return NULL;
  }

  base::Value* project = base::JSONReader::Read(data, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!project)
  {
    LOG(ERROR) << "DPEProject::Start ParseProject failed";
    return NULL;
  }

  scoped_refptr<DPEProject> ret = new DPEProject();
  bool ok = false;
  do
  {
    base::DictionaryValue* dv = NULL;
    if (!project->GetAsDictionary(&dv)) break;

    ret->job_home_path_ = job_path.DirName();

    std::string val;
    if (dv->GetString("name", &val))
    {
      ret->job_name_ = base::SysUTF8ToWide(val);
    }
    else
    {
      ret->job_name_ = L"Unknown";
    }

    if (dv->GetString("compiler_type", &val))
    {
      ret->compiler_type_ = base::SysUTF8ToWide(val);
    }

    if (dv->GetString("language", &val))
    {
      ret->language_ = val;
    }
    
    if (dv->GetString("computed_result", &val))
    {
      ret->computed_result_path_ = ret->job_home_path_.Append(base::UTF8ToNative(val));
    }
    else
    {
      ret->computed_result_path_ = ret->job_home_path_.Append(base::FilePath(L"computed_result.txt"));
    }

    if (dv->GetString("computed_result_checksum", &val))
    {
      ret->computed_result_checksum_ = val;
    }

    std::string source;
    std::string worker;
    std::string sink;
    if (!dv->GetString("source", &source))
    {
      LOG(ERROR) << "DPEProject::No source";
      break;
    }
    if (!dv->GetString("worker", &worker))
    {
      LOG(ERROR) << "DPEProject::No worker";
      break;
    }
    if (!dv->GetString("sink", &sink))
    {
      LOG(ERROR) << "DPEProject::No sink";
      break;
    }

    ret->source_path_ = ret->job_home_path_.Append(base::UTF8ToNative(source));
    ret->worker_path_ = ret->job_home_path_.Append(base::UTF8ToNative(worker));
    ret->sink_path_ = ret->job_home_path_.Append(base::UTF8ToNative(sink));

    if (!base::ReadFileToString(ret->source_path_, &ret->source_data_))
    {
      LOG(ERROR) << "DPEProject::ReadFileToString failed: source";
      break;
    }
    if (!base::ReadFileToString(ret->worker_path_, &ret->worker_data_))
    {
      LOG(ERROR) << "DPEProject::ReadFileToString failed: worker";
      break;
    }
    if (!base::ReadFileToString(ret->sink_path_, &ret->sink_data_))
    {
      LOG(ERROR) << "DPEProject::SReadFileToString failed: sink";
      break;
    }

    ok = true;
  } while (false);

  delete project;
  if (!ok) return NULL;
  return ret;
}

DPEPreprocessor::DPEPreprocessor(DPEController* host)
  : host_(host),
    state_(PREPROCESS_STATE_IDLE),
    weakptr_factory_(this)
{
}

DPEPreprocessor::~DPEPreprocessor()
{
}

bool DPEPreprocessor::StartPreprocess(scoped_refptr<DPEProject> dpe_project)
{
  if (state_ != PREPROCESS_STATE_IDLE &&
      state_ != PREPROCESS_STATE_SUCCESS &&
      state_ != PREPROCESS_STATE_EEROR)
  {
    LOG(ERROR) << "DPEPreprocessor : invalid state";
    return false;
  }

  if (!dpe_project)
  {
    LOG(ERROR) << "DPEPreprocessor : null project";
    return false;
  }

  dpe_project_ = dpe_project;

  compile_home_path_ = dpe_project_->job_home_path_.Append(L"running");
  base::CreateDirectory(compile_home_path_);

  cj_ = new CompileJob();
  cj_->language_ = dpe_project_->language_;
  cj_->compiler_type_ = dpe_project_->compiler_type_;
  cj_->current_directory_ = compile_home_path_;
  cj_->source_files_.clear();
  cj_->source_files_.push_back(dpe_project_->source_path_);
  cj_->callback_ = this;

  compiler_ = MakeNewCompiler(cj_);

  if (!compiler_)
  {
    LOG(ERROR) << "DPEPreprocessor : can not create compiler:";
    state_ = PREPROCESS_STATE_EEROR;
    return false;
  }

  if (!compiler_->StartCompile(cj_))
  {
    LOG(ERROR) << "DPEPreprocessor : can not start compile source";
    state_ = PREPROCESS_STATE_EEROR;
    return false;
  }

  source_process_ = new process::Process(this);
  sink_process_ = new process::Process(this);

  state_ = PREPROCESS_STATE_COMPILING_SOURCE;
  return true;
}

scoped_refptr<Compiler> DPEPreprocessor::MakeNewCompiler(CompileJob* job)
{
  scoped_refptr<Compiler> cr =
    host_->dpe_->CreateCompiler(
        dpe_project_->compiler_type_, L"", ARCH_UNKNOWN, dpe_project_->language_, job->source_files_
      );
  return cr;
}

void DPEPreprocessor::OnCompileFinished(CompileJob* job)
{
  if (cj_->compile_process_)
  {
    if (cj_->compile_process_->GetProcessContext()->exit_code_ != 0 ||
        cj_->compile_process_->GetProcessContext()->exit_reason_ != process::EXIT_REASON_EXIT)
    {
      state_ = PREPROCESS_STATE_EEROR;
      LOG(ERROR) << "DPEPreprocessor : compile error:\n" << cj_->compiler_output_;
      host_->OnPreprocessError();
      return;
    }
  }

  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEPreprocessor::ScheduleNextStep,
            weakptr_factory_.GetWeakPtr()));
}

void DPEPreprocessor::OnStop(process::Process* p, process::ProcessContext* context)
{
  if (p == source_process_)
  {
    if (context->exit_code_ != 0 || context->exit_reason_ != process::EXIT_REASON_EXIT)
    {
      state_ = PREPROCESS_STATE_EEROR;
      host_->OnPreprocessError();
      return;
    }
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPEPreprocessor::ScheduleNextStep,
          weakptr_factory_.GetWeakPtr()));
  }
}

void DPEPreprocessor::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (!is_std_out) return;
  if (p == source_process_)
  {
    input_data_.append(data.begin(), data.end());
  }
}

void  DPEPreprocessor::ScheduleNextStep(base::WeakPtr<DPEPreprocessor> ctrl)
{
  if (DPEPreprocessor* pThis = ctrl.get())
  {
    pThis->ScheduleNextStepImpl();
  }
}

void  DPEPreprocessor::ScheduleNextStepImpl()
{
  switch (state_)
  {
    case PREPROCESS_STATE_COMPILING_SOURCE:
    {
      auto& po = source_process_->GetProcessOption();

      po.image_path_ = cj_->image_path_.IsAbsolute() ?
                       cj_->image_path_ :
                       cj_->current_directory_.Append(cj_->image_path_);
      po.argument_list_ = cj_->arguments_;
      po.current_directory_ = cj_->current_directory_;
      po.redirect_std_inout_ = true;
      po.treat_err_as_out_ = false;

      cj_->language_ = dpe_project_->language_;
      cj_->source_files_.clear();
      cj_->source_files_.push_back(dpe_project_->worker_path_);
      cj_->output_file_ = base::FilePath(L"");

      compiler_ = MakeNewCompiler(cj_);

      if (!compiler_->StartCompile(cj_))
      {
        LOG(ERROR) << "DPEPreprocessor : can not start compile worker";
        state_ = PREPROCESS_STATE_EEROR;
        break;
      }

      state_ = PREPROCESS_STATE_COMPILING_WORKER;
      break;
    }
    case PREPROCESS_STATE_COMPILING_WORKER:
    {
      cj_->language_ = dpe_project_->language_;
      cj_->source_files_.clear();
      cj_->source_files_.push_back(dpe_project_->sink_path_);
      cj_->output_file_ = base::FilePath(L"");

      compiler_ = MakeNewCompiler(cj_);

      if (!compiler_->StartCompile(cj_))
      {
        LOG(ERROR) << "DPEPreprocessor : can not start compile sink";
        state_ = PREPROCESS_STATE_EEROR;
        break;
      }

      state_ = PREPROCESS_STATE_COMPILING_SINK;
      break;
    }
    case PREPROCESS_STATE_COMPILING_SINK:
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
        LOG(ERROR) << "DPEPreprocessor : can not start source process";
        state_ = PREPROCESS_STATE_EEROR;
        break;
      }
      state_ = PREPROCESS_STATE_GENERATING_INPUT;
      break;
    }
    case PREPROCESS_STATE_GENERATING_INPUT:
    {
      state_ = PREPROCESS_STATE_SUCCESS;
      break;
    }
  }

  if (state_ == PREPROCESS_STATE_SUCCESS)
  {
    host_->OnPreprocessSuccess();
  }
  else if (state_ == PREPROCESS_STATE_EEROR)
  {
    host_->OnPreprocessError();
  }
}

RemoteDPEDeviceManager::RemoteDPEDeviceManager(DPEScheduler* host)
  : host_(host),
    weakptr_factory_(this)
{
}

RemoteDPEDeviceManager::~RemoteDPEDeviceManager()
{

}

bool  RemoteDPEDeviceManager::AddRemoteDPEService(bool is_local, const std::string& server_address)
{
  for (auto it : dpe_list_)
  {
    if (it->server_address_ == server_address)
      return true;
  }

  auto s = new RemoteDPEDeviceCreator(this);
  s->is_local_ = is_local;
  s->server_address_ = server_address;

  dpe_list_.push_back(s);

  return true;
}

void  RemoteDPEDeviceManager::AddRemoteDPEDevice(scoped_refptr<RemoteDPEDevice> device)
{
  if (!device) return;
  device_list_.push_back(device);
  //if (host_->job_state_ == DPE_JOB_STATE_RUNNING && host_->dpe_project_)
  {
    device->InitJob(
        base::NativeToUTF8(host_->dpe_project_->job_name_),
        host_->dpe_project_->language_,
        host_->dpe_project_->worker_path_,
        base::SysWideToUTF8(host_->dpe_project_->compiler_type_)
      );
  }
}
bool  RemoteDPEDeviceManager::AddTask(int64_t task_id, int32_t task_idx, const std::string& task_input)
{
  for (auto& iter : device_list_)
  {
    if (iter->device_state_ == RemoteDPEDevice::STATE_RUNNING_IDLE)
    {
      iter->DoTask(task_id, task_idx, task_input);
      return true;
    }
  }
  return false;
}

int32_t RemoteDPEDeviceManager::AvailableWorkerCount()
{
  int32_t av = 0;
  for (auto& iter : device_list_)
  {
    if (iter->device_state_ == RemoteDPEDevice::STATE_RUNNING_IDLE)
    {
      ++av;
    }
  }
  return av;
}

RemoteDPEDevice*  RemoteDPEDeviceManager::GetWorker()
{
   for (auto& iter : device_list_)
  {
    if (iter->device_state_ == RemoteDPEDevice::STATE_RUNNING_IDLE)
    {
      return iter;
    }
  }
  return NULL;
}

void  RemoteDPEDeviceManager::OnInitJobFinish(RemoteDPEDevice* device)
{
  if (device->device_state_ == RemoteDPEDevice::STATE_FAILED)
  {
    ;//todo
  }
  else if (device->device_state_ == RemoteDPEDevice::STATE_RUNNING_IDLE)
  {
    host_->OnDeviceAvailable();
  }
}

void  RemoteDPEDeviceManager::OnTaskSucceed(RemoteDPEDevice* device)
{
  host_->OnTaskSucceed(device);
  if (device->device_state_ == RemoteDPEDevice::STATE_RUNNING_IDLE)
  {
    host_->OnDeviceAvailable();
  }
}

void  RemoteDPEDeviceManager::OnTaskFailed(RemoteDPEDevice* device)
{
  host_->OnTaskFailed(device);
  if (device->device_state_ == RemoteDPEDevice::STATE_RUNNING_IDLE)
  {
    host_->OnDeviceAvailable();
  }
}

void  RemoteDPEDeviceManager::OnDeviceLose(RemoteDPEDevice* device, int32_t state)
{
  host_->OnDeviceLose(device, state);

  device->Stop();

  for (auto iter = device_list_.begin(); iter != device_list_.end();)
  {
    if (*iter == device)
    {
      device_list_.erase(iter);
      break;
    }
  }
}

void  RemoteDPEDeviceManager::ScheduleRefreshDeviceState(int32_t delay)
{
  if (delay < 0) delay = 0;

  if (delay == 0)
  {
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&RemoteDPEDeviceManager::RefreshDeviceState, weakptr_factory_.GetWeakPtr())
      );
  }
  else
  {
    base::ThreadPool::PostDelayedTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&RemoteDPEDeviceManager::RefreshDeviceState, weakptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay));
  }
}

void  RemoteDPEDeviceManager::RefreshDeviceState(base::WeakPtr<RemoteDPEDeviceManager> ctrl)
{
  if (RemoteDPEDeviceManager* pThis = ctrl.get())
  {
    pThis->RefreshDeviceStateImpl();
  }
}

void  RemoteDPEDeviceManager::RefreshDeviceStateImpl()
{
  for (auto it : dpe_list_)
  {
    if (!it->creating_ && it->device_list_.empty())
    {
      LOG(INFO) << "DPEController : request new device";
      it->RequestNewDevice();
    }
  }
  ScheduleRefreshDeviceState(1000);
}

DPEScheduler::DPEScheduler(DPEController* host)
  : host_(host)
{

}

DPEScheduler::~DPEScheduler()
{
  Stop();
}

bool  DPEScheduler::AddRemoteDPEService(bool is_local, const std::string& server_address)
{
  return dpe_worker_manager_->AddRemoteDPEService(is_local, server_address);
}

bool  DPEScheduler::Run(DPEProject* project, DPEPreprocessor* processor)
{
  dpe_worker_manager_ = new RemoteDPEDeviceManager(this);
  dpe_project_ = project;
  
  // step 1: prepare data
  if (!dpe_project_->AddInputData(processor->InputData()))
  {
    LOG(ERROR) << "DPEScheduler : cant not add input data";
    return false;
  }

  // step 2: prepare task queue and running queue
  std::map<int64_t, int32_t>().swap(running_queue_);
  if (!dpe_project_->MakeTaskQueue(task_queue_))
  {
    LOG(ERROR) << "DPEScheduler : cant not make task queue";
    return false;
  }

  // step 3: start sink process
  output_file_path_ = dpe_project_->job_home_path_.Append(L"running/output.txt");
  output_temp_file_path_ = dpe_project_->job_home_path_.Append(L"running/output_temp.txt");
  sink_process_ = processor->SinkProcess();
  sink_process_->SetHost(this);
  if (!sink_process_->Start())
  {
    LOG(ERROR) << "DPEScheduler : can not start sink process";
    return false;
  }
  
  base::WriteFile(output_temp_file_path_, "", 0);
  std::string().swap(output_data_);
  {
    std::string data = base::StringPrintf("%d\n", dpe_project_->TaskData().size());
    auto po = sink_process_->GetProcessContext();
    po->std_in_write_->Write(data.c_str(), data.size());
    po->std_in_write_->WaitForPendingIO(-1);
    LOG(INFO) << "DPEScheduler : sink process ready";
  }

  // step 4: refresh device
  dpe_worker_manager_->ScheduleRefreshDeviceState();

  return true;
}

bool  DPEScheduler::Stop()
{
  dpe_project_ = NULL;
  sink_process_ = NULL;
  dpe_worker_manager_ = NULL;
  std::string().swap(output_data_);
  
  std::queue<std::pair<int64_t, int32_t> >().swap(task_queue_);
  std::map<int64_t, int32_t>().swap(running_queue_);
  
  return true;
}

void  DPEScheduler::OnTaskSucceed(RemoteDPEDevice* device)
{
  int64_t id = atoi(device->curr_task_id_.c_str());
  int32_t idx = device->curr_task_idx_;
  
  // write result
  auto* task = dpe_project_->GetTask(id, idx);
  if (task)
  {
    task->result_ = std::move(device->curr_task_output_);
    task->state_ = DPEProject::TASK_STATE_FINISH;
  }
  running_queue_.erase(id);
  
  // check finished
  if (task_queue_.empty() && running_queue_.empty())
  {
    LOG(INFO) << "Begin compute result";
    auto context = sink_process_->GetProcessContext();
    for (auto& it : dpe_project_->TaskData())
    {
      auto data = it.result_ + "\n";
      context->std_in_write_->Write(data.c_str(), data.size());
      context->std_in_write_->WaitForPendingIO(-1);
    }
    LOG(INFO) << "Finish compute result";
  }
}

void  DPEScheduler::OnTaskFailed(RemoteDPEDevice* device)
{
  int64_t id = atoi(device->curr_task_id_.c_str());
  int32_t idx = device->curr_task_idx_;
  
  running_queue_.erase(id);
  
  // write result
  auto* task = dpe_project_->GetTask(id, idx);
  if (task)
  {
    task->state_ = DPEProject::TASK_STATE_ERROR;
  }
  
  host_->OnRunningFailed();
}

void  DPEScheduler::OnDeviceLose(RemoteDPEDevice* device, int32_t state)
{
  if (state == RemoteDPEDevice::STATE_RUNNING)
  {
    int64_t id = atoi(device->curr_task_id_.c_str());
    int32_t idx = device->curr_task_idx_;
    
    auto* task = dpe_project_->GetTask(id, idx);
    if (task)
    {
      task->state_ = DPEProject::TASK_STATE_PENDING;
    }
    
    running_queue_.erase(id);
    
    task_queue_.push({id, idx});
  }
}

void  DPEScheduler::OnDeviceAvailable()
{
  while (!task_queue_.empty() && dpe_worker_manager_->AvailableWorkerCount() > 0)
  {
    auto curr = task_queue_.front();
    
    auto* task = dpe_project_->GetTask(curr.first, curr.second);
    if (!task)
    {
      task_queue_.pop();
      continue;
    }
    
    auto* worker = dpe_worker_manager_->GetWorker();
    if (!worker)
    {
      continue;
    }
    
    std::string task_input = task->input_;
    task_input.append(1, '\n');
    
    if (worker->DoTask(curr.first, curr.second, task_input))
    {
      task->state_ = DPEProject::TASK_STATE_RUNNING;
      running_queue_.insert(curr);
      task_queue_.pop();
    }
  }
}

void DPEScheduler::OnStop(process::Process* p, process::ProcessContext* context)
{
  if (p == sink_process_)
  {
    if (context->exit_code_ != 0 || context->exit_reason_ != process::EXIT_REASON_EXIT)
    {
      host_->OnRunningFailed();
      return;
    }
    std::string temp = std::move(output_data_);
    base::WriteFile(output_file_path_, temp.c_str(), temp.size());
    output_data_ = std::move(temp);
    host_->OnRunningSuccess();
  }
}

void DPEScheduler::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (!is_std_out) return;
  if (p == sink_process_)
  {
    output_data_.append(data.begin(), data.end());
    base::AppendToFile(output_temp_file_path_, data.c_str(), data.size());
  }
}

DPEController::DPEController(DPEService* dpe) :
  dpe_(dpe),
  job_state_(DPE_JOB_STATE_PREPARE),
  weakptr_factory_(this)
{
}

DPEController::~DPEController()
{
  Stop();
}

bool DPEController::AddRemoteDPEService(bool is_local, int32_t ip, int32_t port)
{
  return AddRemoteDPEService(is_local, base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

bool  DPEController::AddRemoteDPEService(bool is_local, const std::string& server_address)
{
  for (auto& iter: servers_)
  {
    if (iter.second == server_address)
    {
      return true;
    }
  }
  
  servers_.emplace_back(is_local, server_address);
  if (dpe_scheduler_)
  {
    dpe_scheduler_->AddRemoteDPEService(is_local, server_address);
  }
  return true;
}

bool DPEController::Start(const base::FilePath& job_path)
{
  if (job_state_ == DPE_JOB_STATE_FAILED ||
      job_state_ == DPE_JOB_STATE_FINISH)
    job_state_ = DPE_JOB_STATE_PREPARE;
  if (job_state_ != DPE_JOB_STATE_PREPARE) return false;

  dpe_project_ = DPEProject::FromFile(job_path);
  if (!dpe_project_)
  {
    LOG(ERROR) << "DPEController: Can not load dpe project";
    return false;
  }

  dpe_preprocessor_ = new DPEPreprocessor(this);
  if (dpe_preprocessor_->StartPreprocess(dpe_project_))
  {
    job_state_ = DPE_JOB_STATE_PREPROCESSING;
  }
  else
  {
    job_state_ = DPE_JOB_STATE_FAILED;
  }

  return job_state_ == DPE_JOB_STATE_PREPROCESSING;
}

bool  DPEController::Stop()
{
  job_state_ = DPE_JOB_STATE_PREPARE;

  dpe_preprocessor_ = NULL;
  dpe_scheduler_ = NULL;

  return true;
}

void  DPEController::OnPreprocessError()
{
  LOG(INFO) << "OnPreprocessError";
  if (job_state_ == DPE_JOB_STATE_PREPROCESSING)
  {
    job_state_ = DPE_JOB_STATE_FAILED;
    dpe_preprocessor_ = NULL;
  }
}

void  DPEController::OnPreprocessSuccess()
{
  LOG(INFO) << "OnPreprocessSuccess";
  if (job_state_ != DPE_JOB_STATE_PREPROCESSING)
  {
    return;
  }
  do
  {
    dpe_scheduler_ = new DPEScheduler(this);
    if (dpe_scheduler_->Run(dpe_project_, dpe_preprocessor_))
    {
      job_state_ = DPE_JOB_STATE_RUNNING;
      for (auto& iter: servers_)
      {
        dpe_scheduler_->AddRemoteDPEService(iter.first, iter.second);
      }
    }
    else
    {
      job_state_ = DPE_JOB_STATE_FAILED;
      dpe_scheduler_ = NULL;
    }
  } while (false);

  dpe_preprocessor_ = NULL;
}

void  DPEController::OnRunningFailed()
{
  if (job_state_ == DPE_JOB_STATE_RUNNING)
  {
    Stop();
    job_state_ = DPE_JOB_STATE_FAILED;
  }
}

void  DPEController::OnRunningSuccess()
{
  if (job_state_ == DPE_JOB_STATE_RUNNING)
  {
    Stop();
    job_state_ = DPE_JOB_STATE_FINISH;
  }
}

}