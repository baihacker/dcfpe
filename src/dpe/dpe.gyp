{
  'includes':[
    '../build/common.gypi',
    '../build/win_precompile.gypi',
    '../dpe_base/support_zmq.gypi',
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
      'target_name': 'dpe',
      'type': 'shared_library',
      'variables': {
        'use_zmq': 1,
      },
      'defines': [
          'DPE_IMPLEMENTATION',
        ],
      'sources':[
          'dpe.h',
          'dpe_internal.h',
          'lib.cc',
          'zserver.h',
          'zserver.cc',
          'remote_node_impl.h',
          'remote_node_impl.cc',
          'dpe_node_base.h',
          'dpe_master_node.h',
          'dpe_master_node.cc',
          'dpe_worker_node.h',
          'dpe_worker_node.cc',
          'compute_model.h',
          'compute_model.cc',
          'dpe_export.def',
        ],
      'dependencies':[
          '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
          '<(DEPTH)/process/process.gyp:process',
          '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
        ],
    },
    {
      'target_name': 'main',
      'type': 'executable',
      'sources':[
          'dpe.h',
          'main.cc',
        ],
      'dependencies':[
          '<(DEPTH)/dpe/dpe.gyp:dpe',
        ],
    },
  ]
}
