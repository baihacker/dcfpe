# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../common.gypi',
  ],
  'variables': {
    'conditions': [
      ['sysroot!=""', {
        'pkg-config': '<(chroot_cmd) ./pkg-config-wrapper "<(sysroot)" "<(target_arch)" "<(system_libdir)"',
      }, {
        'pkg-config': 'pkg-config',
      }],
    ],

    'linux_link_libgps%': 0,
    'linux_link_libpci%': 0,
    'linux_link_libspeechd%': 0,
    'linux_link_libbrlapi%': 0,
  },
  'targets': [
    {
      'target_name': 'glib',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'variables': {
        'glib_packages': 'glib-2.0 gmodule-2.0 gobject-2.0 gthread-2.0',
      },
      'conditions': [
        ['_toolset=="target"', {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags <(glib_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other <(glib_packages))',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l <(glib_packages))',
            ],
          },
        }, {
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags <(glib_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other <(glib_packages))',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l <(glib_packages))',
            ],
          },
        }],
      ],
    },
  ],
}
