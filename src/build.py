import os
import sys
import shutil

def copy_file_if_necessary(src, dest):
  if not os.path.isfile(dest): shutil.copyfile(src, dest)

def build_dependencies():
  os.system(r'python third_party\chromium\build_chromium.py')
  os.system(r'python third_party\zmq\build_zmq.py')

def build_zmq_test():
  args = []
  args.append('zmq_demo\\zmq_demo.gyp')
  args.append('--depth=.')
  args.append('--no-circular-check')
  args.extend(['-D', 'gyp_output_dir=out'])
  args.extend(['-D', 'component='+os.environ.get('ENV_COMPONENT')])
  args.extend(['-D', 'build_dir='+os.environ.get('ENV_BUILD_DIR')])
  args.append('-I'+os.path.join(ENV_SOLUTION_DIRECTORY, 'build\\common.gypi'))
  ret = gyp.main(args)
  
  # zmq test (vs 2010)
  dest_dir = os.path.join(ENV_SOLUTION_DIRECTORY, os.environ.get('ENV_BUILD_DIR'))
  src_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'third_party\\zmq\\bin')
  debug_dir = os.path.join(dest_dir, 'Debug')
  release_dir = os.path.join(dest_dir, 'Release')
  if not os.path.exists(debug_dir): os.makedirs(debug_dir)
  if not os.path.exists(release_dir): os.makedirs(release_dir)
  copy_file_if_necessary(os.path.join(src_dir, 'libzmq-v100-mt-gd-4_0_4.dll'),
                         os.path.join(debug_dir, 'libzmq-v100-mt-gd-4_0_4.dll'))
  copy_file_if_necessary(os.path.join(src_dir, 'libzmq-v100-mt-4_0_4.dll'),
                         os.path.join(release_dir, 'libzmq-v100-mt-4_0_4.dll'))
  return ret

def build_dcfpe():
  args = []
  args.append('build\\dcfpe.gyp')
  args.append('--depth=.')
  args.append('--no-circular-check')
  args.extend(['-D', 'gyp_output_dir=out'])
  args.extend(['-D', 'component='+os.environ.get('ENV_COMPONENT')])
  args.extend(['-D', 'build_dir='+os.environ.get('ENV_BUILD_DIR')])
  args.append('-I'+os.path.join(ENV_SOLUTION_DIRECTORY, 'build\\common.gypi'))
  ret = gyp.main(args)
  return ret
  
if __name__ == '__main__':
  ENV_SOLUTION_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
  
  # platform and tools
  os.environ['ENV_TARGET_OS'] = 'win'
  os.environ['ENV_TARGET_PLATFORM'] = 'x86'
  os.environ['ENV_GYP_DIRECTORY'] = os.path.abspath(os.path.join(ENV_SOLUTION_DIRECTORY, r'tools\gyp\pylib'))
  
  # solution
  os.environ['ENV_SOLUTION_DIRECTORY'] = ENV_SOLUTION_DIRECTORY
  os.environ['ENV_COMPONENT'] = 'static_library'
  os.environ['ENV_BUILD_DIR'] = 'output'
  
  # control
  os.environ['ENV_GENERATE_PROJECT'] = '1'
  os.environ['ENV_BUILD_PROJECT'] = '0'         # we do not support auto build now
  os.environ['ENV_COPY_PROJECT_OUTPUT'] = '1'
  
  # build dependences
  build_dependencies()
  
  # build solution
  os.environ['GYP_MSVS_VERSION'] = '2010'
  os.environ['GYP_GENERATORS'] = 'msvs'
  
  sys.path.insert(0, os.environ.get('ENV_GYP_DIRECTORY'))
  import gyp
  
  #build_dcfpe()
  
  sys.exit(build_zmq_test())

# ##
#{
#'RULE_INPUT_ROOT': '$(InputName)',
#'PRODUCT_DIR': '$(OutDir)',
#'SHARED_INTERMEDIATE_DIR': '$(OutDir)obj/global_intermediate',
#'RULE_INPUT_DIRNAME': '$(InputDir)',
#'RULE_INPUT_EXT': '$(InputExt)',
#'gyp_outpu_dir': 'out',
#'MSVS_VERSION': '2013',
#'EXECUTABLE_PREFIX': '',
#'GENERATOR': 'msvs',
#'SHARED_LIB_SUFFIX': '.dll',
#'LIB_DIR': '$(OutDir)lib',
#'EXECUTABLE_SUFFIX': '.exe',
#'SHARED_LIB_PREFIX': '',
#'STATIC_LIB_PREFIX': '',
#'component': 'shared_library',
#'MSVS_OS_BITS': 64,
#'CONFIGURATION_NAME': '$(ConfigurationName)',
#'INTERMEDIATE_DIR': '$(IntDir)',
#'RULE_INPUT_PATH': '$(InputPath)',
#'RULE_INPUT_NAME': '$(InputFileName)',
#'STATIC_LIB_SUFFIX': '.lib',
#'OS': 'win'}
# ##