{
  'target_defaults': {
    'variables':{
      'use_zmq' : 0,
      'zmq_debug_lib%':'libzmq-v120-mt-gd-4_0_4.lib',
      'zmq_release_lib%':'libzmq-v120-mt-4_0_4.lib',
    },
    'target_conditions': [
      ['use_zmq==1', {
          'include_dirs': [
            '<(DEPTH)\\third_party\\zmq\\include',
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
      ] # 'use_zmq==1'
    ]
  }
}