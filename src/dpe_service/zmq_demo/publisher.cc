#include <zmq.h>

int main()
{
	void* context = zmq_ctx_new();
	void* sender = zmq_socket(context, ZMQ_REQ);
	int rc = zmq_connect(sender, "tcp://127.0.0.1:5678");
	for (int msg = 1;;++msg)
	{
		char buff[4096] = R"({"type":"rsc","dest":"remote","src":"local","cookie":"demo_cookie","request":"CreateDPEDevice"})";
		zmq_send(sender, buff, strlen(buff)+1, 0);
    int n = zmq_recv(sender, buff, 4096, 0);
    buff[n] = 0;
    puts(buff);
		Sleep(10000);
	}
  zmq_close(sender);
  zmq_ctx_term(context);
	return 0;
}