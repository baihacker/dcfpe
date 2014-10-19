#include "dpe_service/main/dpe_service.h"

#include "dpe_service/main/compiler_impl.h"

#include <iostream>
using namespace std;
namespace ds
{

/*
*******************************************************************************
*/
DPEService::DPEService() :
  compilers_(NULL)
{
}

DPEService::~DPEService()
{
}
scoped_refptr<CompilerResource> cl;
scoped_refptr<CompileJob>       cj;
void DPEService::Start()
{
  // todo: register ipc channel
  ZServer* test_server = new ZServer();
  test_server->Start(MAKE_IP(127, 0, 0, 1));
#if 0
  LoadCompilers(L"D:\\compilers.json");

  cl = CreateCompiler(L"vc", L"", ARCH_UNKNOWN, PL_CPP);
  cj = new CompileJob();
  cj->current_directory_ = L"D:\\projects";
  cj->source_files_.push_back(L"D:\\projects\\orz.c");
  cj->output_file_ = L"orddddddz.exe";
  cj->language_ = PL_CPP;
  cj->cflags_ = L"/EHsc";
  
  cl->StartCompile(cj);
#endif
}

void DPEService::WillStop()
{
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(&DPEService::StopImpl, base::Unretained(this)
      )
    );
}

void DPEService::StopImpl()
{
  const int n = server_list_.size();
  for (int i = n - 1; i >= 0; --i)
  {
    server_list_[i]->Stop();
  }
  for (int i = n - 1; i >= 0; --i)
  {
    delete server_list_[i];
  }
  std::vector<ZServer*>().swap(server_list_);
  base::quit_main_loop();
}

int32_t DPEService::handle_message(int32_t handle, const std::string& data)
{
  return 0;
}

scoped_refptr<CompilerResource> DPEService::CreateCompiler(
    const std::wstring& type, const std::wstring& version, int32_t arch, 
    int32_t language, const std::vector<std::wstring>& source_file)
{
  if (language == PL_UNKNOWN) language = DetectLanguage(source_file);
  if (language == PL_UNKNOWN) return NULL;

  if (base::StringEqualCaseInsensitive(type, L"mingw") ||
      base::StringEqualCaseInsensitive(type, L"vc"))
  {
    if (language != PL_C && language != PL_CPP) return NULL;
  }

  if (base::StringEqualCaseInsensitive(type, L"python"))
  {
    if (language != PL_PYTHON) return NULL;
  }

  for (auto& it: compilers_)
  {
    if (base::StringEqualCaseInsensitive(it.type_, type))
    {

      if (arch != ARCH_UNKNOWN && arch != it.arch_) continue;

      if (base::StringEqualCaseInsensitive(it.type_, L"mingw"))
      {
        return new MingwCompiler(it);
      }
      else if (base::StringEqualCaseInsensitive(it.type_, L"vc"))
      {
        return new VCCompiler(it);
      }
      else if (base::StringEqualCaseInsensitive(it.type_, L"python"))
      {
        return new PythonCompiler(it);
      }
      else if (base::StringEqualCaseInsensitive(it.type_, L"pypy"))
      {
        return new PypyCompiler(it);
      }
    }
  }

  return NULL;
}

static CompilerConfiguration::env_var_list_t
ParseEnvVar(base::DictionaryValue* val)
{
  CompilerConfiguration::env_var_list_t ret;
  for (auto iter = base::DictionaryValue::Iterator(*val); !iter.IsAtEnd(); iter.Advance())
  {
    std::string val;
    
    if (iter.value().GetAsString(&val))
    {
      ret.push_back(
        {
          base::SysUTF8ToWide(iter.key()),
          base::SysUTF8ToWide(val)
        });
    }
  }
  return ret;
}
void DPEService::LoadCompilers(const std::wstring& file)
{
  std::string data;

  if (!base::ReadFileToString(base::FilePath(file), &data)) return;
  base::Value* root = base::JSONReader::Read(data.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);
  if (!root) return;

  base::ListValue* lv = NULL;
  if (!root->GetAsList(&lv)) return;

    const int n = lv->GetSize();
    for (int i = 0; i < n; ++i)
    {
      base::DictionaryValue* dv = NULL;
      if (!lv->GetDictionary(i, &dv)) continue;

      CompilerConfiguration config;
      config.arch_ = ARCH_UNKNOWN;
      std::string val;
      if (dv->GetString("type", &val))
      {
        config.type_ = base::SysUTF8ToWide(val);
      }
      else
      {
        continue;
      }

      if (dv->GetString("name", &val))
      {
        config.name_ = base::SysUTF8ToWide(val);
      }
      else
      {
        config.name_ = L"Unknown";
      }
      
      if (dv->GetString("version", &val))
      {
        config.version_ = base::SysUTF8ToWide(val);
      }
      if (dv->GetString("image_dir", &val))
      {
        config.image_dir_ = base::SysUTF8ToWide(val);
      }
      if (dv->GetString("arch", &val))
      {
        if (base::StringEqualCaseInsensitive(val, "x86"))
        {
          config.arch_ = ARCH_X86;
        }
        else if (base::StringEqualCaseInsensitive(val, "x64"))
        {
          config.arch_ = ARCH_X64;
        }
      }

      base::DictionaryValue* ev = NULL;
      if (dv->GetDictionary("env_var_keep", &ev))
      {
        config.env_var_keep_ = ParseEnvVar(ev);
      }
      if (dv->GetDictionary("env_var_merge", &ev))
      {
        config.env_var_merge_ = ParseEnvVar(ev);
      }
      if (dv->GetDictionary("env_var_replace", &ev))
      {
        config.env_var_replace_ = ParseEnvVar(ev);
      }

      compilers_.push_back(config);
    }
  delete root;
}

}