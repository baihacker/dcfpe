#ifndef DPE_SERVICE_DPE_SERVICE_H_
#define DPE_SERVICE_DPE_SERVICE_H_

#include "dpe_base/dpe_base.h"
#include "dpe_service/main/zserver.h"
#include "dpe_service/main/resource.h"
#include "dpe_service/main/compiler/compiler.h"
#include "dpe_service/main/dpe_model/dpe_device.h"
#include "dpe_service/main/dpe_model/dpe_controller.h"

namespace ds
{
// ipc message handling: bind and receive
// ipc message notification: bind and send
class DPEService : public base::MessageHandler
{
public:
  DPEService();
  ~DPEService();
  
  void Start();
  void WillStop();

  std::wstring  GetDefaultCompilerType(int32_t language);
  
  scoped_refptr<Compiler> CreateCompiler(
    std::wstring type, const std::wstring& version, int32_t arch,
    int32_t language, const std::vector<base::FilePath>& source_file = std::vector<base::FilePath>());

  scoped_refptr<DPEDevice> CreateDPEDevice(
      ZServer* zserver, base::DictionaryValue* request);

  void  RemoveDPEDevice(DPEDevice* device);

private:
  void StopImpl();
  
  void LoadConfig();
  
  void LoadCompilers(const base::FilePath& file);

private:
  int32_t handle_message(int32_t handle, const std::string& data) override;

private:
  std::string                 ipc_sub_address_;
  int32_t                     ipc_sub_handle_;
  
  std::vector<ZServer*>       server_list_;
  std::vector<DPEDevice*>     dpe_device_list_;

  // config
  base::FilePath              home_dir_;
  base::FilePath              config_dir_;
  base::FilePath              temp_dir_;
  
  // resource
  std::vector<CompilerConfiguration>  compilers_;
};
}

#endif