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
      'variables': {
        'use_zmq': 1,
      },
      'sources':['publisher.cc'],
      'dependencies':[
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
        ],
    },
    {
      'target_name': 'subscriber',
      'type': 'executable',
      'variables': {
        'use_zmq': 1,
      },
      'sources':['subscriber.cc'],
      'dependencies':[
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
        ],
    },
  ]
}