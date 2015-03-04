# this script work isolately with .\build\gyp_chromium
import os
import sys

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
    os.environ['GYP_MSVS_VERSION'] = '2013'
    os.environ['GYP_GENERATORS'] = 'msvs'
    os.environ['GYP_DEFINES'] = r'component='+ENV_COMPONENT+' build_dir="..\\..\\'+ENV_BUILD_DIR+'"'
    os.environ['GYP_PATH'] = ENV_GYP_DIRECTORY
    where = os.path.join(CURRENT_DIRECTORY, r'.\build\gyp_chromium base\base.gyp')
    os.system(r'python ' + where + ' --depth=.')
  
  # build
  if os.environ.get('ENV_BUILD_PROJECT', '0') == '1':
    pass
  
  # copy
  if os.environ.get('ENV_COPY_PROJECT_OUTPUT', '0') == '1':
    pass
    
  sys.exit(0)