{
  'includes':[
    '../build/common.gypi',
    '../build/win_precompile.gypi',
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
        'utility/utility_impl.cc',
        
        'utility/repeated_action.h',
        'utility/repeated_action.cc',
        
        # thread pool
        'thread_pool.h',
        'thread_pool/thread_pool_impl.h',
        'thread_pool/thread_pool_impl.cc',
        
        # io
        'io_handler.h',
        'io/io_handler.cc',
        'pipe.h',
        'io/pipe.cc',
        
        # zmq adapter
        'zmq_adapter.h',
        'zmq/zmq_adapter.cc',
        'zmq/msg_center.cc',
        'zmq/zmq_server.cc',
        'zmq/zmq_client.cc',
        
        # main
        'dpe_base.h',
        'dpe_base.cc',
        ],
      'dependencies':[
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
        ],
    },
  ]
}
