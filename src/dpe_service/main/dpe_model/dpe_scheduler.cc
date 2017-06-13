#include "dpe_service/main/dpe_model/dpe_scheduler.h"
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
      host_->OnCreateRemoteDPEDeviceSucceed(this, d);
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
    mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
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
  /*
  LOG(INFO) << session_;
  LOG(INFO) << send_address_;
  LOG(INFO) << receive_address_;
  */
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

  device_state_ = STATE_INITIALIZING;
  do_init_job_ = new base::RepeatedAction(this);
  do_init_job_->Start(base::Bind(&RemoteDPEDevice::TrySendInitJobMessage,
    weakptr_factory_.GetWeakPtr(), job_name, language,
    base::SysWideToUTF8(source.BaseName().value()), data, compiler_type),
    base::TimeDelta::FromSeconds(0), base::TimeDelta::FromMilliseconds(100), 15);
  return true;
}

void RemoteDPEDevice::TrySendInitJobMessage(base::WeakPtr<RemoteDPEDevice> self,
              const std::string& job_name, const ProgrammeLanguage& language,
              const std::string& worker_name, const std::string& source_data, const std::string& compiler_type)
{
  if (auto* pThis = self.get())
    pThis->TrySendInitJobMessageImpl(job_name, language, worker_name, source_data, compiler_type);
}

void RemoteDPEDevice::TrySendInitJobMessageImpl(const std::string& job_name, const ProgrammeLanguage& language,
              const std::string& worker_name, const std::string& source_data, const std::string& compiler_type)
{
  if (device_state_ != STATE_INITIALIZING) return;
  
  std::string msg;
  {
    base::DictionaryValue req;
    req.SetString("type", "rsc");
    base::AddPaAndTs(&req);
    req.SetString("message", "InitJob");
    req.SetString("job_name", job_name);
    req.SetString("language", language.ToUTF8());
    req.SetString("compiler_type", compiler_type);
    req.SetString("worker_name", worker_name);

    req.SetString("data", source_data);

    req.SetString("dest", send_address_);
    req.SetString("session", session_);

    if (!base::JSONWriter::Write(&req, &msg))
    {
      LOG(ERROR) << "can not write InitJob message";
      //return false;
    }
  }

  auto mc = base::zmq_message_center();
  auto ret = mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));
}

void  RemoteDPEDevice::OnRepeatedActionFinish(base::RepeatedAction* ra)
{
  if (ra == do_init_job_)
  {
    if (do_init_job_->Interval() < base::TimeDelta::FromSeconds(1))
    {
      do_init_job_->Restart(base::TimeDelta::FromSeconds(0), base::TimeDelta::FromSeconds(10), 6);
    }
    else
    {
      // todo: can not init job, notify manager
      do_init_job_ = NULL;
    }
  }
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
  mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));

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
    mc->SendMessage(send_channel_, msg.c_str(), static_cast<int>(msg.size()));

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

int32_t RemoteDPEDevice::handle_message(void* handle, const std::string& data)
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
        do_init_job_ = NULL;
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

void  RemoteDPEDeviceManager::OnCreateRemoteDPEDeviceSucceed(
    RemoteDPEDeviceCreator* creator, scoped_refptr<RemoteDPEDevice> device)
{
  if (!device) return;
  device_list_.push_back(device);
  //if (host_->job_state_ == DPE_JOB_STATE_RUNNING && host_->dpe_project_)
  {
    device->InitJob(
        base::NativeToUTF8(host_->dpe_project_->job_name_),
        PL_UNKNOWN,
        host_->dpe_project_->worker_path_,
        base::SysWideToUTF8(host_->dpe_project_->compiler_type_)
      );
  }
}

void  RemoteDPEDeviceManager::OnCreateRemoteDPEDeviceFailed(RemoteDPEDeviceCreator* creator)
{
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
}

