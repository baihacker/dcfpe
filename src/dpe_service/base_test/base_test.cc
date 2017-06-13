#include <iostream>
#include <windows.h>
#include <process.h>

#include "dpe_base/dpe_base.h"
#include "process/process.h"

using namespace std;
struct MessageHandler : public base::MessageHandler
{
virtual int32_t handle_message(void* handle, const std::string& data)
{
  cerr << "time" << clock() << endl;
  cerr << "begin message" << endl;
  cerr << data << endl;
  cerr << "end message" << endl;
  return 0;
}
};

struct ORZServer : public base::RequestHandler
{
virtual std::string handle_request(base::ServerContext& context)
{
  cerr << "time" << clock() << endl;
  cerr << "begin request" << endl;
  cerr << context.data_ << endl;
  cerr << "end request" << endl;
  
  return "I received";
}
};

void handle_response(scoped_refptr<base::ZMQResponse> rep)
{
  cerr << "time" << clock() << endl;
  cerr << "begin response" << endl;
  cerr << rep->data_ << endl;
  cerr << "end response" << endl;
}

MessageHandler  g_msg_handler;
ORZServer       g_server;
void HelloFile()
{
  cerr << "hello FILE" << endl;
}

struct ProcessTest : public process::ProcessHost
{
  void OnStop(process::Process*, process::ProcessContext* context)
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
  void OnOutput(process::Process*, bool is_std_out, const std::string& data)
  {
    cerr << "process output:" << endl;
    cerr << data << endl;
  }
  void RunTest()
  {
    p = new process::Process(this);
    p->GetProcessOption().image_path_ = base::FilePath(L"D:\\Projects\\a.exe");
    p->GetProcessOption().redirect_std_inout_ = true;
    p->GetProcessOption().treat_err_as_out_ = false;
    p->GetProcessOption().job_time_limit_ = 5000;
    //p->GetProcessOption().job_memory_limit_ = 100*1024*1024;
    cerr << p->Start() << endl;
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
  void* p = center->RegisterChannel(base::CHANNEL_TYPE_PUB, "tcp://127.1.1.1:1234", true);
  void* q = center->RegisterChannel(base::CHANNEL_TYPE_SUB, "tcp://127.1.1.1:1234", false);

  Sleep(30);
  for (int i = 0; i < 3; ++i)
  cerr << base::zmq_message_center()->SendMessage(p, "123000", 7) << endl;

  test = new ProcessTest();
  test->RunTest();
  
  auto* s = base::zmq_server();
  s->StartServer("tcp://127.1.1.1:12345", &g_server);
  Sleep(30);
  
  auto* c = base::zmq_client();
  c->SendRequest("tcp://127.1.1.1:12345", "hello server",
    13, base::Bind(&handle_response), 0);
}

int main()
{
  base::dpe_base_main(HelloUI);
  return 0;
}
