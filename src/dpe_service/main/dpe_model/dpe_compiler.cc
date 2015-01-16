#include "dpe_service/main/dpe_model/dpe_compiler.h"
#include "dpe_service/main/dpe_service.h"
namespace ds
{

DPECompiler::DPECompiler(DPECompilerHost* host, DPEService* dpe)
  : host_(host),
    dpe_(dpe),
    state_(DPE_COMPILE_STATE_IDLE),
    weakptr_factory_(this)
{
}

DPECompiler::~DPECompiler()
{
}

bool DPECompiler::StartCompile(scoped_refptr<DPEProject> dpe_project)
{
  if (state_ != DPE_COMPILE_STATE_IDLE &&
      state_ != DPE_COMPILE_STATE_SUCCESS &&
      state_ != DPE_COMPILE_STATE_EEROR)
  {
    LOG(ERROR) << "DPECompiler : invalid state";
    return false;
  }

  if (!dpe_project)
  {
    LOG(ERROR) << "DPECompiler : null project";
    return false;
  }

  dpe_project_ = dpe_project;

  compile_home_path_ = dpe_project_->job_home_path_.Append(L"running");
  base::CreateDirectory(compile_home_path_);

  source_cj_ = new CompileJob();
  source_cj_->language_ = PL_UNKNOWN;
  source_cj_->compiler_type_ = dpe_project_->compiler_type_;
  source_cj_->current_directory_ = compile_home_path_;
  source_cj_->source_files_.clear();
  source_cj_->source_files_.push_back(dpe_project_->source_path_);
  source_cj_->callback_ = this;
  
  worker_cj_ = new CompileJob();
  worker_cj_->language_ = PL_UNKNOWN;
  worker_cj_->compiler_type_ = dpe_project_->compiler_type_;
  worker_cj_->current_directory_ = compile_home_path_;
  worker_cj_->source_files_.clear();
  worker_cj_->source_files_.push_back(dpe_project_->worker_path_);
  worker_cj_->callback_ = this;

  sink_cj_ = new CompileJob();
  sink_cj_->language_ = PL_UNKNOWN;
  sink_cj_->compiler_type_ = dpe_project_->compiler_type_;
  sink_cj_->current_directory_ = compile_home_path_;
  sink_cj_->source_files_.clear();
  sink_cj_->source_files_.push_back(dpe_project_->sink_path_);
  sink_cj_->callback_ = this;

  compiler_ = MakeNewCompiler(source_cj_);

  if (!compiler_)
  {
    LOG(ERROR) << "DPECompiler : can not create compiler:";
    state_ = DPE_COMPILE_STATE_EEROR;
    return false;
  }

  if (!compiler_->StartCompile(source_cj_))
  {
    LOG(ERROR) << "DPECompiler : can not start compile source";
    state_ = DPE_COMPILE_STATE_EEROR;
    return false;
  }

  state_ = DPE_COMPILE_STATE_COMPILING_SOURCE;
  return true;
}

scoped_refptr<Compiler> DPECompiler::MakeNewCompiler(CompileJob* job)
{
  scoped_refptr<Compiler> cr =
    dpe_->CreateCompiler(
        dpe_project_->compiler_type_, L"", ARCH_UNKNOWN, job->language_, job->source_files_
      );
  return cr;
}

void DPECompiler::OnCompileFinished(CompileJob* job)
{
  if (job->compile_process_)
  {
    if (job->compile_process_->GetProcessContext()->exit_code_ != 0 ||
        job->compile_process_->GetProcessContext()->exit_reason_ != process::EXIT_REASON_EXIT)
    {
      state_ = DPE_COMPILE_STATE_EEROR;
      LOG(ERROR) << "DPECompiler : compile error:\n" << job->compiler_output_;
      host_->OnCompileError();
      return;
    }
  }

  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&DPECompiler::ScheduleNextStep,
            weakptr_factory_.GetWeakPtr()));
}

void  DPECompiler::ScheduleNextStep(base::WeakPtr<DPECompiler> ctrl)
{
  if (DPECompiler* pThis = ctrl.get())
  {
    pThis->ScheduleNextStepImpl();
  }
}

void  DPECompiler::ScheduleNextStepImpl()
{
  switch (state_)
  {
    case DPE_COMPILE_STATE_COMPILING_SOURCE:
    {
      compiler_ = MakeNewCompiler(worker_cj_);

      if (!compiler_->StartCompile(worker_cj_))
      {
        LOG(ERROR) << "DPECompiler : can not start compile worker";
        state_ = DPE_COMPILE_STATE_EEROR;
        break;
      }

      state_ = DPE_COMPILE_STATE_COMPILING_WORKER;
      break;
    }
    case DPE_COMPILE_STATE_COMPILING_WORKER:
    {
      compiler_ = MakeNewCompiler(sink_cj_);

      if (!compiler_->StartCompile(sink_cj_))
      {
        LOG(ERROR) << "DPECompiler : can not start compile sink";
        state_ = DPE_COMPILE_STATE_EEROR;
        break;
      }

      state_ = DPE_COMPILE_STATE_COMPILING_SINK;
      break;
    }
    case DPE_COMPILE_STATE_COMPILING_SINK:
    {
      state_ = DPE_COMPILE_STATE_SUCCESS;
      break;
    }
  }

  if (state_ == DPE_COMPILE_STATE_SUCCESS)
  {
    host_->OnCompileSuccess();
  }
  else if (state_ == DPE_COMPILE_STATE_EEROR)
  {
    host_->OnCompileError();
  }
}

}