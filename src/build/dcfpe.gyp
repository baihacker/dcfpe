{
  'includes':[
    'common.gypi',
    'win_precompile.gypi'
  ],
  'targets':[
    {
      'target_name': 'test',
      'type': 'executable',
      'sources':['123.cc'],
      'dependencies':[
        '<(DEPTH)\\third_party\\chromium\\base\\base.gyp:base',
        ],
    }]
}