#include "process_impl.h"
#include <process.h>

namespace process{

int ProcessImpl::next_ip_ = 1;

ProcessImpl::ProcessImpl() :
  process_status_(STATUS_PREPARE)
{
  CreateUtility(INTERFACE_MESSAGE_CENTER, msg_center_.storage());
}

ProcessImpl::~ProcessImpl()
{
}

int32_t ProcessImpl::AddObserver(IProcessObserver* observer)
{
  for (auto it: observers_) if (observer == it)
  {
    return DPE_OK;
  }
  observer->AddRef();
  observers_.push_back(observer);
  return DPE_OK;
}

int32_t ProcessImpl::RemoveObserver(IProcessObserver* observer)
{
  std::remove_if(observers_.begin(), observers_.end(),
    [=](IProcessObserver* x)
    {
      if (x == observer)
      {
        x->Release();
        return 1;
      }
      return 0;
    }
    );
  return DPE_FAILED;
}

int32_t ProcessImpl::SetImagePath(const wchar_t* image)
{
  if (process_status_ != STATUS_PREPARE)
  {
    return DPE_FAILED;
  }
  
  if (image && image[0])
  {
    image_path_ = image;
    return DPE_OK;
  }
  return DPE_FAILED;
}

int32_t ProcessImpl::SetArgument(const wchar_t* arg)
{
  if (process_status_ != STATUS_PREPARE)
  {
    return DPE_FAILED;
  }
  
  if (arg)
  {
    argument_ = arg;
    return DPE_OK;
  }
  return DPE_FAILED;
}

int32_t ProcessImpl::AppendArgumentItem(const wchar_t* arg_item)
{
  if (process_status_ != STATUS_PREPARE)
  {
    return DPE_FAILED;
  }
  
  if (arg_item)
  {
    if (arg_item[0])
    {
      argument_list_.push_back(arg_item);
    }
    return DPE_OK;
  }
  return DPE_FAILED;
}

int32_t ProcessImpl::Start()
{
  return 0;
}


int32_t ProcessImpl::Stop()
{
  if (process_status_ != STATUS_STOPPED)
  {
    return DPE_OK;
  }
  if (process_status_ != STATUS_RUNNING)
  {
    return DPE_FAILED;
  }
  
  return DPE_OK;
}

int32_t ProcessImpl::GetNextIP()
{
  if (next_ip_ < MAX_IP_ADDRESS)
  {
    next_ip_++;
  }
  next_ip_ = 1;
  return MAX_IP_ADDRESS;
}

std::string GetAddress(int32_t ip)
{
  char address[64];
  sprintf(address, "tcp://%d.%d.%d.%d:%d",
      ip>>24, (ip>>16)&255, (ip>>8)&255, ip&255, IPC_PORT);
  return address;
}

}