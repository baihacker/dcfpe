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
      'type': '<(component)',
      'variables': {
        'use_zmq': 1,
      },
      'defines': [
          'DPE_BASE_IMPLEMENTATION',
        ],
      'sources':[
        # export
        'dpe_base_export.h',
      
        # chromium
        'chromium_base.h',
        
        # interface
        'interface_base.h',
        'utility_interface.h',
        'utility\\utility_impl.cc',
        
        # thread pool
        'thread_pool.h',
        'thread_pool\\thread_pool_impl.h',
        'thread_pool\\thread_pool_impl.cc',
        
        # zmq adapter
        'zmq\\zmq_adapter.cc',
        'zmq_adapter.h'
        
        
        # main
        'dpe_base.h',
        'dpe_base.cc',
        ],
      'dependencies':[
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
          '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
    },
  ]
}