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
      'target_name': 'dpe_service',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'include_dirs': [
          '<(DEPTH)\\third_party\\wtl\\Include',
        ],
      'sources':[
          'main/dpe_service_resource.rc',
          'main/dpe_service_dialog.h',
          'main/dpe_service_dialog.cc',
          
          'main/main.cc',
          'main/dpe_service.h',
          'main/dpe_service.cc',
          'main/zserver.h',
          'main/zserver.cc',
          
          'main/zserver_client.h',
          'main/zserver_client.cc',
          
          'main/resource.h',
          
          'main/compiler/compiler.h',
          'main/compiler/compiler_impl.h',
          'main/compiler/compiler_impl.cc',
          
          'main/dpe_model/dpe_device.h',
          'main/dpe_model/dpe_device_impl.h',
          'main/dpe_model/dpe_device_impl.cc',
          
          'main/dpe_model/dpe_project.h',
          'main/dpe_model/dpe_project.cc',
          
          'main/dpe_model/dpe_compiler.h',
          'main/dpe_model/dpe_compiler.cc',
          
          'main/dpe_model/dpe_scheduler.h',
          'main/dpe_model/dpe_scheduler.cc',
          
          'main/dpe_model/dpe_controller.h',
          'main/dpe_model/dpe_controller.cc',
          
          'main/dpe_model/dpe_shell.h',
          'main/dpe_model/dpe_shell.cc',
        ],
      'dependencies':[
          '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
          '<(DEPTH)/process/process.gyp:process',
          '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
        ],
      'msvs_settings': {
      'VCLinkerTool': {
        'SubSystem': 'Windows',
      },
    },
    },
    {
      'target_name': 'base_test',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['base_test/base_test.cc'],
      'dependencies':[
          '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
          '<(DEPTH)/process/process.gyp:process',
          '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
        ],
    },
    {
      'target_name': 'base_test1',
      'type': 'executable',
      'variables': {
        'use_zmq': 0,
      },
      'sources':['base_test/base_test1.cc'],
      'dependencies':[
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
        ],
    },
    {
      'target_name': 'publisher',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['zmq_demo/publisher.cc'],
      'dependencies':[
        '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
        '<(DEPTH)/third_party/chromium/base/base.gyp:base',
        '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
        ],
    },
    {
      'target_name': 'subscriber',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['zmq_demo/subscriber.cc'],
      'dependencies':[
        '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
        '<(DEPTH)/third_party/chromium/base/base.gyp:base',
        '<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
        ],
    },
  ]
}
