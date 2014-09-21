import os
gyp_path = os.path.dirname(os.path.realpath(__file__))
gyp_path = os.path.abspath(os.path.join(gyp_path, r'..\..\tools\gyp\pylib'))
os.environ.setdefault('GYP_MSVS_VERSION', '2013')
os.environ.setdefault('GYP_GENERATORS', 'msvs')
os.environ.setdefault('GYP_DEFINES', 'component=shared_library')
os.environ.setdefault('GYP_PATH', gyp_path)
print gyp_path
os.system(r'python build\gyp_chromium base\base.gyp --depth=.')
