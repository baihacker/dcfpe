#! python2
import os
import sys
import shutil

HOMEDIR = os.environ.get('HOMEDIR', '')


def main(argv):
  print 'Install dpe'
  if len(HOMEDIR) == 0:
    print "Cannot find HOMEDIR"
    sys.exit(-1)

  # src dir
  ENV_SOLUTION_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
  src_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'output\\Release_x64')
  dpe_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'dpe')

  # modules and main.cc
  dest_dir = os.path.join(HOMEDIR, 'usr\\bin\\dpe')
  if os.path.isdir(dest_dir):
    os.system('rmdir /S /Q %s' % dest_dir)
  os.makedirs(dest_dir)

  shutil.copyfile(os.path.join(src_dir, 'zmq.dll'),
                  os.path.join(dest_dir, 'zmq.dll'))
  shutil.copyfile(os.path.join(src_dir, 'dpe.dll'),
                  os.path.join(dest_dir, 'dpe.dll'))
  shutil.copyfile(os.path.join(dpe_dir, 'web\\Chart.bundle.js'),
                  os.path.join(dest_dir, 'Chart.bundle.js'))
  shutil.copyfile(os.path.join(dpe_dir, 'web\\index.html'),
                  os.path.join(dest_dir, 'index.html'))
  shutil.copyfile(os.path.join(dpe_dir, 'web\\jquery.min.js'),
                  os.path.join(dest_dir, 'jquery.min.js'))
  shutil.copyfile(os.path.join(dpe_dir, 'main.cc'),
                  os.path.join(dest_dir, 'main.cc'))

  # includes
  dest_dir = os.path.join(HOMEDIR, 'usr\\include')
  shutil.copyfile(os.path.join(dpe_dir, 'dpe.h'),
                  os.path.join(dest_dir, 'dpe.h'))


if __name__ == '__main__':
  main(sys.argv)
