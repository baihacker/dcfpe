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
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:base_test',
        '<(DEPTH)\\process\\process.gyp:process',
        '<(DEPTH)\\process\\process.gyp:process_proxy',
        '<(DEPTH)\\zmq_demo\\zmq_demo.gyp:publisher',
        '<(DEPTH)\\zmq_demo\\zmq_demo.gyp:subscriber',
        ],
    }
    ]
}