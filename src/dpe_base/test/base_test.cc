#include <iostream>
#include <windows.h>
#include <process.h>

#include "dpe_base/dpe_base.h"
#include "process/process.h"

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

struct ProcessTest : public process::ProcessHost
{
  void OnStop(process::ProcessContext* context)
  {
    cerr << "process exit " << endl;
    cerr << "exit reason " << context->exit_reason_ <<endl;
    cerr << "exit code " << context->exit_code_ << endl;
    cerr << "output size " << context->output_size_ << endl;
    cerr << "time usage " << context->time_usage_ << endl;
    cerr << "time usage user " << context->time_usage_user_ << endl;
    p = NULL;
    delete this;
  }
  void OnOutput(bool is_std_out, const char* buffer, int32_t size)
  {
    cerr << "process output:" << endl;
    cerr << buffer << endl;
  }
  void RunTest()
  {
    p = new process::Process(this);
    p->GetProcessOption().image_path_ = L"D:\\Projects\\a.exe";
    p->GetProcessOption().redirect_std_inout_ = true;
    p->GetProcessOption().treat_err_as_out_ = false;
    p->GetProcessOption().job_time_limit_ = 1000;
    //p->GetProcessOption().job_memory_limit_ = 100*1024*1024;
    p->Start();
  }
  
  scoped_refptr<process::Process> p;
};

ProcessTest* test;

void HelloUI()
{
  cerr << "hello UI" << endl;
  base::zmq_message_center()->AddMessageHandler(&g_msg_handler);
  base::ThreadPool::PostTask(base::ThreadPool::FILE, FROM_HERE,
        base::Bind(HelloFile));

  base::MessageCenter* center = base::zmq_message_center();
  int32_t p = center->RegisterChannel("tcp://127.1.1.1:1234", true, true);
  int32_t q = center->RegisterChannel("tcp://127.1.1.1:1234", false, false);

  Sleep(30);
  for (int i = 0; i < 3; ++i)
  cerr << base::zmq_message_center()->SendMessage(p, "123000", 7) << endl;

  test = new ProcessTest();
  test->RunTest();
}

int main()
{
  base::dpe_base_main(HelloUI);
  return 0;
}
