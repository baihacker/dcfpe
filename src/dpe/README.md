# dpe
A C++ library allows you to leverage distributed environment to compute.

# Design
Updated Apr 11, 2021, [history version](https://github.com/baihacker/dcfpe/blob/master/docs/design_diary.txt).

## Library structures:
* Core
  * dpe.dll The binary that implements the main scheduling logic.
  * dpe.h The user includes this file and implements the Solver.
* Example
  * main.cc An examepl of using this library.
  * main.exe The generated example binary.
* Dependencies
  * msvcp120.dll, msvcr120.dll, zmq.dll
* Web UI on Master node
  * index.html, Chart.bundle.js, jquery.min.js are expected to the folder where dpe.dll exists.

## MasterNode:
* Initializes the solver as master and retrieves the tasks (int64). The task's status is PENDING.
  * If possible, loads the saved state: if a task's status in the cache is DONE, the cached status is copied.
* Receives GetTaskRequest from worker nodes and assigns tasks to the requestor. A task is marked as RUNNING.
* Receives FinishComputeRequest from worker nodes and the task is marked as DONE.

## WorkerNode:
* Connects to MasterNode.
* Sends GetTaskRequest to get new tasks and execute them.
* When a task is finished, sends FinishComputeRequest to save the result and Sends GetTaskRequest to get more tasks.

# Usage
See [README_cn.txt](https://github.com/baihacker/dcfpe/blob/master/src/dpe/README_cn.txt)
