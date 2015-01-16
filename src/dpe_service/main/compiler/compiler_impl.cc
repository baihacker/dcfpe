#include "dpe_service/main/compiler/compiler_impl.h"

namespace ds
{

extern const ProgrammeLanguage  PL_UNKNOWN    = "";
extern const ProgrammeLanguage  PL_C          = "c";
extern const ProgrammeLanguage  PL_CPP        = "cpp";
extern const ProgrammeLanguage  PL_PYTHON     = "python";
extern const ProgrammeLanguage  PL_JAVA       = "java";
extern const ProgrammeLanguage  PL_HASKELL    = "haskell";
extern const ProgrammeLanguage  PL_GO         = "go";
extern const ProgrammeLanguage  PL_SCALA      = "scala";

extern const ISArch             ARCH_UNKNOWN  = "";
extern const ISArch             ARCH_X86      = "x86";
extern const ISArch             ARCH_X64      = "x64";

CompileJob::CompileJob() :
  language_(PL_UNKNOWN),
  optimization_option_(2),
  callback_(NULL)
{
}

CompileJob::~CompileJob()
{

}

BasicCompiler::BasicCompiler(const CompilerConfiguration& context) :
  curr_job_(NULL),
  context_(context),
  weakptr_factory_(this)
{

}

BasicCompiler::~BasicCompiler()
{
  if (compile_process_)
  {
    compile_process_->Stop();
  }
}

void BasicCompiler::OnStop(process::Process* p, process::ProcessContext* exit_code)
{
  if (!curr_job_) return;
  
  LOG(INFO) << "compiler output:\n" << curr_job_->compiler_output_;
  
  GenerateCmdline(curr_job_);
  
  auto cj = curr_job_;

  curr_job_ = NULL;
  compile_process_ = NULL;

  if (cj->callback_) cj->callback_->OnCompileFinished(cj);
  
}

void BasicCompiler::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (curr_job_)
  {
    curr_job_->compiler_output_.append(data.begin(), data.end());
  }
}

void BasicCompiler::ReportCompileSuccess(base::WeakPtr<BasicCompiler> self)
{
  if (auto* pThis = self.get())
  {
    pThis->OnStop(NULL, NULL);
  }
}

void BasicCompiler::ScheduleReportCompileSuccess()
{
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&BasicCompiler::ReportCompileSuccess,
            weakptr_factory_.GetWeakPtr()));
}

LanguageDetail BasicCompiler::GetLanguageDetail(const ProgrammeLanguage& language) const
{
  for (auto& iter: context_.language_detail_)
  {
    if (iter.language_ == language) return iter;
  }
  return LanguageDetail();
}

bool BasicCompiler::PreProcessJob(CompileJob* job)
{
  if (!job) return false;

  if (job->source_files_.empty()) return false;
  if (job->language_ == PL_UNKNOWN) job->language_ = DetectLanguage(job->source_files_);
  if (job->language_ == PL_UNKNOWN) return false;
  if (!context_.Accept(job->language_)) return false;
  
  if (job->output_file_.empty())
  {
    auto language_detail = GetLanguageDetail(job->language_);
    if (!language_detail.default_output_file_.empty())
    {
      std::map<std::string, std::string> kv = context_.variables_;
      
      kv["$(SOURCE_FILE_PATH)"] = base::NativeToUTF8(job->source_files_[0].value());
      kv["$(SOURCE_FILE_PATH_NO_EXT)"] = base::NativeToUTF8(job->source_files_[0].RemoveExtension().value());
      kv["$(SOURCE_FILE_BASENAME)"] = base::NativeToUTF8(job->source_files_[0].BaseName().value());
      kv["$(SOURCE_FILE_BASENAME_NO_EXT)"] = base::NativeToUTF8(job->source_files_[0].BaseName().RemoveExtension().value());
      kv["$(SOURCE_FILE_DIRNAME)"] = base::NativeToUTF8(job->source_files_[0].DirName().value());
      
      job->output_file_ = base::FilePath(base::UTF8ToNative(FixString(language_detail.default_output_file_, kv)));
    }
  }
  
  return true;
}

std::string BasicCompiler::FixString(const std::string& s, const std::map<std::string, std::string>& kv)
{
  std::string ret = s;
  for (auto& iter: kv)
  {
    ReplaceSubstringsAfterOffset(&ret, 0, iter.first, iter.second);
  }
  return ret;
}

