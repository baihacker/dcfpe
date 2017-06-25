本文路径:https://github.com/baihacker/dcfpe/blob/master/src/dpe/README.txt
请通过该链接访问最新版本.

1. 程序框架
参考main.cc.
保证编译main.cc时能正常引用到dpe.h:
dpe.h在和main.cc同一目录下或编译器的include路径下.

2. 编译应用程序Binary (32bit,64bit均可)
MinGW:
g++ main.cc --std=c++11 -O3

通过CPP_INCLUDE_PATH环境变量配置编译器的include路径.

VC:
建立工程配置并包含路径后编译,或使用vc命令行编译.

sb:
https://github.com/baihacker/sb
通过配置compilers.json可以用pe++.py调用MinGW以及vc++.py调用vc进行编译

3. 部署 
保证在同一目录下有如下文件:
a.exe 2中输出的binary,demo中为main.exe
dpe.dll dpe支持dll (dpe实现)
zmq.dll zmq支持dll (消息库)
msvcp120.dll, msvcr120.dll C++运行时库
pm.exe 可选的ProcessMonitor,用于同时启动多个子进程,监控子进程,在被监控进程退出时重启子进程
index.html, Chart.bundle.js, jquery.min.js 可选的master结点监控页面,需要访问本页面时应该这些文件

在Master(或称为Server)和Worker结点上的部署方法相同

4. 命令行参考
在部署目录下执行命令(可以用shift+右键在菜单中选择"在此处打开命令窗口")
命令选项格式: --key=value或 -key value
(-的个数不影响命令,key不区分大小写)

4.1 启动Master或Worker结点:
a.exe [选项列表]

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
Master结点的ip,用于在Worker结点上指定Master结点.
该选项对Master结点无效.
默认值为本地第一个可用的ip.
Master结点和Worker在同一计算机时,可以不用指定本选项.

--p=port
--port=port
本结点的端口.
server默认值为3310.
worker默认值为3311.

--id=id
实例id,只对Worker结点生效.
用于标识同一计算机上部署的不同instance.
未使用port参数时,默认端口为 默认值+id.
默认值0.

--c=cacheFilePath
--cache=cacheFilePath
缓存文件路径.
使用默认缓存Reader和Writer时对应的路径.
注意:多个Worker在同一计算机上时会读写相同文件而导致问题.
可以通过在默认路径上追加id来使得Worker读写不同Cache.但是
一般不在Worker上使用缓存.
默认值为:dpeCache.txt.

--reset_cache=value
在新建默认缓存Writer时是否清空已有文件.
value=true或1认为值是true,其它值均认为是false.
默认值为:false

--http_port=port
--hp=port
用于master结点的结点监控页面端口.
默认值为:80

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

4.2 通过进程监视器启动Worker结点并监控其运行状态
pm.exe 目标进程路径 [选项列表] -- 子进程参数列表

--l=logLevel
--log=logLevel
日志级别,一般值取0或1.
默认级别1.

--id=id
--id=minId-maxId
子进程的id,id的含义见a.exe的选项列表.
--id=id被视作minId=id,maxId=id
通过该选项在同一计算机上启动多个子进程.
默认值minId=0,maxId=0.


其中 目标进程路径 和 选项列表 的顺序可以任意.
而-- 子进程参数列表必须是在最后.

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