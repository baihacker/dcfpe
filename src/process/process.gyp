{
  'includes':[
    '../build/common.gypi',
    '../build/win_precompile.gypi',
    '../dpe_base/support_zmq.gypi',
  ],
  'targets':[
    {
      'target_name': 'process',
      'type': 'static_library',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['process_impl/process_impl.cc'],
      'dependencies':[
          '<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
          '<(DEPTH)/third_party/chromium/base/base.gyp:base',
        ],
    },
  ]
}
