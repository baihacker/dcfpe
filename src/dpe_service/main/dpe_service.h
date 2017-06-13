#ifndef DPE_SERVICE_MAIN_DPE_SERVICE_H_
#define DPE_SERVICE_MAIN_DPE_SERVICE_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/zserver.h"
#include "dpe_service/main/resource.h"
#include "dpe_service/main/compiler/compiler.h"
#include "dpe_service/main/dpe_model/dpe_device.h"
#include "dpe_service/main/dpe_model/dpe_controller.h"

#include <windows.h>
#include <winsock2.h>

namespace ds
{
class DPENodeManager;
class DPEService;
class CDPEServiceDlg;
class DPENode : public base::RefCounted<DPENode>
{
friend class DPENodeManager;
friend class CDPEServiceDlg;
public:
  DPENode(int32_t node_id, const std::string& address, bool is_local = false);
  ~DPENode();

  void  SetIsLocal(bool is_local){is_local_ = is_local;}
  int32_t GetNodeId() const {return node_id_;}
  
  base::WeakPtr<DPENode>  GetWeakPtr(){return weakptr_factory_.GetWeakPtr();}
  scoped_refptr<ZServerClient>  GetZClient(){return zclient_;}
  bool operator == (const DPENode& other) const {return server_address_ == other.server_address_;}

private:
  int32_t                                       node_id_;
  std::string                                   server_address_;
  bool                                          is_local_;
  std::string                                   pa_;
  base::TimeDelta                               response_time_;
  int32_t                                       response_count_;
  scoped_refptr<ZServerClient>                  zclient_;
  base::WeakPtrFactory<DPENode>                 weakptr_factory_;
};

class DPEGraphObserver
{
public:
  virtual ~DPEGraphObserver(){}
  virtual void OnServerListUpdated() = 0;
};

class DPENodeManager
{
friend class DPEService;
public:
  DPENodeManager(DPEService* dpe);
  ~DPENodeManager();

  void  AddObserver(DPEGraphObserver* ob);
  void  RemoveObserver(DPEGraphObserver* ob);
  
  void  AddNode(int32_t ip, int32_t port = kServerPort);
  void  AddNode(const std::string& address);
  void  RemoveNode(int32_t ip, int32_t port = kServerPort, bool remove_local = false);
  void  RemoveNode(const std::string& address, bool remove_local = false);
  const std::vector<scoped_refptr<DPENode> >& NodeList() const {return node_list_;}
  DPENode*  GetNodeById(const int32_t id) const;
  
private:
  static void  HandleResponse(base::WeakPtr<DPENodeManager> self,
                int32_t node_id,
                scoped_refptr<base::ZMQResponse> rep);
  void  HandleResponseImpl(int32_t node_id, scoped_refptr<base::ZMQResponse> rep);

private:
  DPEService*                                   dpe_;
  std::vector<scoped_refptr<DPENode> >          node_list_;
  int32_t                                       next_node_id_;
  std::vector<DPEGraphObserver*>                observer_list_;
  base::WeakPtrFactory<DPENodeManager>          weakptr_factory_;
};

class DPEService : public base::MessageHandler
{
friend class CDPEServiceDlg;
public:
  DPEService();
  ~DPEService();
  
  void Start();
  void WillStop();
  DPENodeManager& GetNodeManager() {return node_manager_;}
  void SetLastOpen(const base::FilePath& path);
  void SaveConfig();
  const std::string& DefaultServerAddress() const {return server_address_;}
  void test_action();
private:
  void StopImpl();
  void LoadConfig();
  void LoadCompilers(const base::FilePath& file);

public:
  // resource management
  std::wstring  GetDefaultCompilerType(const ProgrammeLanguage& language);
  
  scoped_refptr<Compiler> CreateCompiler(
          std::wstring type, const std::wstring& version, ISArch arch,
          ProgrammeLanguage language, const std::vector<base::FilePath>&
          source_file = std::vector<base::FilePath>()
        );

  scoped_refptr<DPEDevice> CreateDPEDevice(
          ZServer* zserver, base::DictionaryValue* request
          );

  void  RemoveDPEDevice(DPEDevice* device);

  void  HandleMulticast(uint32_t network, uint32_t ip, int32_t port, const std::string& data);
  
private:
  int32_t handle_message(void* handle, const std::string& data) override;

private:
  std::string                 ipc_sub_address_;
  void*                       ipc_sub_handle_;
  
  base::FilePath              config_path_;
  std::string                 config_server_address_;
  std::string                 server_address_;
  std::string                 last_open_;
  
  std::vector<ZServer*>       server_list_;
  std::vector<DPEDevice*>     dpe_device_list_;
  DPENodeManager              node_manager_;
  
  // config
  base::FilePath              home_dir_;
  base::FilePath              config_dir_;
  base::FilePath              temp_dir_;
  
  // resource
  std::vector<CompilerConfiguration>  compilers_;
  
  // dialog
  scoped_refptr<CDPEServiceDlg>  dlg_;
};
}

#endif