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
    'msvs_disabled_warnings': [
      4309, 4267
    ],
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
      'include_dirs': [
        '<(DEPTH)/third_party/protobuf-3.3.1/src',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
        },
      },
      'sources':[
          'dpe.h',
          'dpe_internal.h',
          'dpe.cc',
          'zserver.h',
          'zserver.cc',
          'dpe_master_node.h',
          'dpe_master_node.cc',
          'dpe_worker_node.h',
          'dpe_worker_node.cc',
          'dpe_export.def',

          'proto/dpe.pb.h',
          'proto/dpe.pb.cc',

          'http_server.h',
          'http_server.cc',
        ],
      'dependencies':[
          '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
          '<(DEPTH)/process/process.gyp:process',
          '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
          '<(DEPTH)/third_party/protobuf-3.3.1/gyp/protobuf.gyp:protobuf',
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
    {
      'target_name': 'pm',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':[
          'pm.cc',
        ],
      'dependencies':[
          '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
          '<(DEPTH)/process/process.gyp:process',
          '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
        ],
    }
  ]
}
