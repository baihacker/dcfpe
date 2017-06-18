1. 程序框架
参考main.cc。
保证dpe.h在同一目录下。

2. 编译Binary (32bit,64bit均可)
环境：MinGW或VC

MinGW:
g++ main.cc --std=c++11 -O3

3. 部署 
保证在同一目录下有如下文件:
a.exe 2中输出的binary
dpe.dll dpe支持dll (dpe实现)
zmq.dll zmq支持dll (消息库)
msvcp120.dll, msvcr120.dll C++运行时库
pm.exe ProcessMonitor,可选,用于监控进程,在被监控进程退出时重启

在Master(或称为Server)和Worker结点上的部署方法相同

4. 命令行参考
格式: --key=value或 -key value
(-的个数不影响命令,key不区分大小写)

4.1 a.exe [选项列表]

--l=logLevel
--log=logLevel
日志级别,一般值取0或1.
默认级别1.

--t=type
--type=type
结点类型,值为server或worker.
默认值server.

--ip=ip
本结点的ip.
默认值为本地第一个可用的ip.

--server_ip=server_ip
Master结点的ip.
该选项对Master结点无效.

--p=port
--port=port
本结点的端口.
server默认值为3310
worker默认值为3311

--id=id
实例id.
只对worker结点生效.
默认值0.
用于标识在一结点上不同的instance.
未使用port参数时,默认端口为 默认值+id

示例:
在同一台计算机上

Master结点:
a.exe --l=0

Worker结点:

部署1个worker结点
a.exe --l=0 -type=worker

部署2个worker结点
a.exe --l=0 -type=worker --id=0
a.exe --l=0 -type=worker --id=1

在不同计算机上

Master结点:
a.exe --l=0

Worker结点:
部署1个worker结点
a.exe --l=0 -type=worker --server_ip=<server ip>

部署2个worker结点
a.exe --l=0 -type=worker --id=0 --server_ip=<server ip>
a.exe --l=0 -type=worker --id=1 --server_ip=<server ip>

4.2 pm.exe 目标进程路径 [选项列表] -- [子进程选项]

--l=logLevel
--log=logLevel
日志级别,一般值取0或1.
默认级别1.

--id=id
--id=minId-maxId
子进程的id,见a.exe的选项列表.
--id=id被视作minId=id,maxId=id
默认值minId=0,maxId=0
该选项决定子进程数目.

其中 目标进程路径 和 选项列表的顺序可以任意

4.3 例子

4.3.1 在一台计算机上部署1个master,2个worker.
4.3.1.1
main.exe --log=0
4.3.1.2
pm.exe main.exe --log=0 --id=0-1 -- --log=0 --type=worker

4.3.2 在一台计算机部署1个master,在另一台计算机上部署2个worker
4.3.2.1
main.exe --log=0
4.3.2.2
pm.exe main.exe --log=0 --id=0-1 -- --log=0 --type=worker --server_ip=192.168.137.128