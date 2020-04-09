# dpe
A C++ library allows you to leverage distributed environment to compute.

# Design
Updated Jun 12, 2017, [history version](https://github.com/baihacker/dcfpe/blob/master/docs/design_diary.txt).

MasterNode:
* Receives connection from workers. (duplex communication connection).
* Uses a MasterTaskScheduler to maintain a task queue, schedule task to workers, handle request/connection error. SimpleMasterTaskScheduler uses a very simple strategy to schedule tasks.
* MasterTaskScheduler uses a Solver to initialize the tasks (just holds the id of task), and tell the solver when a task is finished or all the tasks are completed.

WorkerNode:
* Connects to MasterNode.
* Uses WorkerTaskExecuter to execute task. WorkerTaskExecuter will dispatch task to correct thread anc call the corresponding compute method provide by Solver.

Usage design:
* Provides a .dll, .h and the client code can use it.
* Support x86 and x64.

# Other:
* Main language is C++11. It is possible to use python, lua, php, javascript.
* Basic network libary: ZMQ
* Worker will protect the W process. (sandbox)
* Trust mechanism between Host and Worker. (It is possible that the worker will receive a virus from Host).

# Usage
See [README_cn.txt](https://github.com/baihacker/dcfpe/blob/master/src/dpe/README_cn.txt)
