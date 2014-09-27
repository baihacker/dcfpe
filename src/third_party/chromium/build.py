import os

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
os.chdir(CURRENT_DIRECTORY)

# input environment
ENV_SOLUTION_DIRECTORY = os.environ.get('ENV_SOLUTION_DIRECTORY', '')
ENV_GYP_DIRECTORY = os.environ.get('ENV_GYP_DIRECTORY', '')
ENV_COMPONENT = os.environ.get('ENV_COMPONENT', '')

# generate
# prepare gyp environment
if os.environ.get('ENV_GENERATE_PROJECT', '0') == '1':
  os.environ['GYP_MSVS_VERSION'] = '2013'
  os.environ['GYP_GENERATORS'] = 'msvs'
  os.environ['GYP_DEFINES'] = r'component='+ENV_COMPONENT
  os.environ['GYP_PATH'] = ENV_GYP_DIRECTORY
  where = os.path.join(CURRENT_DIRECTORY, r'.\build\gyp_chromium base\base.gyp')
  os.system(r'python ' + where + ' --depth=.')

# build
if os.environ.get('ENV_BUILD_PROJECT', '0') == '1':
  # we can not build base now
  pass

# copy
if os.environ.get('ENV_COPY_PROJECT', '0') == '1':
  third_party_dir = os.path.join(ENV_SOLUTION_DIRECTORY, r'third_party\build')

  for configuration in ['Debug', 'Release']:
    src_dir = os.path.join(CURRENT_DIRECTORY, 'build\\' + configuration)
    src_lib_dir = os.path.join(src_dir, r'lib')
    src_bin_dir = os.path.join(src_dir, r'bin')
    
    target_dir = os.path.join(third_party_dir, configuration)
    target_lib_dir = os.path.join(target_dir, 'lib')
    target_bin_dir = os.path.join(target_dir, 'bin')
    
    src_files = []
    src_files.append(os.path.join(target_lib_dir, 'base.lib'))
    src_files.append(os.path.join(target_lib_dir, 'base_static.lib'))
    src_files.append(os.path.join(target_bin_dir, 'base.dll'))
    src_files.append(os.path.join(target_bin_dir, 'base.pdb'))
    print src_files
