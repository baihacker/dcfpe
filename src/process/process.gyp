{
  'includes':[
    '..\\build\\common.gypi',
    '..\\build\\win_precompile.gypi',
    '..\\dpe_base\\support_zmq.gypi',
  ],
  'targets':[
    {
      'target_name': 'process',
      'type': '<(component)',
      'sources':['process_impl\\process_impl.cc'],
      'dependencies':[
        '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
        ],
    },
    {
      'target_name': 'process_proxy',
      'type': 'executable',
      'sources':['process_proxy\\process_proxy.cc'],
      'dependencies':[
        '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
    },
    ]
}