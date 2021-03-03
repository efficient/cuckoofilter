{
  'targets': [
    {
      'target_name': 'cuckoosync',
      'sources': [ 'src/bitsutil.h, cuckoofilter.h, debug.h, hashutil.cc, hashutil.h, packedtable.h, permencoding.h, printutil.cc, printutil.h, simd-block.h, singletable.h' ],
      'include_dirs': ["<!@(node -p \"require('node-addon-api').include\")"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'CLANG_CXX_LIBRARY': 'libc++',
        'MACOSX_DEPLOYMENT_TARGET': '10.7'
      },
      'msvs_settings': {
        'VCCLCompilerTool': { 'ExceptionHandling': 1 },
      }
    }
  ]
}