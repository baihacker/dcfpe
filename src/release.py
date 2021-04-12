#! python2
import os
import sys
import shutil

if __name__ == '__main__':
  ENV_SOLUTION_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
  output = os.path.join(ENV_SOLUTION_DIRECTORY, 'dcfpe')
  if os.path.isdir(output):
    os.system('rmdir /S /Q %s'%output)
  os.makedirs(output)
  for dir in ["Release", "Release_x64"]:
    src_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'output\\' + dir)

    # Release rs
    dest_dir = os.path.join(output, 'rs\\' + dir)
    os.makedirs(dest_dir)
    shutil.copyfile(os.path.join(src_dir, 'msvcr120.dll'),
                    os.path.join(dest_dir, 'msvcr120.dll'))
    shutil.copyfile(os.path.join(src_dir, 'msvcp120.dll'),
                    os.path.join(dest_dir, 'msvcp120.dll'))
    shutil.copyfile(os.path.join(src_dir, 'zmq.dll'),
                    os.path.join(dest_dir, 'zmq.dll'))
    shutil.copyfile(os.path.join(src_dir, 'rs.exe'),
                    os.path.join(dest_dir, 'rs.exe'))
    rs_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'remote_shell')
    shutil.copyfile(os.path.join(rs_dir, 'deploy_demo.txt'),
                    os.path.join(dest_dir, 'deploy_demo.txt'))
    shutil.copyfile(os.path.join(rs_dir, 'deploy_demo_README.txt'),
                    os.path.join(dest_dir, 'deploy_demo_README.txt'))

    # Release dpe
    dest_dir = os.path.join(output, 'dpe\\' + dir)
    os.makedirs(dest_dir)
    shutil.copyfile(os.path.join(src_dir, 'msvcr120.dll'),
                    os.path.join(dest_dir, 'msvcr120.dll'))
    shutil.copyfile(os.path.join(src_dir, 'msvcp120.dll'),
                    os.path.join(dest_dir, 'msvcp120.dll'))
    shutil.copyfile(os.path.join(src_dir, 'zmq.dll'),
                    os.path.join(dest_dir, 'zmq.dll'))
    shutil.copyfile(os.path.join(src_dir, 'dpe.dll'),
                    os.path.join(dest_dir, 'dpe.dll'))
    shutil.copyfile(os.path.join(src_dir, 'pm.exe'),
                    os.path.join(dest_dir, 'pm.exe'))
    dpe_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'dpe')
    shutil.copyfile(os.path.join(dpe_dir, 'web\\Chart.bundle.js'),
                    os.path.join(dest_dir, 'Chart.bundle.js'))
    shutil.copyfile(os.path.join(dpe_dir, 'web\\index.html'),
                    os.path.join(dest_dir, 'index.html'))
    shutil.copyfile(os.path.join(dpe_dir, 'web\\jquery.min.js'),
                    os.path.join(dest_dir, 'jquery.min.js'))
    shutil.copyfile(os.path.join(dpe_dir, 'main.cc'),
                    os.path.join(dest_dir, 'main.cc'))
    shutil.copyfile(os.path.join(dpe_dir, 'dpe.h'),
                    os.path.join(dest_dir, 'dpe.h'))
  # Releae dpe
  dest_dir = os.path.join(output, 'dpe')
  dpe_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'dpe')
  shutil.copyfile(os.path.join(dpe_dir, 'README.md'),
                  os.path.join(dest_dir, 'README.md'))
  shutil.copyfile(os.path.join(dpe_dir, 'README_cn.txt'),
                  os.path.join(dest_dir, 'README_cn.txt'))
  # Release rs
  dest_dir = os.path.join(output, 'rs')
  rs_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'remote_shell')
  shutil.copyfile(os.path.join(rs_dir, 'README.md'),
                  os.path.join(dest_dir, 'README.md'))

  # Release dcfpe
  shutil.copyfile(os.path.join(ENV_SOLUTION_DIRECTORY, '../README.md'),
                  os.path.join(output, 'README.md'))
  sys.exit(0)

