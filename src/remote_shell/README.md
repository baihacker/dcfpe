Remote shell
============

A windows remote shell allows you to run command remotely.

Usage
=====
## Remote computer
#### Start the listener on remote computer
* Command: rs --log=0 -b
* Note: You can remove "--log=0" for performance issue. "--log=0" will allow to show all logs.

## Local computer
#### Start a shell on remote computer
* Command: rs -t=\<IP of remote computer\>
* Note1: IP can be found in the log of the remote computer.
* Note2: The listener on the remote computer will start a process to handle the request from local computer.
* Note3: It is supported to start another session to the same remote computer.
#### Execute script
* Command: rs -f=<script file name> -action=<action parameter of the script>
* Script example:
```proto
name: "test_deploy"
resource {
  files: "orz.c"
  files: "abc.dll"
}
deploys {
  hosts: "192.168.0.1"
  hosts: "192.168.0.2"
  target_dir: "test_deploy"
  deploy: "g++ test_deploy/orz.c -o test_deploy/orz.exe"
  deploy: "test_deploy\\orz.exe
}
```
* A real [deploy example](https://github.com/baihacker/dcfpe/blob/master/src/remote_shell/deploy_demo.txt).

Command reference
=================
## Command option
* option \<option1\> \<option2\> ...
  * wait_for_command: for each command wait for the remote process to stop.
  * remote_show_output, remote_show_error: whether to show the output / error output on remote process.
  * local_show_output, local_show_error: whether to show the output / error output on local shell. if wait_for_command is false, these two options will not take effect.
  * no_wait_for_command, no_remote_show_output, no_remote_show_error, no_local_show_output, no_local_show_error: the corresponding opposite options.

## DOS commands
All dos command is supported, please enter it directly.

## Run the commands on the local computer
* Just put l and a space before the command you want to run. e.g. l dir will run dir command locally.

## File operation
* fs \<local file path 1\> \<local file path 2\> ...: send the files to remote computer. The target dir is the current dir of remote prodcess.
* fst \<local file path 1\> \<remote file path 1\> \<local file path 2\> \<remote file path 2\> ...: send one file to remote computer while the remote path is specified.
* fg \<remote file path 1\> \<remote file path 2\> ...: get the files from remote computer.
* fgt \<remote file path 1\> \<local file path 1\> \<remote file path 2\> \<local file path 2\> ...: get the files from remote computer while the local path is specified.

## Exit remote shell
* exit or q to quit.
