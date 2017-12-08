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
      4309, 4267, 4541
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
      'target_name': 'rs',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'include_dirs': [
        '<(DEPTH)/third_party/protobuf-3.3.1/src',
      ],
      'sources':[
          'rs.cc',
          'server_node.h',
          'server_node.cc',
          'zserver.h',
          'zserver.cc',
          'command_executor.h',
          'command_executor.cc',
          'remote_server_node.h',
          'remote_server_node.cc',
          'local_server_node.h',
          'local_server_node.cc',
          'listener_node.h',
          'listener_node.cc',
          'message_sender.h',
          'message_sender.cc',
          'local_shell.h',
          'local_shell.cc',
          'batch_execute_shell.h',
          'batch_execute_shell.cc',
          'script_engine.h',
          'script_engine.cc',
          'proto/rs.pb.h',
          'proto/rs.pb.cc',
          'proto/deploy.pb.h',
          'proto/deploy.pb.cc',
        ],
      'dependencies':[
          '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
          '<(DEPTH)/process/process.gyp:process',
          '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
          '<(DEPTH)/third_party/protobuf-3.3.1/gyp/protobuf.gyp:protobuf',
        ],
    }
  ]
}
