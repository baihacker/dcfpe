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
protected:
  CompileJob*         curr_job_;
  CompilerConfiguration   context_;
  scoped_refptr<process::Process>  compile_process_;
};

class MingwCompiler : public BasicCompiler
{
public:
  MingwCompiler(const CompilerConfiguration& context);
  ~MingwCompiler();

  bool                StartCompile(CompileJob* job) override;
  bool                GenerateCmdline(CompileJob* job) override;

};

class VCCompiler : public BasicCompiler
{
public:
  VCCompiler(const CompilerConfiguration& context);
  ~VCCompiler();

  bool                StartCompile(CompileJob* job) override;
  bool                GenerateCmdline(CompileJob* job) override;
};

class GHCCompiler : public BasicCompiler
{
public:
  GHCCompiler(const CompilerConfiguration& context);
  ~GHCCompiler();

  bool                StartCompile(CompileJob* job) override;
  bool                GenerateCmdline(CompileJob* job) override;
};

class PythonCompiler : public BasicCompiler
{
public:
  PythonCompiler(const CompilerConfiguration& context);
  ~PythonCompiler();

  bool                StartCompile(CompileJob* job) override;
  bool                GenerateCmdline(CompileJob* job) override;
};

}
#endif