#! python2
import os
import sys
import shutil


def copy_file_if_necessary(src, dest):
  if not os.path.isfile(dest):
    shutil.copyfile(src, dest)


def copy_file(src, dest):
  shutil.copyfile(src, dest)


def build_dependencies():
  # os.environ['GYP_GENERATORS'] = 'ninja'
  # There 3 ways to build chromium:
  # 1. build_chromium_old.py will use ".\build\gyp_chromium" to build it
  # 2. build_chromium.py will call gyp directly.
  # 3. build_dcfpe() will call gyp directly, the projects depends on chromium will be built automatically.
  # methods 1 and 2 will produce binary only.
  # so, we need not to build chromium here.
  # os.system(r'python third_party\chromium\build_chromium.py')
  # os.system(r'python third_party/zmq/build_zmq.py')

  # Build proto here
  includePath1 = os.path.join(ENV_SOLUTION_DIRECTORY,
                              r'third_party\protobuf-3.3.1\src')

  includePath2 = os.path.join(ENV_SOLUTION_DIRECTORY, r'dpe\proto')
  filePath = os.path.join(ENV_SOLUTION_DIRECTORY, r'dpe\proto\dpe.proto')
  outputPath = os.path.join(ENV_SOLUTION_DIRECTORY, r'dpe\proto')
  os.system(
      r'tools\protoc-3.3.0-win32\bin\protoc.exe --proto_path=%s --proto_path=%s %s --cpp_out=%s'
      % (includePath1, includePath2, filePath, outputPath))

  includePath2 = os.path.join(ENV_SOLUTION_DIRECTORY, r'remote_shell\proto')
  filePath = os.path.join(ENV_SOLUTION_DIRECTORY,
                          r'remote_shell\proto\rs.proto')
  outputPath = os.path.join(ENV_SOLUTION_DIRECTORY, r'remote_shell\proto')
  os.system(
      r'tools\protoc-3.3.0-win32\bin\protoc.exe --proto_path=%s --proto_path=%s %s --cpp_out=%s'
      % (includePath1, includePath2, filePath, outputPath))

  includePath2 = os.path.join(ENV_SOLUTION_DIRECTORY, r'remote_shell\proto')
  filePath = os.path.join(ENV_SOLUTION_DIRECTORY,
                          r'remote_shell\proto\deploy.proto')
  outputPath = os.path.join(ENV_SOLUTION_DIRECTORY, r'remote_shell\proto')
  os.system(
      r'tools\protoc-3.3.0-win32\bin\protoc.exe --proto_path=%s --proto_path=%s %s --cpp_out=%s'
      % (includePath1, includePath2, filePath, outputPath))


def prepare_path():
  if os.environ['GYP_GENERATORS'] == 'ninja':
    sys.path.insert(0, os.path.join(ENV_SOLUTION_DIRECTORY, 'build'))
    import vs_toolchain
    vs2013_runtime_dll_dirs = vs_toolchain.SetEnvironmentAndGetRuntimeDllDirs()

  sys.path.insert(0, os.environ.get('ENV_GYP_DIRECTORY'))


