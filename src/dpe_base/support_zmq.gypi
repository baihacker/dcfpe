{
  'target_defaults': {
    'variables':{
      'use_zmq' : 0,
    },
    'target_conditions': [
      ['use_zmq==1', {
          'include_dirs': [
            '<(DEPTH)/third_party/zeromq_4.2.1/include',
          ],
          'dependencies':[
            ],
          'defines': [
            'CRT_SECURE_NO_WARNINGS',
            '_WINSOCK_DEPRECATED_NO_WARNINGS',
            'FD_SETSIZE=16384',
            'WIN32_LEAN_AND_MEAN',
            'ZMQ_USE_SELECT',
            'DLL_EXPORT',
            ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalDependencies': [
                  ],
                },
              },
            },
            'Release_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalDependencies': [
                  ],
                },
              },
            },
            'x86_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                'AdditionalLibraryDirectories': [
                  ],
                },
              },
            },
            'x64_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                'AdditionalLibraryDirectories': [
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