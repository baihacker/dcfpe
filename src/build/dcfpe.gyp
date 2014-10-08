{
  'includes':[
    'common.gypi',
    'win_precompile.gypi'
  ],
  'targets':[
    {
      'target_name': 'all',
      'type': 'none',
      'sources':['123.cc'],
      'dependencies':[
        '<(DEPTH)\\dpe_base\\dpe_base.gyp:dpe_base',
        ],
    }
    ]
}