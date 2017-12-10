1. Setup remote rs nodes
1.1 On target nodes, run "rs -b -l=0" (-l=0 is optional)
1.2 It's also OK if you have onlye one node.

2. Deploy demo to your nodes.
2.1 Config the "local_dir" of your resources.
2.1.1 If your target binary is 32-bit, please remove the _x64 suffix.
2.2 Config "hosts" fields of master.
2.3 Config "hosts" fields of worker.
2.4 If you have only one node for both master and worker, please make sure the target_dir fields are different for master and worker config.
2.5 Change the "pe++.py deploy_demo/main.cc -o deploy_demo/main.exe" if necessary.
2.5.1 "pe++.py" may be replaced by "g++" if you don't have the sb system.
2.5.2 The src and output path is determined by your target_dir.
2.6 Change the "deploy_demo\\main.exe -l=0 -id=0 --type=worker --server_ip=192.168.137.128" If necessary.
2.6.1 "deploy_demo\\main.exe" is determined by the previous commands.
2.7 Run "rs -f=deploy_demo.txt -action=deploy"