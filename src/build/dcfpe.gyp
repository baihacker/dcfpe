{
  'includes':[
    'common.gypi',
    'win_precompile.gypi'
  ],
  'targets':[
    {
      'target_name': 'all',
      'type': 'none',
      'sources':[],
      'dependencies':[
        #'<(DEPTH)/dpe_base/dpe_base.gyp:dpe_base',
        #'<(DEPTH)/process/process.gyp:process',
        #'<(DEPTH)/dpe_service/dpe_service.gyp:dpe_service',
        #'<(DEPTH)/dpe_service/dpe_service.gyp:base_test',
        #'<(DEPTH)/dpe_service/dpe_service.gyp:base_test1',
        #'<(DEPTH)/dpe_service/dpe_service.gyp:publisher',
        #'<(DEPTH)/dpe_service/dpe_service.gyp:subscriber',
        '<(DEPTH)/dpe/dpe.gyp:dpe',
        '<(DEPTH)/dpe/dpe.gyp:main',
        #'<(DEPTH)/dpe/dpe.gyp:pm',
        #'<(DEPTH)/remote_shell/rs.gyp:rs',
        #'<(DEPTH)/third_party/zeromq_4.2.1/builds/msvc/vs2015/libzmq/zmq.gyp:zmq',
        #'<(DEPTH)/third_party/protobuf-3.3.1/gyp/protobuf.gyp:protobuf',
        ],
    }
    ]
}