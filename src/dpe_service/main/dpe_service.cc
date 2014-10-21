#include "dpe_service/main/dpe_service.h"

#include "dpe_service/main/compiler_impl.h"
#include "dpe_service/main/dpe_device_impl.h"
#include "dpe_service/main/dpe_controller.h"

#include <Shlobj.h>

#include <iostream>
using namespace std;
namespace ds
{

/*
*******************************************************************************
*/
DPEService::DPEService() :
  compilers_(NULL),
  ipc_sub_handle_(base::INVALID_CHANNEL_ID)
{
}

DPEService::~DPEService()
{
}

DPEController* ctrl;
void DPEService::Start()
{
  LoadConfig();
  LoadCompilers(base::FilePath(L"D:\\compilers.json"));

  auto mc = base::zmq_message_center();
  for (int i = 0; i < 3; ++i)
  {
    std::string address = base::AddressHelper::MakeZMQTCPAddress(
        base::AddressHelper::GetProcessIP(),
        base::AddressHelper::GetNextAvailablePort()
      );

    ipc_sub_handle_ = mc->RegisterChannel(base::CHANNEL_TYPE_SUB,
      address, true);

    if (ipc_sub_handle_ != base::INVALID_CHANNEL_ID)
    {
      ipc_sub_address_ = address;
      break;
    }
  }
  if (ipc_sub_handle_ == base::INVALID_CHANNEL_ID)
  {
    LOG(ERROR) << "Can not start IPC server";
    WillStop();
    return;
  }
  
  mc->AddMessageHandler(this);
  
  ZServer* default_server = new ZServer(this);
  default_server->Start(MAKE_IP(127, 0, 0, 1));
  server_list_.push_back(default_server);
  
#if 1
  ctrl = new DPEController(this);
  ctrl->SetLanguage(PL_CPP);
  ctrl->SetCompilerType(L"mingw");
  ctrl->SetHomePath(base::FilePath(L"D:\\projects"));
  ctrl->SetJobName(L"test_job");
  ctrl->SetSource(base::FilePath(L"D:\\Projects\\case\\source.c"));
  ctrl->SetWorker(base::FilePath(L"D:\\Projects\\case\\worker.c"));
  ctrl->SetSink(base::FilePath(L"D:\\Projects\\case\\sink.c"));
  ctrl->AddRemoteDPEService(true, default_server->GetServerAddress());
  ctrl->Start();
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
  if (handle != ipc_sub_handle_) return 0;

  base::DictionaryValue rep;
  base::Value* v = base::JSONReader::Read(data.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);
  rep.SetString("type", "ipc");
  rep.SetString("error_code", "-1");
  
  return 1;
}

void DPEService::LoadConfig()
{
  home_dir_ = base::GetHomeDir().Append(L"dcfpe");
  base::CreateDirectory(home_dir_);

  {
    wchar_t lpszFolder[ MAX_PATH ] = { 0 };
    ::SHGetSpecialFolderPath( NULL, lpszFolder, CSIDL_LOCAL_APPDATA, TRUE);
    config_dir_ = base::FilePath(lpszFolder).DirName().Append(L"LocalLow\\dcfpe");
    base::CreateDirectory(config_dir_);
  }

  base::GetTempDir(&temp_dir_);
  temp_dir_ = temp_dir_.Append(L"dcfpe");
  base::CreateDirectory(temp_dir_);
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
          base::UTF8ToNative(iter.key()),
          base::UTF8ToNative(val)
        });
    }
  }
  return ret;
}

void DPEService::LoadCompilers(const base::FilePath& file)
{
  std::string data;

  if (!base::ReadFileToString(file, &data)) return;
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
        config.type_ = base::UTF8ToNative(val);
      }
      else
      {
        continue;
      }

      if (dv->GetString("name", &val))
      {
        config.name_ = base::UTF8ToNative(val);
      }
      else
      {
        config.name_ = L"Unknown";
      }
      
      if (dv->GetString("version", &val))
      {
        config.version_ = base::UTF8ToNative(val);
      }
      if (dv->GetString("image_dir", &val))
      {
        config.image_dir_ = base::FilePath(base::UTF8ToNative(val));
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

std::wstring DPEService::GetDefaultCompilerType(int32_t language)
{
  // todo : get default compiler type from configuration 
  if (language == PL_C || language == PL_CPP) return L"mingw";
  if (language == PL_HASKELL) return L"ghc";
  if (language == PL_PYTHON) return L"python";
  return L"";
}

scoped_refptr<CompilerResource> DPEService::CreateCompiler(
    std::wstring type, const std::wstring& version, int32_t arch, 
    int32_t language, const std::vector<base::FilePath>& source_file)
{
  if (language == PL_UNKNOWN) language = DetectLanguage(source_file);
  if (language == PL_UNKNOWN) return NULL;

  if (type.empty())
  {
    type = GetDefaultCompilerType(language);
  }
  
  if (base::StringEqualCaseInsensitive(type, L"mingw") ||
      base::StringEqualCaseInsensitive(type, L"vc"))
  {
    if (language != PL_C && language != PL_CPP) return NULL;
  }

  if (base::StringEqualCaseInsensitive(type, L"ghc"))
  {
    if (language != PL_HASKELL) return NULL;
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
      else if (base::StringEqualCaseInsensitive(it.type_, L"ghc"))
      {
        return new GHCCompiler(it);
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

void DPEService::RemoveDPEDevice(DPEDevice* device)
{
  for (auto iter = dpe_device_list_.begin();
        iter != dpe_device_list_.end();)
  {
    if (*iter == device)
    {
      device->Release();
      iter = dpe_device_list_.erase(iter);
    }
    else
    {
      ++iter;
    }
  }
}

scoped_refptr<DPEDevice> DPEService::CreateDPEDevice(
  ZServer* zserver, base::DictionaryValue* request)
{
  scoped_refptr<DPEDevice> device = new DPEDeviceImpl(this);
  if (!device->OpenDevice(zserver->ip_))
  {
    return NULL;
  }
  device->SetHomePath(base::FilePath(L"D:\\worker_home"));
  device->AddRef();
  dpe_device_list_.push_back(device);
  return device;
}

}