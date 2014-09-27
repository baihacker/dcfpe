import os
import sys

ENV_SOLUTION_DIRECTORY = os.path.dirname(os.path.realpath(__file__))

# platform and tools
ENV_GYP_DIRECTORY = os.path.abspath(os.path.join(ENV_SOLUTION_DIRECTORY, r'tools\gyp\pylib'))
os.environ['ENV_TARGET_OS'] = 'win'
os.environ['ENV_TARGET_PLATFORM'] = 'x86'
os.environ['ENV_GYP_DIRECTORY'] = ENV_GYP_DIRECTORY

# solution
ENV_COMPONENT = 'static_library'  # shared_library or static_library
ENV_BUILD_DIR = 'build'
os.environ['ENV_SOLUTION_DIRECTORY'] = ENV_SOLUTION_DIRECTORY
os.environ['ENV_COMPONENT'] = ENV_COMPONENT
os.environ['ENV_BUILD_DIR'] = ENV_BUILD_DIR

# control
os.environ['ENV_GENERATE_PROJECT'] = '1'
os.environ['ENV_BUILD_PROJECT'] = '0'         # we do not support auto build now
os.environ['ENV_COPY_PROJECT_OUTPUT'] = '1'

# build dependences
os.system(r'python third_party\chromium\build.py')

# build main solution
os.environ['GYP_MSVS_VERSION'] = '2010'
os.environ['GYP_GENERATORS'] = 'msvs'

sys.path.insert(0, ENV_GYP_DIRECTORY)
import gyp
args = []
args.append('dcfpe.gyp')
args.append('--depth=.')
args.append('--no-circular-check')
args.append('-D')
args.append('gyp_output_dir=out')
args.append('-D')
args.append('component='+ENV_COMPONENT)
args.append('-D')
args.append('build_dir='+os.environ.get('ENV_BUILD_DIR'))
args.append('-Icommon.gypi')
gyp.main(args)

###
{
'RULE_INPUT_ROOT': '$(InputName)',
'PRODUCT_DIR': '$(OutDir)',
'SHARED_INTERMEDIATE_DIR': '$(OutDir)obj/global_intermediate',
'RULE_INPUT_DIRNAME': '$(InputDir)',
'RULE_INPUT_EXT': '$(InputExt)',
'gyp_outpu_dir': 'out',
'MSVS_VERSION': '2013',
'EXECUTABLE_PREFIX': '',
'GENERATOR': 'msvs',
'SHARED_LIB_SUFFIX': '.dll',
'LIB_DIR': '$(OutDir)lib',
'EXECUTABLE_SUFFIX': '.exe',
'SHARED_LIB_PREFIX': '',
'STATIC_LIB_PREFIX': '',
'component': 'shared_library',
'MSVS_OS_BITS': 64,
'CONFIGURATION_NAME': '$(ConfigurationName)',
'INTERMEDIATE_DIR': '$(IntDir)',
'RULE_INPUT_PATH': '$(InputPath)',
'RULE_INPUT_NAME': '$(InputFileName)',
'STATIC_LIB_SUFFIX': '.lib',
'OS': 'win'}
###