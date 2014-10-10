{
  'includes':[
    '..\\build\\common.gypi',
    '..\\build\\win_precompile.gypi',
    '..\\dpe_base\\support_zmq.gypi',
  ],
  'targets':[
    {
      'target_name': 'publisher',
      'type': 'executable',
      'sources':['publisher.cc'],
      'dependencies':[
        '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
    },
    {
      'target_name': 'subscriber',
      'type': 'executable',
      'sources':['subscriber.cc'],
    },
  ]
}