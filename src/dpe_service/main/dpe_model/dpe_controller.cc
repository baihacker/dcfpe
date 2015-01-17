#include "dpe_service/main/dpe_model/dpe_controller.h"

#include "dpe_service/main/dpe_service.h"

namespace ds
{
DPEController::DPEController(DPEService* dpe) :
  dpe_(dpe),
  job_state_(DPE_JOB_STATE_PREPARE)
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

  dpe_compiler_ = new DPECompiler(this, dpe_);
  if (dpe_compiler_->StartCompile(dpe_project_))
  {
    job_state_ = DPE_JOB_STATE_COMPILING;
  }
  else
  {
    job_state_ = DPE_JOB_STATE_FAILED;
  }

  return job_state_ == DPE_JOB_STATE_COMPILING;
}

bool  DPEController::Stop()
{
  job_state_ = DPE_JOB_STATE_PREPARE;

  dpe_compiler_ = NULL;
  dpe_scheduler_ = NULL;

  return true;
}

void  DPEController::OnCompileError()
{
  LOG(INFO) << "OnCompileError";
  if (job_state_ == DPE_JOB_STATE_COMPILING)
  {
    job_state_ = DPE_JOB_STATE_FAILED;
    dpe_compiler_ = NULL;
  }
}

void  DPEController::OnCompileSuccess()
{
  LOG(INFO) << "OnCompileSuccess";
  if (job_state_ != DPE_JOB_STATE_COMPILING)
  {
    return;
  }

  dpe_scheduler_ = new DPEScheduler(this);
  if (dpe_scheduler_->Run(servers_, dpe_project_, dpe_compiler_))
  {
    job_state_ = DPE_JOB_STATE_RUNNING;
  }
  else
  {
    job_state_ = DPE_JOB_STATE_FAILED;
    dpe_scheduler_ = NULL;
  }

  dpe_compiler_ = NULL;
}

void  DPEController::OnRunningError()
{
  LOG(INFO) << "OnRunningError";
  if (job_state_ == DPE_JOB_STATE_RUNNING)
  {
    Stop();
    job_state_ = DPE_JOB_STATE_FAILED;
  }
}

void  DPEController::OnRunningSuccess()
{
  LOG(INFO) << "OnRunningSuccess";
  if (job_state_ == DPE_JOB_STATE_RUNNING)
  {
    Stop();
    job_state_ = DPE_JOB_STATE_FINISH;
  }
}

}