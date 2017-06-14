#include "dpe_service/main/dpe_service.h"

#include "dpe_service/main/dpe_service_dialog.h"
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

DPENodeManager::DPENodeManager(DPEService* dpe) :
  dpe_(dpe),
  next_node_id_(0),
  weakptr_factory_(this)
{
}

DPENodeManager::~DPENodeManager()
{
}

void  DPENodeManager::AddObserver(DPEGraphObserver* ob)
{
  if (!ob) return;
  
  for (auto node: observer_list_)
  if (node == ob) return;
  
  observer_list_.push_back(ob);
}

void  DPENodeManager::RemoveObserver(DPEGraphObserver* ob)
{
  for (std::vector<DPEGraphObserver*>::iterator iter = observer_list_.begin();
    iter != observer_list_.end(); ++iter)
  if (*iter == ob)
  {
    observer_list_.erase(iter);
    break;
  }
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
  
  for (auto ob: observer_list_)
  ob->OnServerListUpdated();
}

void  DPENodeManager::RemoveNode(int32_t ip, int32_t port, bool remove_local)
{
  RemoveNode(base::AddressHelper::MakeZMQTCPAddress(ip, port));
}

void  DPENodeManager::RemoveNode(const std::string& address, bool remove_local)
{
  int erased_count = 0;
  
  for (auto iter = node_list_.begin(); iter != node_list_.end();)
  {
    if ((*iter)->server_address_ == address && (remove_local || !(*iter)->is_local_))
    {
      iter = node_list_.erase(iter);
      ++erased_count;
    }
    else
    {
      ++iter;
    }
  }
  
  if (erased_count > 0)
  {
    for (auto ob: observer_list_)
    ob->OnServerListUpdated();
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

      if (val == "HelloResponse")
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
  for (auto ob: observer_list_)
  ob->OnServerListUpdated();
}

/*
*******************************************************************************
*/
DPEService::DPEService() :
  node_manager_(this),
  compilers_(NULL),
  ipc_sub_handle_(base::INVALID_CHANNEL_ID),
  dlg_(new CDPEServiceDlg(this))
{
  
}

DPEService::~DPEService()
{
}

static inline std::string get_iface_address()
{
  char hostname[128];
  char localHost[128][32]={{0}};
  struct hostent* temp;
  gethostname( hostname , 128 );
  temp = gethostbyname( hostname );
  for(int i=0 ; temp->h_addr_list[i]!=NULL && i< 1; ++i)
  {
    strcpy(localHost[i], inet_ntoa(*(struct in_addr *)temp->h_addr_list[i]));
  }
  return localHost[0];
}

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

  dlg_->Create(NULL);
  dlg_->ShowWindow(SW_SHOW);
  
  node_manager_.AddObserver(dlg_);
  
  if (!server_address_.empty())
  {
    if (server_address_ == "0.0.0.0")
    {
      server_address_ = get_iface_address();
    }
    std::vector<std::string> tokens;
    Tokenize(server_address_, ".", &tokens);

    if (tokens.size() == 4)
    {
      uint32_t ip = 0;
      for (auto item: tokens)
      {
        ip = ip << 8 | (uint8_t)atoi(item.c_str());
      }
      ZServer* server = new ZServer(this);
      if (server->Start(ip))
      {
        LOG(INFO) << "Start server at " << server_address_;
        server_list_.push_back(server);
        node_manager_.AddNode(server->GetServerAddress());
        node_manager_.node_list_.front()->SetIsLocal(true);
        server->AddRef();
        //auto succeed = server->Advertise();
        //LOG(INFO) << "Advertise result " << succeed;
      }
      else
      {
        LOG(INFO) << "Can not start server at " << server_address_;
        server->AddRef();
        server->Release();
      }
    }
  }
  // we always have a default server
  /*
  ZServer* default_server = new ZServer(this);
  default_server->Start(MAKE_IP(127, 0, 0, 1));
  server_list_.push_back(default_server);

  node_manager_.AddNode(default_server->GetServerAddress());
  node_manager_.node_list_.front()->SetIsLocal(true);
  */
  
#if 0
  ctrl = new DPEController(this);
  ctrl->AddRemoteDPEService(true, default_server->GetServerAddress());
  ctrl->Start(base::FilePath(L"D:\\projects\\git\\dcfpe\\demo\\square_sum\\test.dpe"));
#endif
}