def build_dcfpe():
  os.environ['GYP_GENERATORS'] = 'ninja'
  os.environ['GYP_MSVS_VERSION'] = '2013'
  prepare_path()
  # base.gyp depends on this directory
  sys.path.insert(
      1, os.path.join(ENV_SOLUTION_DIRECTORY, 'third_party/chromium/build'))
  import gyp

  print 'build dcfpe'
  print 'Generators=' + os.environ['GYP_GENERATORS']
  print 'MSVSVersion=' + os.environ['GYP_MSVS_VERSION']
  print 'Component=' + os.environ['ENV_COMPONENT']

  args = []
  args.append('build/dcfpe.gyp')
  args.append('--depth=.')
  args.append('--no-circular-check')
  args.extend(['-G', 'output_dir=' + os.environ['ENV_BUILD_DIR']])
  args.extend(['-D', 'component=' + os.environ.get('ENV_COMPONENT')])
  args.extend(['-D', 'build_dir=' + os.environ.get('ENV_BUILD_DIR')])

  # do not use common.gypi, because it conflicts with third_party\chromium\build\common.gypi
  # the above comment does not take effect since 6a9af727adc6 on Sat Oct 25 20:35:48 2014
  # When revert the changes to common.gypi in chromium
  # I keep this comment here to emphasize that we can add a default gypi file here

  args.append('-I' + os.path.join(ENV_SOLUTION_DIRECTORY, 'build/common.gypi'))
  ret = gyp.main(args)

  dest_dir = os.path.join(ENV_SOLUTION_DIRECTORY,
                          os.environ.get('ENV_BUILD_DIR'))

  for sfx in ["", "_x64"]:
    debug_dir = os.path.join(dest_dir, 'Debug' + sfx)
    release_dir = os.path.join(dest_dir, 'Release' + sfx)
    if not os.path.exists(debug_dir):
      os.makedirs(debug_dir)
    if not os.path.exists(release_dir):
      os.makedirs(release_dir)

    # copy crt
    src_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'third_party/vc/crt' + sfx)
    copy_file_if_necessary(os.path.join(src_dir, 'msvcr120d.dll'),
                           os.path.join(debug_dir, 'msvcr120d.dll'))
    copy_file_if_necessary(os.path.join(src_dir, 'msvcp120d.dll'),
                           os.path.join(debug_dir, 'msvcp120d.dll'))
    copy_file_if_necessary(os.path.join(src_dir, 'msvcr120.dll'),
                           os.path.join(release_dir, 'msvcr120.dll'))
    copy_file_if_necessary(os.path.join(src_dir, 'msvcp120.dll'),
                           os.path.join(release_dir, 'msvcp120.dll'))

    # copy dpe web resources
    src_dir = os.path.join(ENV_SOLUTION_DIRECTORY, 'dpe/web')
    for filename in ['index.html', 'Chart.bundle.js', 'jquery.min.js']:
      copy_file(os.path.join(src_dir, filename),
                os.path.join(release_dir, filename))
      copy_file(os.path.join(src_dir, filename),
                os.path.join(debug_dir, filename))

  return ret


if __name__ == '__main__':
  ENV_SOLUTION_DIRECTORY = os.path.dirname(os.path.realpath(__file__))

  # platform and tools
  os.environ['ENV_TARGET_OS'] = 'win'
  os.environ['ENV_TARGET_PLATFORM'] = 'x86'
  os.environ['ENV_TOOLS_DIRECTORY'] = os.path.join(ENV_SOLUTION_DIRECTORY,
                                                   'tools')
  os.environ['ENV_GYP_DIRECTORY'] = os.path.join(ENV_SOLUTION_DIRECTORY,
                                                 r'tools/gyp/pylib')

  # solution
  os.environ['ENV_SOLUTION_DIRECTORY'] = ENV_SOLUTION_DIRECTORY
  os.environ['ENV_COMPONENT'] = 'static_library'
  os.environ['ENV_BUILD_DIR'] = 'output'

  # control
  os.environ['ENV_GENERATE_PROJECT'] = '1'
  os.environ['ENV_BUILD_PROJECT'] = '0'  # we do not support auto build now
  os.environ['ENV_COPY_PROJECT_OUTPUT'] = '1'

  # win toolchain configurations
  os.environ['ENV_WIN_TOOL_CHAIN_DATA'] = os.path.join(
      ENV_SOLUTION_DIRECTORY, 'build/win_toolchain.json')
  os.environ['ENV_WIN_TOOL_CHAIN_SCRIPT'] = os.path.join(
      ENV_SOLUTION_DIRECTORY, 'build/vs_toolchain.py')

  # build dependences
  build_dependencies()

  rc = build_dcfpe()

  sys.exit(rc)

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
