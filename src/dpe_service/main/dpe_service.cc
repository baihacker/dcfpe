#include "dpe_service/main/dpe_service.h"

#include "dpe_service/main/compiler/compiler_impl.h"
#include "dpe_service/main/dpe_model/dpe_device_impl.h"
#include "dpe_service/main/dpe_model/dpe_controller.h"

#include <Shlobj.h>

namespace ds
{
DPENode::DPENode(int32_t node_id, const std::string& address, bool is_local) :
  node_id_(node_id),
  server_address_(address),
  is_local_(is_local),
  response_time_(base::TimeDelta::FromInternalValue(0)),
  response_count_(0),
  zclient_(new ZServerClient(address)),
  weakptr_factory_(this)
{
}

DPENode::~DPENode()
{
}

DPENodeManager::DPENodeManager() :
  next_node_id_(0),
  weakptr_factory_(this)
{
}

DPENodeManager::~DPENodeManager()
{
}

void  DPENodeManager::AddNode(int32_t ip, int32_t port)
{
  AddNode(base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

void  DPENodeManager::AddNode(const std::string& address)
{
  for (auto iter = node_list_.begin(); iter != node_list_.end();)
  {
    if ((*iter)->server_address_ == address)
    {
      return;
    }
    else
    {
      ++iter;
    }
  }
  
  auto node = new DPENode(next_node_id_++, address, false);
  node_list_.push_back(node);
  node->GetZClient()->SayHello(
    base::Bind(&DPENodeManager::HandleResponse,
      weakptr_factory_.GetWeakPtr(),
      node->GetNodeId())
    );
}

void  DPENodeManager::RemoveNode(int32_t ip, int32_t port)
{
  RemoveNode(base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

void  DPENodeManager::RemoveNode(const std::string& address)
{
  for (auto iter = node_list_.begin(); iter != node_list_.end();)
  {
    if ((*iter)->server_address_ == address)
    {
      iter = node_list_.erase(iter);
    }
    else
    {
      ++iter;
    }
  }
}

DPENode*  DPENodeManager::GetNodeById(const int32_t id) const
{
  for (auto& it : node_list_)
  if (it->GetNodeId() == id)
  {
    return it;
  }
  return NULL;
}

void  DPENodeManager::HandleResponse(base::WeakPtr<DPENodeManager> self,
                int32_t node_id, scoped_refptr<base::ZMQResponse> rep)
{
  if (DPENodeManager* pThis = self.get())
  {
    pThis->HandleResponseImpl(node_id, rep);
  }
}

void  DPENodeManager::HandleResponseImpl(int32_t node_id, scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ != 0)
  {
    LOG(INFO) << "DPENodeManager : handle response " << rep->error_code_;
    return;
  }
  
  base::Value* v = base::JSONReader::Read(rep->data_, base::JSON_ALLOW_TRAILING_COMMAS);

  do
  {
    if (!v)
    {
      LOG(ERROR) << "\nCan not parse response";
      break;
    }
    
    base::DictionaryValue* dv = NULL;
    if (!v->GetAsDictionary(&dv)) break;
    {
      std::string val;
      std::string cookie;
      if (!dv->GetString("type", &val)) break;
      if (val != "rsc") break;
      
      if (!dv->GetString("src", &val)) break;
      
      if (!dv->GetString("dest", &val)) break;
      
      if (dv->GetString("cookie", &val))
      {
        cookie = std::move(val);
      }
      
      if (!dv->GetString("reply", &val)) break;
      
      if (val != "HelloResponse")
      {
        if (!dv->GetString("error_code", &val)) break;
        if (val != "0") break;
        
        if (!dv->GetString("ots", &val)) break;

        DPENode* node = GetNodeById(node_id);
        auto last = base::Time::FromInternalValue(atoll(val.c_str()));
        auto now = base::Time::Now();
        dv->GetString("pa", &node->pa_);
        node->response_time_ += now - last;
        ++node->response_count_;
      }
    }
  } while (false);
  
  delete v;
}

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
  LoadCompilers(config_dir_.Append(L"compilers.json"));
  
  // start ipc
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
  
  // we always have a default server
  ZServer* default_server = new ZServer(this);
  default_server->Start(MAKE_IP(127, 0, 0, 1));
  server_list_.push_back(default_server);
  
  node_manager_.AddNode(default_server->GetServerAddress());
  node_manager_.node_list_.front()->SetIsLocal(true);
  
#if 1
  ctrl = new DPEController(this);
  ctrl->SetLanguage(PL_CPP);
  ctrl->SetCompilerType(L"mingw");
  ctrl->SetHomePath(home_dir_);
  ctrl->SetJobName(L"test_job");
  ctrl->SetSource(base::FilePath(L"D:\\usr\\projects\\test_job\\source.c"));
  ctrl->SetWorker(base::FilePath(L"D:\\usr\\projects\\test_job\\worker.c"));
  ctrl->SetSink(base::FilePath(L"D:\\usr\\projects\\test_job\\sink.c"));
  ctrl->AddRemoteDPEService(true, default_server->GetServerAddress());
  ctrl->Start();
#endif
}

void DPEService::WillStop()
{
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
        base::Bind(&DPEService::StopImpl, base::Unretained(this)
      ));
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
  {
    wchar_t lpszFolder[ MAX_PATH ] = { 0 };
    ::SHGetSpecialFolderPath( NULL, lpszFolder, CSIDL_LOCAL_APPDATA, TRUE);
    config_dir_ = base::FilePath(lpszFolder).DirName().Append(L"LocalLow\\dcfpe");
    base::CreateDirectory(config_dir_);
  }
  base::Value* root = NULL;
  do
  {
    base::FilePath path = config_dir_.Append(L"config.json");
    std::string data;

    if (!base::ReadFileToString(path, &data)) break;
    root = base::JSONReader::Read(data.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);
    if (!root) break;

    base::DictionaryValue* dv = NULL;
    if (!root->GetAsDictionary(&dv)) break;

    std::string val;
    if (dv->GetString("home_dir", &val))
    {
      home_dir_ = base::FilePath(base::UTF8ToNative(val));
    }
    if (dv->GetString("temp_dir", &val))
    {
      temp_dir_ = base::FilePath(base::UTF8ToNative(val));
    }
    LOG(INFO) << "parse config successful";
  } while (false);

  if (root)
  {
    delete root;
  }
  
  if (home_dir_.value().empty())
  {
    //home_dir_ = base::GetHomeDir().Append(L"dcfpe");
    home_dir_ = base::FilePath(L"C:\\Dcfpe\\Home");
  }
  
  if (temp_dir_.value().empty())
  {
    //base::GetTempDir(&temp_dir_);
    //temp_dir_ = temp_dir_.Append(L"dcfpe");
    temp_dir_ = base::FilePath(L"C:\\Dcfpe\\Temp");
  }
  
  base::CreateDirectory(home_dir_);
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
      config.type_ = base::UTF8ToWide(val);
    }
    else
    {
      continue;
    }

    if (dv->GetString("name", &val))
    {
      config.name_ = base::UTF8ToWide(val);
    }
    else
    {
      config.name_ = L"Unknown";
    }
    
    if (dv->GetString("version", &val))
    {
      config.version_ = base::UTF8ToWide(val);
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

std::wstring DPEService::GetDefaultCompilerType(const ProgrammeLanguage& language)
{
  // todo : get default compiler type from configuration 
  if (language == PL_C || language == PL_CPP) return L"mingw";
  if (language == PL_HASKELL) return L"ghc";
  if (language == PL_PYTHON) return L"python";
  return L"";
}

scoped_refptr<Compiler> DPEService::CreateCompiler(
    std::wstring type, const std::wstring& version, ISArch arch, 
    ProgrammeLanguage language, const std::vector<base::FilePath>& source_file)
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
  device->SetHomePath(home_dir_);
  device->AddRef();
  dpe_device_list_.push_back(device);
  return device;
}

}