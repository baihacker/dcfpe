{
  'includes':[
    '..\\build\\win_precompile.gypi'
  ],
  'variables':{
    'zmq_debug_lib%':'libzmq-v100-mt-gd-4_0_4.lib',
    'zmq_release_lib%':'libzmq-v100-mt-4_0_4.lib',
  },
  'target_defaults': {
    'include_dirs': [
      '<(DEPTH)\\thrid_party\\zmq\\include',
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'AdditionalLibraryDirectories': [
          '<(DEPTH)\\third_party\\zmq\\lib'
          ],
      },
    },
    'configurations': {
      
      'Release_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              '<(zmq_release_lib)',
            ],
          },
        },
      },
      
      'Debug_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              '<(zmq_debug_lib)',
            ],
          },
        },
      },
    }
  },
  'targets':[
    {
      'target_name': 'publisher',
      'type': 'executable',
      'sources':['publisher.cc'],
    },
    {
      'target_name': 'subscriber',
      'type': 'executable',
      'sources':['subscriber.cc'],
    },
    ]
}