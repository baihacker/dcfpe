#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_COMPILER_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_COMPILER_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/dpe_model/dpe_project.h"
#include "dpe_service/main/compiler/compiler.h"

namespace ds
{
class DPEService;
class DPECompilerHost
{
public:
virtual ~DPECompilerHost(){}
virtual void  OnCompileError() = 0;
virtual void  OnCompileSuccess() = 0;
};

class DPECompiler :
    public base::RefCounted<DPECompiler>,
    public CompilerCallback
{
  enum
  {
    DPE_COMPILE_STATE_IDLE,
    DPE_COMPILE_STATE_COMPILING_SOURCE,
    DPE_COMPILE_STATE_COMPILING_WORKER,
    DPE_COMPILE_STATE_COMPILING_SINK,
    DPE_COMPILE_STATE_SUCCESS,
    DPE_COMPILE_STATE_EEROR,
  };
public:
  DPECompiler(DPECompilerHost* host, DPEService* dpe);
  ~DPECompiler();

  bool  StartCompile(scoped_refptr<DPEProject> dpe_project);

  scoped_refptr<CompileJob> SourceCompileJob(){return source_cj_;}
  scoped_refptr<CompileJob> WorkerCompileJob(){return worker_cj_;}
  scoped_refptr<CompileJob> SinkCompileJob(){return sink_cj_;}

private:
  scoped_refptr<Compiler> MakeNewCompiler(CompileJob* job);
  void  OnCompileFinished(CompileJob* job) override;

  static void  ScheduleNextStep(base::WeakPtr<DPECompiler> ctrl);
  void  ScheduleNextStepImpl();

private:
  DPECompilerHost*                              host_;
  DPEService*                                   dpe_;
  int                                           state_;
  scoped_refptr<DPEProject>                     dpe_project_;
  base::FilePath                                compile_home_path_;

  scoped_refptr<CompileJob>                     source_cj_;
  scoped_refptr<CompileJob>                     worker_cj_;
  scoped_refptr<CompileJob>                     sink_cj_;

  scoped_refptr<Compiler>                       compiler_;

  base::WeakPtrFactory<DPECompiler>             weakptr_factory_;
};
}
#endif