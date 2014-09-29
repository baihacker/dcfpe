import os
import sys

if __name__ == '__main__':
  CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
  os.chdir(CURRENT_DIRECTORY)
  
  # prepare ninja
  use_ninja = False
  if os.environ.get('GYP_GENERATORS', '') == 'ninja':
    use_ninja = True

  if use_ninja:
    tool_chain_script = os.environ.get('ENV_WIN_TOOL_CHAIN_SCRIPT', '')
    tool_chain_dir = os.path.dirname(tool_chain_script)
    sys.path.insert(0, tool_chain_dir)
    import vs_toolchain

  # prepare gyp
  sys.path.insert(0, os.environ.get('ENV_GYP_DIRECTORY'))
  sys.path.insert(0, os.path.join(CURRENT_DIRECTORY, 'build'))
  import gyp

  # input environment
  ENV_SOLUTION_DIRECTORY = os.environ.get('ENV_SOLUTION_DIRECTORY', '')
  
  # generate
  # prepare gyp environment
  if os.environ.get('ENV_GENERATE_PROJECT', '0') == '1':
    if use_ninja:
      vs2013_runtime_dll_dirs = vs_toolchain.SetEnvironmentAndGetRuntimeDllDirs()
    args=[];
    args.append(os.path.join(CURRENT_DIRECTORY, r'.\base\base.gyp'))
    args.append('--depth=.')
    args.extend(['-D', 'component='+os.environ['ENV_COMPONENT']])
    args.extend(['-D', 'build_dir=./../../'+os.environ['ENV_BUILD_DIR']])
    args.extend(['-G', 'output_dir=./../../'+os.environ['ENV_BUILD_DIR']])
    #args.append('-I'+os.path.join(CURRENT_DIRECTORY, 'build\\common.gypi'))
    gyp.main(args)
  
  # build
  if os.environ.get('ENV_BUILD_PROJECT', '0') == '1':
    pass
  
  # copy
  if os.environ.get('ENV_COPY_PROJECT_OUTPUT', '0') == '1':
    pass
    
  sys.exit(0)