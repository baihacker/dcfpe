# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
    '../build/win_precompile.gypi',
    'base.gypi',
  ],
  'targets': [
    {
      'target_name': 'base',
      'type': '<(component)',
      'toolsets': ['host', 'target'],
      'variables': {
        'base_target': 1,
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'dependencies': [
        'base_static',
        #'allocator/allocator.gyp:allocator_extension_thunks',
        #'../testing/gtest.gyp:gtest_prod',
        #'../third_party/modp_b64/modp_b64.gyp:modp_b64',
        #'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      # TODO(gregoryd): direct_dependent_settings should be shared with the
      #  64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['desktop_linux == 1 or chromeos == 1', {
          'conditions': [
            ['chromeos==1', {
              'sources/': [ ['include', '_chromeos\\.cc$'] ]
            }],
          ],
          'dependencies': [
            'symbolize',
            'xdg_mime',
          ],
          'defines': [
            'USE_SYMBOLIZE',
          ],
        }, {  # desktop_linux == 0 and chromeos == 0
            'sources/': [
              ['exclude', '/xdg_user_dirs/'],
              ['exclude', '_nss\\.cc$'],
            ],
        }],
        ['use_glib==1', {
          'dependencies': [
            '../build/linux/system.gyp:glib',
          ],
          'export_dependent_settings': [
            '../build/linux/system.gyp:glib',
          ],
        }],
        ['OS == "win"', {
          # Specify delayload for base.dll.
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'powrprof.dll',
              ],
              'AdditionalDependencies': [
                'powrprof.lib',
              ],
            },
          },
          # Specify delayload for components that link with base.lib.
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  'powrprof.dll',
                ],
                'AdditionalDependencies': [
                  'powrprof.lib',
                ],
              },
            },
          },
        }],
        
        ['OS != "win" and OS != "ios"', {
            'dependencies': ['../third_party/libevent/libevent.gyp:libevent'],
        },],
        ['component=="shared_library"', {
          'conditions': [
            ['OS=="win"', {
              'sources!': [
                'debug/debug_on_start_win.cc',
              ],
            }],
          ],
        }],
      ],
      'sources': [
        'third_party/xdg_user_dirs/xdg_user_dir_lookup.cc',
        'third_party/xdg_user_dirs/xdg_user_dir_lookup.h',
        'async_socket_io_handler.h',
        'async_socket_io_handler_posix.cc',
        'async_socket_io_handler_win.cc',
        'auto_reset.h',
        'event_recorder.h',
        'event_recorder_stubs.cc',
        'event_recorder_win.cc',
        'linux_util.cc',
        'linux_util.h',
        'message_loop/message_pump_android.cc',
        'message_loop/message_pump_android.h',
        'message_loop/message_pump_glib.cc',
        'message_loop/message_pump_glib.h',
        'message_loop/message_pump_io_ios.cc',
        'message_loop/message_pump_io_ios.h',
        'message_loop/message_pump_observer.h',
        'message_loop/message_pump_libevent.cc',
        'message_loop/message_pump_libevent.h',
        'message_loop/message_pump_mac.h',
        'message_loop/message_pump_mac.mm',
        'metrics/field_trial.cc',
        'metrics/field_trial.h',
        'posix/file_descriptor_shuffle.cc',
        'posix/file_descriptor_shuffle.h',
        'sync_socket.h',
        'sync_socket_win.cc',
        'sync_socket_posix.cc',
      ],
    },
    
    {
      'target_name': 'base_prefs',
      'type': '<(component)',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'dependencies': [
        'base',
      ],
      'export_dependent_settings': [
        'base',
      ],
      'defines': [
        'BASE_PREFS_IMPLEMENTATION',
      ],
      'sources': [
        'prefs/base_prefs_export.h',
        'prefs/default_pref_store.cc',
        'prefs/default_pref_store.h',
        'prefs/json_pref_store.cc',
        'prefs/json_pref_store.h',
        'prefs/overlay_user_pref_store.cc',
        'prefs/overlay_user_pref_store.h',
        'prefs/persistent_pref_store.h',
        'prefs/pref_change_registrar.cc',
        'prefs/pref_change_registrar.h',
        'prefs/pref_filter.h',
        'prefs/pref_member.cc',
        'prefs/pref_member.h',
        'prefs/pref_notifier.h',
        'prefs/pref_notifier_impl.cc',
        'prefs/pref_notifier_impl.h',
        'prefs/pref_observer.h',
        'prefs/pref_registry.cc',
        'prefs/pref_registry.h',
        'prefs/pref_registry_simple.cc',
        'prefs/pref_registry_simple.h',
        'prefs/pref_service.cc',
        'prefs/pref_service.h',
        'prefs/pref_service_factory.cc',
        'prefs/pref_service_factory.h',
        'prefs/pref_store.cc',
        'prefs/pref_store.h',
        'prefs/pref_value_map.cc',
        'prefs/pref_value_map.h',
        'prefs/pref_value_store.cc',
        'prefs/pref_value_store.h',
        'prefs/scoped_user_pref_update.cc',
        'prefs/scoped_user_pref_update.h',
        'prefs/value_map_pref_store.cc',
        'prefs/value_map_pref_store.h',
        'prefs/writeable_pref_store.h',
      ],
    },
    {
      # This is the subset of files from base that should not be used with a
      # dynamic library. Note that this library cannot depend on base because
      # base depends on base_static.
      'target_name': 'base_static',
      'type': 'static_library',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'toolsets': ['host', 'target'],
      'sources': [
        'base_switches.cc',
        'base_switches.h',
        'win/pe_image.cc',
        'win/pe_image.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ],
}
