project('hpt','c', version:'1.9.0')

cc = meson.get_compiler('c')

link_args = []
compile_args = []

add_project_arguments(
  '-DUSE_HPTZIP',
language : 'c')

if cc.get_id() == 'msvc'
  add_project_arguments(
    '-D_CRT_SECURE_NO_DEPRECATE',
    '-D_CRT_NONSTDC_NO_DEPRECATE', 
    '-DWIN32',
    '-D_WINDOWS',
    '-D_CONSOLE',
    language : 'c')
  if get_option('default_library') != 'static'
    add_project_arguments('-D_DLL', '-D_MAKE_DLL', language : 'c')
  endif

else
  add_project_arguments(
    '-DUNIX',
  language : 'c')
endif

incdir = include_directories(['h'])


areafix_proj = subproject('areafix')
areafix_dep=areafix_proj.get_variable('areafix_dep')

hptzip_proj = subproject('hptzip')
hptzip_dep=hptzip_proj.get_variable('hptzip_dep')

executable('hpt',
  files([
    'src/hpt.c',
    'src/dupe.c', 
    'src/pktread.c', 
    'src/fcommon.c', 
    'src/global.c', 
    'src/pktwrite.c', 
    'src/toss.c', 
    'src/scan.c', 
    'src/scanarea.c', 
    'src/post.c', 
    'src/hptafix.c', 
    'src/seenby.c', 
    'src/link.c', 
    'src/carbon.c'
]),
  include_directories : incdir,
  dependencies:[areafix_dep, hptzip_dep],
  install: true
)

executable('pktinfo',
  files([
    'src/pktinfo.c',
    'src/dupe.c',
    'src/pktread.c', 
    'src/fcommon.c',
    'src/global.c'
  ]),
  include_directories : incdir,
  dependencies:[areafix_dep],
  install: true
)
executable('tpkt',
  files([
    'src/tpkt.c',
    'src/dupe.c',
    'src/pktread.c',
    'src/fcommon.c', 
    'src/global.c',
    'src/pktwrite.c'
  ]),
  include_directories : incdir,
  dependencies:[areafix_dep],
  install: true
)

