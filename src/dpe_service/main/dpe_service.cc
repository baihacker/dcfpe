#include "dpe_service/main/dpe_service.h"

#include <iostream>
using namespace std;
namespace ds
{

/*
*******************************************************************************
*/
DPEService::DPEService()
{
}

DPEService::~DPEService()
{
}

void DPEService::Start()
{
  // todo: register ipc channel
  ZServer* test_server = new ZServer();
  test_server->Start(MAKE_IP(127, 0, 0, 1));
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

int32_t DPEService::handle_message(int32_t handle, const char* msg, int32_t length)
{
  return 0;
}

}