#include "third_party/zmq/include/zmq.h"
#include "third_party/zmq/include/zmq_utils.h"
#include "dpe_base/chromium_base.h"
int main()
{
	void* context = zmq_ctx_new();
	void* sender = zmq_socket(context, ZMQ_PUB);
	int rc = zmq_connect(sender, "tcp://127.0.0.1:3357");
	base::debug::Alias(NULL);
	for (int msg = 1;;++msg)
	{
		char buff[64];
		sprintf(buff, "message id = %d", msg);
		zmq_send(sender, buff, strlen(buff)+1, 0);
		Sleep(2000);
	}
	return 0;
}