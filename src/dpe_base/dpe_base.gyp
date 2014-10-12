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
        #chromium
        'chromium_base.h',
        
        #interface
        'interface_base.h',
        'utility_interface.h',
        'utility\\utility_impl.cc',
        'utility\\message_center_impl.cc',
        
        #thread pool
        'thread_pool.h',
        'thread_pool\\thread_pool_impl.h',
        'thread_pool\\thread_pool_impl.cc',
        
        #main
        'dpe_base.h',
        'dpe_base.cc',
        ],
      'dependencies':[
          '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
      # if a module dependences on dpe_base, then it depends on base
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