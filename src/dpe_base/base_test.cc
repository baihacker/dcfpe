#include "utility_interface.h"
#include <iostream>
using namespace std;

InterfacePtr<IMessageCenter>  msg_center;

struct MessageHandler : public DPESingleInterfaceObjectRoot<IMessageHandler>
{
  int32_t handle_message(
        int32_t channel_id, int32_t is_ctrl, const char* msg, int32_t length) override
  {
    cerr << "have_message" << endl;
    cerr << is_ctrl << endl;
    cerr << msg << endl;
    return 1;
  }
};

int main()
{
  CreateUtility(INTERFACE_MESSAGE_CENTER, msg_center.storage());
  MessageHandler handler;
  msg_center->add_message_handler(&handler);
  msg_center->start();
  msg_center->send_ctrl_message("123", 4);
  Sleep(200);
  msg_center->stop();
  
  
  return 0;
}