bool BasicCompiler::StartCompile(CompileJob* job)
{
  if (!job || curr_job_) return false;

  if (!PreProcessJob(job)) return false;

  auto language_detail = GetLanguageDetail(job->language_);

  if (language_detail.compile_binary_.empty())
  {
    LOG(ERROR) << "No compile binary";
    return false;
  }
  
  compile_process_ = new process::Process(this);

  std::map<std::string, std::string> kv = context_.variables_;

  kv["$(OUTPUT_FILE)"] = base::NativeToUTF8(job->output_file_.value());
  kv["$(SOURCE_FILE_PATH)"] = base::NativeToUTF8(job->source_files_[0].value());
  kv["$(SOURCE_FILE_PATH_NO_EXT)"] = base::NativeToUTF8(job->source_files_[0].RemoveExtension().value());
  kv["$(SOURCE_FILE_BASENAME)"] = base::NativeToUTF8(job->source_files_[0].BaseName().value());
  kv["$(SOURCE_FILE_BASENAME_NO_EXT)"] = base::NativeToUTF8(job->source_files_[0].BaseName().RemoveExtension().value());
  kv["$(SOURCE_FILE_DIRNAME)"] = base::NativeToUTF8(job->source_files_[0].DirName().value());

  auto& po = compile_process_->GetProcessOption();
  po.image_path_ = base::FilePath(base::UTF8ToNative(FixString(language_detail.compile_binary_, kv)));
  po.inherit_env_var_ = true;
  po.env_var_keep_ = context_.env_var_keep_;
  po.env_var_merge_ = context_.env_var_merge_;
  po.env_var_replace_ = context_.env_var_replace_;
  po.current_directory_ = job->current_directory_;

  po.argument_list_.clear();
  po.argument_list_r_.clear();

  for (auto& it: language_detail.compile_args_)
  {
    if (it == "$(SOURCE_FILES)")
    {
      for (auto& it : job->source_files_) po.argument_list_.push_back(it.value());
    }
    else if (it == "$(EXTRA_COMPILE_ARGS)")
    {
      if (!job->cflags_.empty())
      {
        po.argument_list_r_.push_back(job->cflags_);
      }
    }
    else
    {
      po.argument_list_.push_back(base::UTF8ToNative(FixString(it, kv)));
    }
  }
  
  std::string().swap(job->compiler_output_);

  po.redirect_std_inout_ = true;
  po.treat_err_as_out_ = true;
  po.create_sub_process_ = true;
  po.treat_err_as_out_ = true;
  po.allow_sub_process_breakaway_job_ = true;

  bool ret = compile_process_->Start();
  if (ret)
  {
    curr_job_ = job;
    job->compile_process_ = compile_process_;
    LOG(INFO) << "compile command:\n" << base::SysWideToUTF8(compile_process_->GetProcessContext()->cmd_line_);
  }
  else
  {
    compile_process_ = NULL;
  }

  return ret;
}

bool BasicCompiler::GenerateCmdline(CompileJob* job)
{
  if (!PreProcessJob(job)) return false;

  auto language_detail = GetLanguageDetail(job->language_);
  std::map<std::string, std::string> kv = context_.variables_;
  
  kv["$(OUTPUT_FILE)"] = base::NativeToUTF8(job->output_file_.value());
  kv["$(SOURCE_FILE_PATH)"] = base::NativeToUTF8(job->source_files_[0].value());
  kv["$(SOURCE_FILE_PATH_NO_EXT)"] = base::NativeToUTF8(job->source_files_[0].RemoveExtension().value());
  kv["$(SOURCE_FILE_BASENAME)"] = base::NativeToUTF8(job->source_files_[0].BaseName().value());
  kv["$(SOURCE_FILE_BASENAME_NO_EXT)"] = base::NativeToUTF8(job->source_files_[0].BaseName().RemoveExtension().value());
  kv["$(SOURCE_FILE_DIRNAME)"] = base::NativeToUTF8(job->source_files_[0].DirName().value());
  
  job->image_path_ = base::FilePath(base::UTF8ToNative(FixString(language_detail.running_binary_, kv)));
  job->arguments_.clear();

  for (auto& iter: language_detail.running_args_)
  {
    job->arguments_.push_back(base::UTF8ToNative(FixString(iter, kv)));
  }
  
  return true;
}

InterpreterCompiler::InterpreterCompiler(const CompilerConfiguration& context) :
  BasicCompiler(context)
{

}

InterpreterCompiler::~InterpreterCompiler()
{

}

bool InterpreterCompiler::StartCompile(CompileJob* job)
{
  if (!job || curr_job_) return false;
  
  GenerateCmdline(job);

  curr_job_ = job;

  ScheduleReportCompileSuccess();

  return true;
}

}

namespace ds
{
struct Ext2Lang
{
  const NativeChar* ext;
  const ProgrammeLanguage    lang;
};

static Ext2Lang info[] = 
{
{L".c", PL_C},
{L".cpp", PL_CPP},
{L".cc", PL_CPP},
{L".hpp", PL_CPP},
{L".py", PL_PYTHON},
{L".java", PL_JAVA},
{L".hs", PL_HASKELL},
{L".go", PL_GO},
{L".scala", PL_SCALA},
};

ProgrammeLanguage DetectLanguage(const std::vector<base::FilePath>& filepath)
{
  static const int32_t info_size = sizeof(info) / sizeof(info[0]);
  for (auto& item: filepath)
  {
    NativeString ext = item.Extension();
    for (auto& it : info)
    {
      if (base::StringEqualCaseInsensitive(ext, it.ext))
      {
        return it.lang;
      }
    }
  }
  return PL_UNKNOWN;
}

bool CompilerConfiguration::Accept(const ProgrammeLanguage& language) const
{
  for (auto& it: language_detail_)
  if (it.language_ == language) return true;
  return false;
}

}
