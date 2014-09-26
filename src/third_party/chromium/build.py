import os

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
os.chdir(CURRENT_DIRECTORY)

# input environment
ENV_SOLUTION_DIRECTORY = os.environ.get('ENV_SOLUTION_DIRECTORY', '')
ENV_GYP_DIRECTORY = os.environ.get('ENV_GYP_DIRECTORY', '')
ENV_COMPONENT = os.environ.get('ENV_COMPONENT', '123')

# prepare gyp environment
os.environ['GYP_MSVS_VERSION'] = '2010'
os.environ['GYP_GENERATORS'] = 'msvs'
os.environ['GYP_DEFINES'] = r'component='+ENV_COMPONENT
os.environ['GYP_PATH'] = ENV_GYP_DIRECTORY

# build base.gyp
where = os.path.join(CURRENT_DIRECTORY, r'.\build\gyp_chromium base\base.gyp')
os.system(r'python ' + where + ' --depth=.')

