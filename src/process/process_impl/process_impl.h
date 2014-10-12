#include "process\process.h"

#include <vector>
#include <string>

#include <windows.h>

#include "third_party/zmq/include/zmq.h"
#include "third_party/zmq/include/zmq_utils.h"

namespace process{
const int IPC_PORT = 10231;
const int MAX_IP_ADDRESS = 127 << 24 | 255 << 16 | 255 << 8 | 254;  // 127.255.255.254
enum
{
  STATUS_PREPARE = 1,
  STATUS_RUNNING = 2,
  STATUS_STOPPED = 3,
};

class ProcessImpl : public DPESingleInterfaceObjectRoot<IProcess>
{
public:
  ProcessImpl();
  ~ProcessImpl();
  
  int32_t AddObserver(IProcessObserver* observer) override;
  int32_t RemoveObserver(IProcessObserver* observer) override;
  int32_t SetImagePath(const wchar_t* image) override;
  int32_t SetArgument(const wchar_t* arg) override;
  int32_t AppendArgumentItem(const wchar_t* arg_item) override;
  
  int32_t Start() override;
  int32_t Stop() override;
  
private:
  static int32_t GetNextIP();
  static std::string GetAddress(int32_t ip);

private:
  std::vector<IProcessObserver*>  observers_;
  int32_t                   process_status_;
  
  // cmd = image_path_ argument "argument_list_[0]" "argument_list_[1]" ...
  std::wstring              image_path_;
  std::wstring              argument_;
  std::vector<std::wstring> argument_list_;
  
  static int32_t            next_ip_;
};
}
