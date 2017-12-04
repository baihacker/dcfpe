dpe
===
A C++ library allows you to leverage distributed environment to compute.

Design
======
Updated Jun 12, 2017, [history version](https://github.com/baihacker/dcfpe/blob/master/docs/design_diary.txt).

MasterNode:
* Receives connection from workers. (duplex communication connection).
* Uses a MasterTaskScheduler to maintain a task queue, schedule task to workers, handle request/connection error. SimpleMasterTaskScheduler uses a very simple strategy to schedule tasks.
* MasterTaskScheduler uses a Solver to initialize the tasks (just holds the id of task), and tell the solver when a task is finished or all the tasks are completed.

WorkerNode:
* Connects to MasterNode.
* Uses WorkerTaskExecuter to execute task. WorkerTaskExecuter will dispatch task to correct thread anc call the corresponding compute method provide by Solver.

How to use:
* Provides a .dll, .h and the client code can use it.
* Support x86 and x64.

Usage
=====
See [README_cn.txt](https://github.com/baihacker/dcfpe/blob/master/src/dpe/README_cn.txt)
