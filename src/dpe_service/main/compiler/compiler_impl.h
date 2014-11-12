#ifndef DPE_SERVICE_MAIN_COMPILER_COMPILER_IMPL_H_
#define DPE_SERVICE_MAIN_COMPILER_COMPILER_IMPL_H_

#include "dpe_service/main/compiler/compiler.h"

namespace ds
{
class BasicCompiler : public Compiler, public process::ProcessHost
{
public:
  BasicCompiler(const CompilerConfiguration& context);
  ~BasicCompiler();
  void OnStop(process::Process* p, process::ProcessContext* exit_code) override;
  void OnOutput(process::Process* p, bool is_std_out, const std::string& data) override;
  ISArch               GetArchitecture() const override{return context_.arch_;}
  LanguageDetail       GetLanguageDetail(const ProgrammeLanguage& language) const;

  bool                  PreProcessJob(CompileJob* job);
  std::string           FixString(const std::string& s, const std::map<std::string, std::string>& kv);
  bool                  StartCompile(CompileJob* job) override;
  bool                  GenerateCmdline(CompileJob* job) override;
  
  static void           ReportCompileSuccess(base::WeakPtr<BasicCompiler> self);
  void                  ScheduleReportCompileSuccess();
  
protected:
  CompileJob*                           curr_job_;
  CompilerConfiguration                 context_;
  scoped_refptr<process::Process>       compile_process_;
  base::WeakPtrFactory<BasicCompiler>   weakptr_factory_;
};

class InterpreterCompiler : public BasicCompiler
{
public:
  InterpreterCompiler(const CompilerConfiguration& context);
  ~InterpreterCompiler();

  bool                StartCompile(CompileJob* job) override;
};


}
#endif