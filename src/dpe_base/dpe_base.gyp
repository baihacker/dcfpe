{
  'includes':[
    '..\\build\\common.gypi',
    '..\\build\\win_precompile.gypi',
    'support_zmq.gypi',
  ],
  'variables':{
  },
  'target_defaults': {
    'include_dirs': [
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'AdditionalLibraryDirectories': [
          ],
      },
    },
    'configurations': {
      
      'Release_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
            ],
          },
        },
      },
      
      'Debug_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
            ],
          },
        },
      },
    }
  },
  'targets':[
    {
      'target_name': 'dpe_base',
      'type': 'static_library',
      'sources':[
        'utility\\utility_impl.cc',
        'utility\\message_center_impl.cc'
        ],
      'dependencies':[
          '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
      # if some module dependences on dpe_base, then it depends on base
      'export_dependent_settings': [
          '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
    },
    {
      'target_name': 'base_test',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['test\\base_test.cc'],
      'dependencies':[
          'dpe_base',
        ],
    },
  ]
}