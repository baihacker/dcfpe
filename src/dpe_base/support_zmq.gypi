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
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalDependencies': [
                    '<(zmq_debug_lib)',
                  ],
                },
              },
            },
            'Release_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalDependencies': [
                    '<(zmq_release_lib)',
                  ],
                },
              },
            },
            'x86_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                'AdditionalLibraryDirectories': [
                  '<(DEPTH)\\third_party\\zmq\\lib'
                  ],
                },
              },
            },
            'x64_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                'AdditionalLibraryDirectories': [
                  '<(DEPTH)\\third_party\\zmq\\lib_x64'
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