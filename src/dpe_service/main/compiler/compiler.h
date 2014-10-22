#ifndef DPE_SERVICE_MAIN_COMPILER_COMPILER_H_
#define DPE_SERVICE_MAIN_COMPILER_COMPILER_H_

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

int32_t DetectLanguage(const std::vector<base::FilePath>& filepath);

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
  std::wstring                compiler_type_;
  
  base::FilePath              current_directory_;
  base::FilePath              output_directory_;
  std::vector<base::FilePath> source_files_;
  base::FilePath              output_file_;
  int32_t                     optimization_option_;

  NativeString                cflags_;
  NativeString                lflags_;

  // about compile result
  std::string                 compiler_output_;

  scoped_refptr<process::Process> compile_process_;
  CompilerCallback*           callback_;

  // how to run the result
  base::FilePath              image_path_;
  std::vector<NativeString>   arguments_;
};

struct Compiler : public ResourceBase
{
  // hold a weak reference to a compile job after start
  virtual bool                StartCompile(CompileJob* job) = 0;
  
  // general utility to fill image_path_ and arguments_
  virtual bool                GenerateCmdline(CompileJob* job) = 0;
  
  virtual int32               GetArchitecture() const = 0;
};

struct CompilerConfiguration
{
  typedef std::vector<std::pair<NativeString, NativeString> > env_var_list_t;
  std::wstring      name_;
  std::wstring      type_;            // basic compile model: [language support, how to find the binary, how to construct command]
  base::FilePath    image_dir_;
  int32_t           arch_;
  std::wstring      version_;
  env_var_list_t    env_var_keep_;
  env_var_list_t    env_var_merge_;
  env_var_list_t    env_var_replace_;
};
}
#endif