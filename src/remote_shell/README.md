Remote shell
============

A windows remote shell allows you to run command remotely.

Usage
=====
## Remote computer
* Command: rs --log=0 -b
* Description: start a listen on remote computer.
* Note: You can remove "--log=0" for performance issue. "--log=0" will allow to show all logs.

## Local computer
* Command: rs -t=\<IP of remote computer\>
* Description: start a session to the remote computer.
* Note1: IP can be found in the log of the remote computer.
* Note2: The listener on the remote computer will start a process to handle the request from local computer.
* Note3: It is supported to start another session to the same remote computer.

Command reference
=================
## DOS commands
All dos command is supported, please enter it directly.

## Run the commands on the local computer
* Just put l and a space before the command you want to run. e.g. l dir will run dir command locally.

## File operation
* fs \<local file path 1\> \<local file path 2\> ...: send the files to remote computer. The target dir is the current dir of remote prodcess.
* fg \<remote file path 1\> \<remote file path 2\> ...: get the files from remote computer.

## Exit remote shell
* exit or q to quit.
