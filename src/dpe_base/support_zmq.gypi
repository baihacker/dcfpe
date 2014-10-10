{
  'target_defaults': {
    'variables':{
      'zmq_debug_lib%':'libzmq-v120-mt-gd-4_0_4.lib',
      'zmq_release_lib%':'libzmq-v120-mt-4_0_4.lib',
    },
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
  }
}