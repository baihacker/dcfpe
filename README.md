dcfpe
=====

Distributed Computation Framework for Project Euler

It is a tool to make computer compute together.


Brief Component List:
====================

Host:
* Discover workers from LAN.
* Discover workers from server. 
* Add workers manual.
* Comunnicate with Worker
* Maintain the H process and R process.
* Maintain the worker list. (View Workers, Network Quality, Worker ability(memory limit, process count limitation), Worker status(Memory usage, CPU usage, process count))
* Distribute task effectively, and manage them. Reliable.
* Receive tasks from H process and distribute them to W process. (H process -> Host -> Worker -> W process)
* Receive result from W process and send them to R process. (W process -> Worker -> Host -> R process)

Worker:
* Announce it is joining or leaving the LAN.
* Register it on the server.
* Communicate with Host.
* Maintain the W process.
* Receive tasks from Worker and pass them to W process.
* Receive result from W process and send them to Host.

H process:
* Generate tasks and send to Host

W process:
* Compute and send result to Worker

R process:
* Receive results from Host and combine them.

Other:
======

* Main language is C++11. It is possible to use python, lua, php, javascript.
* Basic network libary: ZMQ
* Worker will protect the W process. (sandbox)
* Trust mechanism between Host and Worker. (It is possible that the worker will receive a virus from Host).
* I do not when to release the first version because I am very lazy.

Development Environment:
========================
## windows
* win7 64
* python 2.7
* vs2013 or above (or win_toolchain 2013e in chromium)
* [win_toolchain_2013e download] (http://yun.baidu.com/share/link?shareid=2799405881&uk=2684621311)

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



Updated:
========
* 2014.09.22 Upload gyp (basic project management tool) and chromium/base (basic code, especially the thread model).
* 2014.10.21 23:43:16 Compute the square ofr [1..10] by dpe model. 8225 lines, 213248 Bytes.
