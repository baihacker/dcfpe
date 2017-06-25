dcfpe
=====

Distributed Computation Framework for Project Euler

It is a tool to make computer compute together.

[Download releases](http://pan.baidu.com/s/1o8fqAqI) pwd:235f

Overview:
====================

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

Other:
======

* Main language is C++11. It is possible to use python, lua, php, javascript.
* Basic network libary: ZMQ
* Worker will protect the W process. (sandbox)
* Trust mechanism between Host and Worker. (It is possible that the worker will receive a virus from Host).

Development Environment:
========================
## windows
* win7 64 or above
* python 2.7
* vs2013 or above (or win_toolchain 2013e in chromium)
* [win_toolchain_2013e download](http://yun.baidu.com/share/link?shareid=2799405881&uk=2684621311)

### Build by vs2013
* Make sure os.environ['GYP_GENERATORS'] = 'msvs' and os.environ['GYP_MSVS_VERSION'] = '2013' in build.py
* Run command: python build.py
* Open zmq_demo\zmq_demo.sln by vs2013
* Build solution by vs2013

### Build by ninja with win_toolchain
* Config win_toolchain, modify src\build\win_toolchain.json
* Make sure os.environ['GYP_GENERATORS'] = 'ninja' in build.py.(ignore os.environ['GYP_MSVS_VERSION'])
* Run command: python build.py
* Run command: ninja -C output\Release

## linux
* ubuntu 14.04 x86

### Build
* Make sure os.environ['GYP_GENERATORS'] = 'ninja' in build.py.(ignore os.environ['GYP_MSVS_VERSION'])
* Run command: python build.py
* Run command: ninja -C output\Release


Updated:
========
* 2014.09.22 Upload gyp (basic project management tool) and chromium/base (basic code, especially the thread model).
* 2014.10.21 23:43:16 Compute the square of [1..10] by dpe model. 8225 lines, 213248 Bytes.
* 2015.05.18 17:17:24 Compute the square of [1..100] by dpe model on two computers. 5011+5465+927=11403 lines, 131459+147496+25257=304212 Bytes.
* 2017.06.11 A new compute framework called dpe. 1633 lines code. Compute 0^2 + 1^2 + 2^2 + ... + 9^2 = 285 in distributed environment.
* 2017.06.12 Succeeded in generating .dll .h for x86, x64.
* 2017.06.18 Release [dpe_1.0.0.0](https://github.com/baihacker/dcfpe/tree/master/releases) and [dpe_1.0.0.1](https://github.com/baihacker/dcfpe/tree/master/releases).
