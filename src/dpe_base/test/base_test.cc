#include <iostream>
#include <windows.h>
#include <process.h>

#include "dpe_base/dpe_base.h"

using namespace std;
struct MessageHandler : public base::MessageHandler
{
virtual int32_t handle_message(int32_t handle, const char* msg, int32_t length)
{
  cerr << "begin message" << endl;
  cerr << msg << endl;
  cerr << "end message" << endl;
  
  return 0;
}
};
MessageHandler g_msg_handler;
void HelloFile()
{
  cerr << "hello FILE" << endl;
}

void HelloUI()
{
  cerr << "hello UI" << endl;
  base::zmq_message_center()->AddMessageHandler(&g_msg_handler);
  base::ThreadPool::PostTask(base::ThreadPool::FILE, FROM_HERE,
        base::Bind(HelloFile));

  base::MessageCenter* center = base::zmq_message_center();
  int32_t p = center->RegisterSender("tcp://127.1.1.1:1234");
  int32_t q = center->RegisterReceiver("tcp://127.1.1.1:1234");
  //cerr << p << " " << q << endl;
  for (int i = 0; i < 3; ++i)
  cerr << base::zmq_message_center()->SendMessage(p, "123000", 7) << endl;
  base::quit_main_loop();
}

int main()
{
  base::dpe_base_main(HelloUI);
  return 0;
}