void  RemoteDPEDeviceManager::OnTaskFailed(RemoteDPEDevice* device)
{
  host_->OnTaskFailed(device);
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

DPEScheduler::DPEScheduler(DPESchedulerHost* host)
  : host_(host),
    weakptr_factory_(this)
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

bool  DPEScheduler::Run(const std::vector<std::pair<bool, std::string> >& servers,
        DPEProject* project, DPECompiler* compiler)
{
  dpe_worker_manager_ = new RemoteDPEDeviceManager(this);
  project_state_ = new DPEProjectState();
  dpe_project_ = project;
  
  // source process
  source_process_ = new process::Process(this);
  {
    auto cj = compiler->SourceCompileJob();
    auto& po = source_process_->GetProcessOption();

    po.image_path_ = cj->image_path_.IsAbsolute() ?
                     cj->image_path_ :
                     cj->current_directory_.Append(cj->image_path_);
    po.argument_list_ = cj->arguments_;
    po.current_directory_ = cj->current_directory_;
    po.redirect_std_inout_ = true;
    po.treat_err_as_out_ = false;
  }
  std::string().swap(input_data_);
  if (!source_process_->Start())
  {
    LOG(ERROR) << "DPEScheduler : can not start source process";
    return false;
  }

  // sink process
  sink_process_ = new process::Process(this);
  {
    auto cj = compiler->SinkCompileJob();
    auto& po = sink_process_->GetProcessOption();

    po.image_path_ = cj->image_path_.IsAbsolute() ?
                     cj->image_path_ :
                     cj->current_directory_.Append(cj->image_path_);
    po.argument_list_ = cj->arguments_;
    po.current_directory_ = cj->current_directory_;
    po.redirect_std_inout_ = true;
    po.treat_err_as_out_ = false;
  }
  
  // workers
  for (auto& iter: servers)
  {
    dpe_worker_manager_->AddRemoteDPEService(iter.first, iter.second);
  }
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
  auto* task = project_state_->GetTask(id, idx);
  if (task)
  {
    task->result_ = std::move(device->curr_task_output_);
    task->state_ = DPEProjectState::TASK_STATE_FINISH;
  }
  running_queue_.erase(id);

  // check finished
  if (task_queue_.empty() && running_queue_.empty())
  {
    WillReduceResult();
  }
  else  if (device->device_state_ == RemoteDPEDevice::STATE_RUNNING_IDLE)
  {
    OnDeviceAvailable();
  }
}

void  DPEScheduler::OnTaskFailed(RemoteDPEDevice* device)
{
  int64_t id = atoi(device->curr_task_id_.c_str());
  int32_t idx = device->curr_task_idx_;

  running_queue_.erase(id);

  // write result
  auto* task = project_state_->GetTask(id, idx);
  if (task)
  {
    task->state_ = DPEProjectState::TASK_STATE_ERROR;
  }

  dpe_project_->SaveProject(project_state_);
  host_->OnRunningError();
}

// state : old state before losing connection
void  DPEScheduler::OnDeviceLose(RemoteDPEDevice* device, int32_t state)
{
  if (state == RemoteDPEDevice::STATE_RUNNING)
  {
    int64_t id = atoi(device->curr_task_id_.c_str());
    int32_t idx = device->curr_task_idx_;

    auto* task = project_state_->GetTask(id, idx);
    if (task)
    {
      task->state_ = DPEProjectState::TASK_STATE_PENDING;
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

    auto* task = project_state_->GetTask(curr.first, curr.second);
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
      task->state_ = DPEProjectState::TASK_STATE_RUNNING;
      running_queue_.insert(curr);
      task_queue_.pop();
    }
  }
}

void DPEScheduler::OnStop(process::Process* p, process::ProcessContext* context)
{
  if (p == source_process_)
  {
    project_state_->AddInputData(input_data_);
    dpe_project_->LoadSavedState(project_state_);
    
    std::map<int64_t, int32_t>().swap(running_queue_);
    if (!project_state_->MakeTaskQueue(task_queue_))
    {
      LOG(ERROR) << "DPEScheduler : cant not make task queue";
      dpe_project_->SaveProject(project_state_);
      host_->OnRunningError();
      return;
    }
    
    output_file_path_ = dpe_project_->job_home_path_.Append(L"running/output.txt");
    output_temp_file_path_ = dpe_project_->job_home_path_.Append(L"running/output_temp.txt");
    if (!sink_process_->Start())
    {
      LOG(ERROR) << "DPEScheduler : can not start sink process";
      dpe_project_->SaveProject(project_state_);
      host_->OnRunningError();
      return;
    }
    
    base::WriteFile(output_temp_file_path_, "", 0);
    std::string().swap(output_data_);
    {
      std::string data = base::StringPrintf("%d\n", static_cast<int>(project_state_->TaskData().size()));
      auto po = sink_process_->GetProcessContext();
      po->std_in_write_->Write(data.c_str(), static_cast<int>(data.size()));
      po->std_in_write_->WaitForPendingIO(-1);
      LOG(INFO) << "DPEScheduler : sink process ready";
    }
    
    if (task_queue_.empty())
    {
      WillReduceResult();
    }
    else
    {
      OnDeviceAvailable();
    }
  }
  else if (p == sink_process_)
  {
    if (context->exit_code_ != 0 || context->exit_reason_ != process::EXIT_REASON_EXIT)
    {
      host_->OnRunningError();
      return;
    }
    base::WriteFile(output_file_path_, output_data_.c_str(), static_cast<int>(output_data_.size()));
    dpe_project_->SaveProject(project_state_);
    host_->OnRunningSuccess();
  }
}

void DPEScheduler::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (!is_std_out) return;
  if (p == source_process_)
  {
    input_data_.append(data.begin(), data.end());
  }
  else if (p == sink_process_)
  {
    output_data_.append(data.begin(), data.end());
    base::AppendToFile(output_temp_file_path_, data.c_str(), static_cast<int>(data.size()));
  }
}

void DPEScheduler::WillReduceResult()
{
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(&DPEScheduler::ReduceResult, weakptr_factory_.GetWeakPtr())
    );
}

void DPEScheduler::ReduceResult(base::WeakPtr<DPEScheduler> self)
{
  if (auto pThis = self.get())
  {
    pThis->ReduceResultImpl();
  }
}

void DPEScheduler::ReduceResultImpl()
{
  if (!task_queue_.empty() || !running_queue_.empty())
  {
    return;
  }
  
  LOG(INFO) << "Begin reduce result";
  auto context = sink_process_->GetProcessContext();
  for (auto& it : project_state_->TaskData())
  {
    auto data = it.result_ + "\n";
    context->std_in_write_->Write(data.c_str(), static_cast<int>(data.size()));
    context->std_in_write_->WaitForPendingIO(-1);
  }
  LOG(INFO) << "Finish reduce result";
}
}