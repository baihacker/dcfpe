import os
import sys
import shutil

if __name__ == '__main__':
  CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
  os.chdir(CURRENT_DIRECTORY)
  
  # input environment
  ENV_SOLUTION_DIRECTORY = os.environ.get('ENV_SOLUTION_DIRECTORY', '')
  ENV_GYP_DIRECTORY = os.environ.get('ENV_GYP_DIRECTORY', '')
  ENV_COMPONENT = os.environ.get('ENV_COMPONENT', '')
  ENV_BUILD_DIR = os.environ.get('ENV_BUILD_DIR', '')
  
  # generate
  # prepare gyp environment
  if os.environ.get('ENV_GENERATE_PROJECT', '0') == '1':
    pass
  
  # build
  if os.environ.get('ENV_BUILD_PROJECT', '0') == '1':
    pass
  
  # copy
  if os.environ.get('ENV_COPY_PROJECT_OUTPUT', '0') == '1':
    src_dir = os.path.join(CURRENT_DIRECTORY, 'zmq_4.0.4')
    
    # copy binaries if necessary
    src_bin_dir = os.path.join(src_dir, 'bin')
    dest_bin_dir = os.path.join(CURRENT_DIRECTORY, 'bin')
    if not os.path.isdir(dest_bin_dir): os.makedirs(dest_bin_dir)
    for file in os.listdir(src_bin_dir):
      if file[-4:] == '.dll':
        s = os.path.join(src_bin_dir, file)
        d = os.path.join(dest_bin_dir, file)
        if not os.path.isfile(d):
          shutil.copyfile(s, d)
    
    # copy libs if necessary
    src_lib_dir = os.path.join(src_dir, 'lib')
    dest_lib_dir = os.path.join(CURRENT_DIRECTORY, 'lib')
    if not os.path.isdir(dest_lib_dir): os.makedirs(dest_lib_dir)
    for file in os.listdir(src_lib_dir):
      if file[-4:] == '.lib':
        s = os.path.join(src_lib_dir, file)
        d = os.path.join(dest_lib_dir, file)
        if not os.path.isfile(d):
          shutil.copyfile(s, d)
    
    #copy include all
    src_include_dir = os.path.join(src_dir, 'include')
    dest_include_dir = os.path.join(CURRENT_DIRECTORY, 'include')
    if os.path.exists(dest_include_dir): shutil.rmtree(dest_include_dir, True)
    shutil.copytree(src_include_dir, dest_include_dir)
    
  sys.exit(0)