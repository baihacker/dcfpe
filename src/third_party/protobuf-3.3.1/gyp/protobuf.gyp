{
  'includes':[
    '../../../build/common.gypi',
    '../../../build/win_precompile.gypi',
  ],
  'variables':{
  },
  'target_defaults': {
    'include_dirs': [
      '../',
      '../src',
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'AdditionalLibraryDirectories': [
          ],
      },
    },
    'msvs_disabled_warnings': [
      4244, 4267, 4018, 4355, 4800, 4251, 4996, 4146, 4305, 4309, 4506, 4065, 4005
    ],
    'configurations': {
      'Release_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
            ],
          },
        },
      },
      
      'Debug_Base': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
            ],
          },
        },
      },
    }
  },
  'targets':[
    {
      'target_name': 'protobuf',
      'type': 'static_library',
      'variables': {
        'use_zmq': 0,
      },
      'defines': [
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
        },
      },
      'sources':[
        '../src/google/protobuf/any.cc',
        '../src/google/protobuf/any.pb.cc',
        '../src/google/protobuf/api.pb.cc',
        '../src/google/protobuf/compiler/importer.cc',
        '../src/google/protobuf/compiler/parser.cc',
        '../src/google/protobuf/descriptor.cc',
        '../src/google/protobuf/descriptor.pb.cc',
        '../src/google/protobuf/descriptor_database.cc',
        '../src/google/protobuf/duration.pb.cc',
        '../src/google/protobuf/dynamic_message.cc',
        '../src/google/protobuf/empty.pb.cc',
        '../src/google/protobuf/extension_set_heavy.cc',
        '../src/google/protobuf/field_mask.pb.cc',
        '../src/google/protobuf/generated_message_reflection.cc',
        '../src/google/protobuf/generated_message_table_driven.cc',
        '../src/google/protobuf/io/gzip_stream.cc',
        '../src/google/protobuf/io/printer.cc',
        '../src/google/protobuf/io/strtod.cc',
        '../src/google/protobuf/io/tokenizer.cc',
        '../src/google/protobuf/io/zero_copy_stream_impl.cc',
        '../src/google/protobuf/map_field.cc',
        '../src/google/protobuf/message.cc',
        '../src/google/protobuf/reflection_ops.cc',
        '../src/google/protobuf/service.cc',
        '../src/google/protobuf/source_context.pb.cc',
        '../src/google/protobuf/struct.pb.cc',
        '../src/google/protobuf/stubs/mathlimits.cc',
        '../src/google/protobuf/stubs/substitute.cc',
        '../src/google/protobuf/text_format.cc',
        '../src/google/protobuf/timestamp.pb.cc',
        '../src/google/protobuf/type.pb.cc',
        '../src/google/protobuf/unknown_field_set.cc',
        '../src/google/protobuf/util/delimited_message_util.cc',
        '../src/google/protobuf/util/field_comparator.cc',
        '../src/google/protobuf/util/field_mask_util.cc',
        '../src/google/protobuf/util/internal/datapiece.cc',
        '../src/google/protobuf/util/internal/default_value_objectwriter.cc',
        '../src/google/protobuf/util/internal/error_listener.cc',
        '../src/google/protobuf/util/internal/field_mask_utility.cc',
        '../src/google/protobuf/util/internal/json_escaping.cc',
        '../src/google/protobuf/util/internal/json_objectwriter.cc',
        '../src/google/protobuf/util/internal/json_stream_parser.cc',
        '../src/google/protobuf/util/internal/object_writer.cc',
        '../src/google/protobuf/util/internal/proto_writer.cc',
        '../src/google/protobuf/util/internal/protostream_objectsource.cc',
        '../src/google/protobuf/util/internal/protostream_objectwriter.cc',
        '../src/google/protobuf/util/internal/type_info.cc',
        '../src/google/protobuf/util/internal/type_info_test_helper.cc',
        '../src/google/protobuf/util/internal/utility.cc',
        '../src/google/protobuf/util/json_util.cc',
        '../src/google/protobuf/util/message_differencer.cc',
        '../src/google/protobuf/util/time_util.cc',
        '../src/google/protobuf/util/type_resolver_util.cc',
        '../src/google/protobuf/wire_format.cc',
        '../src/google/protobuf/wrappers.pb.cc',
        '../src/google/protobuf/arena.cc',
        '../src/google/protobuf/arenastring.cc',
        '../src/google/protobuf/extension_set.cc',
        '../src/google/protobuf/generated_message_util.cc',
        '../src/google/protobuf/io/coded_stream.cc',
        '../src/google/protobuf/io/zero_copy_stream.cc',
        '../src/google/protobuf/io/zero_copy_stream_impl_lite.cc',
        '../src/google/protobuf/message_lite.cc',
        '../src/google/protobuf/repeated_field.cc',
        '../src/google/protobuf/stubs/atomicops_internals_x86_gcc.cc',
        '../src/google/protobuf/stubs/atomicops_internals_x86_msvc.cc',
        '../src/google/protobuf/stubs/bytestream.cc',
        '../src/google/protobuf/stubs/common.cc',
        '../src/google/protobuf/stubs/int128.cc',
        '../src/google/protobuf/stubs/once.cc',
        '../src/google/protobuf/stubs/status.cc',
        '../src/google/protobuf/stubs/statusor.cc',
        '../src/google/protobuf/stubs/stringpiece.cc',
        '../src/google/protobuf/stubs/stringprintf.cc',
        '../src/google/protobuf/stubs/structurally_valid.cc',
        '../src/google/protobuf/stubs/strutil.cc',
        '../src/google/protobuf/stubs/time.cc',
        '../src/google/protobuf/wire_format_lite.cc',
        ],
      'dependencies':[
        ],
    },
  ]
}
