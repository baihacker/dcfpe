#ifndef DPE_SERVICE_MAIN_COMPILER_RESOURCE_H_
#define DPE_SERVICE_MAIN_COMPILER_RESOURCE_H_

#include "dpe_service/main/resource.h"

namespace ds
{
enum
{
  PL_UNKNOWN = 0,
  PL_C,
  PL_CPP,
  PL_PYTHON,
  PL_JAVA,
  PL_HASKELL,
};

enum
{
  ARCH_UNKNOWN = -1,
  ARCH_X86 = 0,
  ARCH_X64,
};

int32_t DetectLanguage(const std::vector<std::wstring>& filepath);

struct CompileJob;
struct CompilerCallback
{
  virtual ~CompilerCallback() {}
  virtual void OnCompileFinished(CompileJob* job) = 0;
};

struct CompileJob : public base::RefCounted<CompileJob>
{
public:
  CompileJob();
  ~CompileJob();

  int32_t                     language_;
  std::wstring                compiler_;
  
  std::wstring                current_directory_;
  std::wstring                output_directory_;
  std::vector<std::wstring>   source_files_;
  std::wstring                output_file_;
  int32_t                     optimization_option_;

  std::wstring                cflags_;
  std::wstring                lflags_;

  // about compile result
  std::string                 compiler_output_;

  scoped_refptr<process::Process> compile_process_;
  CompilerCallback*           callback_;

  // how to run the result
  std::wstring                image_path_;
  std::vector<std::wstring>   arguments_;
};

struct CompilerResource : public ResourceBase
{
  // hold a weak reference to a compile job after start
  virtual bool                StartCompile(CompileJob* job) = 0;
  
  // general utility to fill image_path_ and arguments_
  virtual bool                GenerateCmdline(CompileJob* job) = 0;
  
  virtual int32               GetArchitecture() const = 0;
};

struct CompilerConfiguration
{
  typedef std::vector<std::pair<std::wstring, std::wstring> > env_var_list_t;
  std::wstring      name_;
  std::wstring      type_;
  std::wstring      image_dir_;
  int32_t           arch_;
  std::wstring      version_;
  env_var_list_t    env_var_keep_;
  env_var_list_t    env_var_merge_;
  env_var_list_t    env_var_replace_;
};
}
#endif