void DPEService::WillStop()
{
  LOG(INFO) << "WillStop";
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
        base::Bind(&DPEService::StopImpl, base::Unretained(this)
      ));
}

void DPEService::StopImpl()
{
  LOG(INFO) << "StopImpl";
  if (dlg_)
  {
    if (dlg_->IsWindow())
    {
      dlg_->DestroyWindow();
    }
    dlg_ = NULL;
  }
  const int n = static_cast<int>(server_list_.size());
  for (int i = n - 1; i >= 0; --i)
  {
    server_list_[i]->Stop();
  }
  for (int i = n - 1; i >= 0; --i)
  {
    server_list_[i]->Release();
    //delete server_list_[i];
  }
  std::vector<ZServer*>().swap(server_list_);
  
  auto mc = base::zmq_message_center();
  mc->RemoveChannel(ipc_sub_handle_);
  mc->RemoveMessageHandler(this);

  SaveConfig();

  base::will_quit_main_loop();
}

void DPEService::test_action()
{
  const int n = static_cast<int>(server_list_.size());
  for (int i = n - 1; i >= 0; --i)
  {
    server_list_[i]->Advertise();
  }
}

void  DPEService::SetLastOpen(const base::FilePath& path)
{
  last_open_ = base::NativeToUTF8(path.value());
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
    config_path_ = config_dir_.Append(L"config.json");
    std::string data;

    if (!base::ReadFileToString(config_path_, &data)) break;
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
    if (dv->GetString("server_address", &val))
    {
      config_server_address_ = val;
      server_address_ = val;
    }
    if (dv->GetString("last_open", &val))
    {
      last_open_ = val;
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

void  DPEService::SaveConfig()
{
  base::DictionaryValue kv;
  kv.SetString("home_dir", base::NativeToUTF8(home_dir_.value()));
  kv.SetString("temp_dir", base::NativeToUTF8(temp_dir_.value()));
  kv.SetString("server_address", config_server_address_);
  kv.SetString("last_open", last_open_);
  
  std::string ret;
  if (base::JSONWriter::WriteWithOptions(&kv, base::JSONWriter::OPTIONS_PRETTY_PRINT, &ret))
  {
    base::WriteFile(config_path_, ret.c_str(), static_cast<int>(ret.size()));
  }
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

static std::vector<std::string>
ParseStringList(base::ListValue* val)
{
  std::vector<std::string> ret;

  const int n = static_cast<int>(val->GetSize());

  for (int i = 0; i < n; ++i)
  {
    std::string s;
    if (val->GetString(i, &s))
    {
      ret.push_back(s);
    }
  }

  return ret;
}

std::map<std::string, std::string>
ParseVariables(base::DictionaryValue* val)
{
  std::map<std::string, std::string> ret;
  for (auto iter = base::DictionaryValue::Iterator(*val); !iter.IsAtEnd(); iter.Advance())
  {
    std::string val;

    if (iter.value().GetAsString(&val))
    {
      ret.insert(
        {
          "$(" + iter.key() + ")",
          val
        });
    }
  }
  return ret;
}

static std::vector<LanguageDetail>
ParseLanguageDetail(base::ListValue* val)
{
  std::vector<LanguageDetail> ret;
  const int n = static_cast<int>(val->GetSize());
  for (int i = 0; i < n; ++i)
  {
    base::DictionaryValue* dv = NULL;
    if (val->GetDictionary(i, &dv))
    {
      LanguageDetail detail;
      std::string language;

      if (!dv->GetString("language", &language)) continue;
      if (language.empty()) continue;
      detail.language_ = language;

      std::string compile_binary;
      dv->GetString("compile_binary", &compile_binary);
      detail.compile_binary_ = compile_binary;

      base::ListValue* lv = NULL;
      if (dv->GetList("compile_args", &lv))
      {
        detail.compile_args_ = ParseStringList(lv);
      }

      dv->GetString("default_output_file", &detail.default_output_file_);

      std::string running_binary;
      if (!dv->GetString("running_binary", &running_binary)) continue;
      if (running_binary.empty()) continue;
      detail.running_binary_ = running_binary;

      if (dv->GetList("running_args", &lv))
      {
        detail.running_args_ = ParseStringList(lv);
      }

      ret.push_back(detail);
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

  const int n = static_cast<int>(lv->GetSize());
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
    
    base::DictionaryValue* var = NULL;
    if (dv->GetDictionary("variables", &var))
    {
      config.variables_ = ParseVariables(var);
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

    base::ListValue* lv = NULL;
    if (dv->GetList("language_detail", &lv))
    {
      config.language_detail_ = ParseLanguageDetail(lv);
    }

    compilers_.push_back(config);
  }
  delete root;
}

std::wstring DPEService::GetDefaultCompilerType(const ProgrammeLanguage& language)
{
  if (language == PL_PYTHON || language == PL_SCALA) return L"interpreter";
  return L"compiler";
}

scoped_refptr<Compiler> DPEService::CreateCompiler(
    std::wstring type, const std::wstring& version, ISArch arch,
    ProgrammeLanguage language, const std::vector<base::FilePath>& source_file)
{
  if (language == PL_UNKNOWN) language = DetectLanguage(source_file);
  if (language == PL_UNKNOWN) return NULL;

  //if (type.empty())
  {
    //type = GetDefaultCompilerType(language);
  }

  for (auto& it: compilers_)
  {
    //if (base::StringEqualCaseInsensitive(it.type_, type))
    {
      if (!it.Accept(language)) continue;

      if (arch != ARCH_UNKNOWN && arch != it.arch_) continue;

      if (base::StringEqualCaseInsensitive(it.type_, L"default") ||
          base::StringEqualCaseInsensitive(it.type_, L"compiler"))
      {
        return new BasicCompiler(it);
      }
      else if (base::StringEqualCaseInsensitive(it.type_, L"interpreter"))
      {
        return new InterpreterCompiler(it);
      }
    }
  }

  return NULL;
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

void  DPEService::HandleMulticast(uint32_t network, uint32_t ip, int32_t port, const std::string& data)
{
  std::vector<std::string> lines;
  Tokenize(data, "\r\n", &lines);

  const int n = static_cast<int>(lines.size());

  std::string action;
  std::string pa;
  std::string address;
  
  for (int32_t i = 0; i < n; ++i)
  {
    std::string& str = lines[i];

    const int len  = static_cast<int>(str.size());
    int pos = 0;
    while (pos < len && str[pos] != ':') ++pos;

    if (pos == len) continue;
    
    std::string key = str.substr(0, pos);
    std::string value = str.substr(pos+1);
    
    for (auto& c: key) c = tolower(c);
    
    if (key == "action") action = std::move(value);
    else if (key == "pa") pa = std::move(value);
    else if (key == "address") address = std::move(value);
  }
  
  if (action == "advertise")
  {
    if (!address.empty())
    {
      node_manager_.AddNode(address);
    }
  }
  else if (action == "bye")
  {
    if (!address.empty())
    {
      node_manager_.RemoveNode(address);
    }
  }
  else if (action == "search")
  {
    for (auto server: server_list_)
    {
      server->Advertise();
    }
  }
}

int32_t DPEService::handle_message(void* handle, const std::string& data)
{
  if (handle != ipc_sub_handle_) return 0;

  base::DictionaryValue rep;
  base::Value* v = base::JSONReader::Read(data.c_str(), base::JSON_ALLOW_TRAILING_COMMAS);
  rep.SetString("type", "ipc");
  rep.SetString("error_code", "-1");

  return 1;
}

}