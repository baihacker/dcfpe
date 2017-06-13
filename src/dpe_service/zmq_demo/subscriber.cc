#include <zmq.h>

#include "dpe_base/chromium_base.h"

int main()
{
	void* context = zmq_ctx_new();
	void* recver = zmq_socket(context, ZMQ_SUB);
	int rc = zmq_bind(recver, "tcp://127.0.0.1:3357");
	zmq_setsockopt (recver, ZMQ_SUBSCRIBE, "", 0);
	for (;;)
	{
		char buff[1024];
		int len = zmq_recv(recver, buff, 1024, 0);
		if (len > 0)
		{
			puts(buff);
		}
	}
	return 0;
}