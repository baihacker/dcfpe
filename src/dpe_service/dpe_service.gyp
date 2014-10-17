{
  'includes':[
    '..\\build\\common.gypi',
    '..\\build\\win_precompile.gypi',
    '..\\dpe_base\\support_zmq.gypi',
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
      'target_name': 'dpe_service',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':[
          'main\\main.cc',
          'main\\dpe_service.h',
          'main\\dpe_service.cc',
          'main\\zserver.h',
          'main\\zserver.cc',
        ],
      'dependencies':[
          '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
          '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
          '<(DEPTH)\\process\\process.gyp:process',
        ],
      'msvs_settings': {
      'VCLinkerTool': {
        #'SubSystem': 'Windows',
      },
    },
    },
    {
      'target_name': 'base_test',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['base_test\\base_test.cc'],
      'dependencies':[
          '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
          '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
          '<(DEPTH)\\process\\process.gyp:process',
        ],
    },
    {
      'target_name': 'publisher',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['zmq_demo\\publisher.cc'],
      'dependencies':[
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
        '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
    },
    {
      'target_name': 'subscriber',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['zmq_demo\\subscriber.cc'],
      'dependencies':[
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
        '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
    },
  ]
}