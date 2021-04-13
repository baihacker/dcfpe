本文路径:https://github.com/baihacker/dcfpe/blob/master/src/dpe/README_cn.md

请通过该链接访问最新版本.

# 1. 程序框架
参考main.cc

# 2. 编译应用程序Binary (32bit, 64bit)
确认编译main.cc时能正常引用到dpe.h, 执行编译命令即可. 一般情况下dpe.h在和main.cc同一目录下或编译器的include路径下.
## MinGW
* g++ main.cc --std=c++11 -O3
* 通过CPP_INCLUDE_PATH环境变量配置编译器的include路径.

## VC
* 建立工程并配置并包含路径后编译
* 或者直接使用vc命令行编译.

## [sb](https://github.com/baihacker/sb)
* 通过配置compilers.json可以用pe++.py调用MinGW或者用vc++.py调用vc进行编译。

# 3. 部署
在Master(或称为Server)和Worker结点上的部署方法相同.
* 主程序 a.exe (2中输出的binary, demo中为main.exe)的位置没有特殊要求
* 依赖项应该当在LoadLibraryA("dpe.dll")API所能发现的位置. 一般说来放在主程序的目录下或者PATH环境变量所包含的某个目录下. 更详细的情况请参考[LoadLibraryA](https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya#parameters)
  * dpe.dll dpe支持dll (dpe实现)
  * zmq.dll zmq支持dll (消息库)
  * msvcp120.dll, msvcr120.dll C++运行时库
* Web UI 所在位置和dpe.dll相同
  * index.html
  * Chart.bundle.js
  * jquery.min.js
* 目前状态文件state.txtproto的保存和主程序相同.

同一台机器上部署单个worker或多个worker
* 支持在同一台机上部署多个worker, 但在Master结点上被视为同一个结点, 因为目前以ip作为worker结点的唯一标识符.
* 如果在一台机器上部署的多个worker不需要共享状态, 可以在同一台机器上部署多个worker，但需要执行多次部署命令.
* 如果每个worker需要一些预计算的结果, 而且该结点占用大量内存时，部署多个worker可能导致内存不够用，这时需要考虑只部署一个worker，而该worker可以并行处理多个task. 由于不同机器支持的最佳线程数不同, thread_number参数会被传给Solver的Compute方法，而在Compute的实现中可以根据该参数启用不同的线程数量. 例如,使用openmp时可以使用num_threads(thread_number).

# 4. 命令行参考
在主程序目录下执行命令(可以用shift+右键在菜单中选择"在此处打开命令窗口")
 * a.exe [选项列表]
 * 其中单个选项格式为 --key=value或 -key value (-的个数不影响命令,key不区分大小写).

## 4.1参数列表
* 日志级别
  * --l=logLevel
  * --log=logLevel
  * 一般值取0或1. 级别可设置为-1，此时输出更多的日志.
  * 默认级别0.

* 结点类型
  * --t=type
  * --type=type
  * 值为server或worker.
  * 默认值server.

* 本结点ip
  * --ip=ip
  * 值为x.x.x.x的ipv4格式
  * 默认值为本地第一个可用的ip,一般不用指定本参数.

* Master结点ip
  * --server_ip=server_ip
  * 默认值为本地第一个可用的ip.
  * Master结点和Worker在同一计算机时,可以不用指定本选项.

* Master的监听端口
  * --sp=port
  * --server_port=port
  * 默认值为3310.

* Worker上的线程数量
  * --tn=thread_number
  * --thread_number=thread_num
  * Worker结点
     * Worker结点用于执行Solver::Compute的线程数
     * 小于1的值是非法的,在Master结点上也会检查该值
  * 默认值1.

* Solver::Compute负载
  * --bs=batch_size
  * --batch_size=batch_size
  * Worker结点
    * batch_size大于0时,指定Worker节点上Solver::Compute执行的最大task数
    * batch_size小于0时,|batch_size|为worker结点上Solver::Compute的期望执行时间(单位:秒)
    * 0值是非法的.
  * 默认值1.

* 自定义并行参数
  * --pi=parallel_info
  * --parallel_info=parallel_info
  * Worker结点
    * 用户自定义的parallel_info可以用于从命令行向Compute方法传递额外的信息
  * 默认值0.

* 是否读取上次保存的状态
  * --rs=one of {true, false, 0, 1}
  * --read_state=one of {true, false, 0, 1}
  * Master结点
    * 是否读取上次保存的状态.
  * 默认值true.

* http服务端口
  * --hp=port
  * --http_port=port
  * Master结点
    * 用于Master结点的结点监控页面端口.
  * 默认值为:80

## 4.2 例子
### 同一台计算机
* Master结点: a.exe --l=0
* Worker结点: a.exe --l=0 -type=worker

### 不同计算机
* Master结点: a.exe --l=0
* Worker结点: a.exe --l=0 -type=worker --server_ip=\<server ip\>
