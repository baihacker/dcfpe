#ifndef DPE_SERVICE_MAIN_COMPILER_COMPILER_H_
#define DPE_SERVICE_MAIN_COMPILER_COMPILER_H_

#include "dpe_service/main/resource.h"
#include "dpe_base/chromium_base.h"

namespace ds
{

struct ProgrammeLanguage
{
ProgrammeLanguage(){}
ProgrammeLanguage(const std::string& lang) : lang_(lang){}
ProgrammeLanguage(std::string&& lang) : lang_(std::move(lang)){}
ProgrammeLanguage& operator = (const ProgrammeLanguage& other)
{
  lang_ = other.lang_;
  return *this;
}
ProgrammeLanguage& operator = (ProgrammeLanguage&& other)
{
  lang_ = std::move(other.lang_);
  return *this;
}
bool operator == (const ProgrammeLanguage& other) const
{
  return base::StringEqualCaseInsensitive(lang_, other.lang_);
}
bool operator != (const ProgrammeLanguage& other) const
{
  return !this->operator == (other);
}
static ProgrammeLanguage FromUTF8(const std::string& s){return s;}
std::string ToUTF8()const {return lang_;}

private:
std::string lang_;
};

struct ISArch
{
ISArch(){}
ISArch(const std::string& arch) : arch_(arch){}
ISArch(std::string&& arch) : arch_(std::move(arch)){}
ISArch& operator = (const ISArch& other)
{
  arch_ = other.arch_;
  return *this;
}
ISArch& operator = (ISArch&& other)
{
  arch_ = std::move(other.arch_);
  return *this;
}
bool operator == (const ISArch& other) const
{
  return base::StringEqualCaseInsensitive(arch_, other.arch_);
}
bool operator != (const ISArch& other) const
{
  return !this->operator == (other);
}
static ISArch FromUTF8(const std::string& s){return s;}
std::string ToUTF8()const {return arch_;}

private:
std::string arch_;
};

extern const ProgrammeLanguage PL_UNKNOWN;
extern const ProgrammeLanguage PL_C;
extern const ProgrammeLanguage PL_CPP;
extern const ProgrammeLanguage PL_PYTHON;
extern const ProgrammeLanguage PL_JAVA;
extern const ProgrammeLanguage PL_HASKELL;
extern const ProgrammeLanguage PL_GO;
extern const ProgrammeLanguage PL_SCALA;

extern const ISArch             ARCH_UNKNOWN;
extern const ISArch             ARCH_X86;
extern const ISArch             ARCH_X64;
/*
extern const ProgrammeLanguage PL_UNKNOWN = "";
extern const ProgrammeLanguage PL_C       = "c";
extern const ProgrammeLanguage PL_CPP     = "cpp";
extern const ProgrammeLanguage PL_PYTHON  = "python";
extern const ProgrammeLanguage PL_JAVA    = "java";
extern const ProgrammeLanguage PL_HASKELL = "haskell";

extern const ISArch             ARCH_UNKNOWN = "";
extern const ISArch             ARCH_X86     = "x86";
extern const ISArch             ARCH_X64     = "x64";

#define PL_UNKNOWN  ""
#define PL_C        "c"
#define PL_CPP      "cpp"
#define PL_PYTHON   "python"
#define PL_JAVA     "java"
#define PL_HASKELL  "haskell"

#define ARCH_UNKNOWN  ""
#define ARCH_X86      "x86"
#define ARCH_X64      "x64"
*/

ProgrammeLanguage DetectLanguage(const std::vector<base::FilePath>& filepath);

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

  ProgrammeLanguage           language_;
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
  
  virtual ISArch              GetArchitecture() const = 0;
};

struct LanguageDetail
{
  ProgrammeLanguage                   language_;
  std::string                         compile_binary_;
  std::vector<std::string>            compile_args_;
  std::string                         default_output_file_;
  std::string                         running_binary_;
  std::vector<std::string>            running_args_;
};

struct CompilerConfiguration
{
  typedef std::vector<std::pair<NativeString, NativeString> > env_var_list_t;
  std::wstring      name_;
  std::wstring      type_;      // basic compile model: [language support, how to find the binary, how to construct command]
  
  std::map<std::string, std::string> variables_;
  ISArch            arch_;
  std::wstring      version_;
  
  env_var_list_t    env_var_keep_;
  env_var_list_t    env_var_merge_;
  env_var_list_t    env_var_replace_;
  
  std::vector<LanguageDetail> language_detail_;
  
  bool              Accept(const ProgrammeLanguage& language) const;
};
}
#endif