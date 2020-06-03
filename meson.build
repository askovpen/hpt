project('hpt','c', version:'1.9.0')

cc = meson.get_compiler('c')

link_args = []
compile_args = []
libs=[]

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

perl_src=[]
  perl=find_program('perl', required: false)
  if perl.found() and get_option('perl-static')==false
    cflags=[]
    ldflags=[]
    if cc.get_id() == 'msvc'
      perl_def=run_command(perl,'-MExtUtils::Embed','-e','ccopts')
      perldir=run_command(perl,'-e','print join(";",@INC)')
      perlver=run_command(perl,'-e','my @v=split(/\./,$^V);print substr($v[0],1).$v[1]').stdout().strip()
      fs=import('fs')
      foreach d: perldir.stdout().strip().split(';')
        if fs.exists(d+'/CORE/EXTERN.h')
          cflags+='-DPERL_IMPLICIT_CONTEXT'
          cflags+='-DPERL_IMPLICIT_SYS'
          cflags+='-DWIN64'
          cflags+='-DDELAYIMP_INSECURE_WRITABLE_HOOKS'
          cflags+='-I'+d+'/CORE'
          ldflags+=d+'/CORE/perl'+perlver+'.lib'
        endif
      endforeach
    else
      if cc.get_id() == 'gcc'
          cflags+='-fPIC'
      endif
      perl_def=run_command(perl,'-MExtUtils::Embed','-e','ccopts')
      foreach d: perl_def.stdout().strip().split()
        if d.startswith('-I') or d.startswith('-D')
          cflags+=d
       endif
      endforeach
      perl_ldef=run_command(perl,'-MExtUtils::Embed','-e','ldopts')
      foreach ld: perl_ldef.stdout().strip().split()
        if ld!=''
          ldflags+=ld
        endif
      endforeach
    endif
    res=cc.run('''
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
int main(int argc, char** argv, char** env) {
 return perl_alloc() ? 0 : 1;
}
 ''',name: 'try compile with perl', args: cflags+ldflags)
    if res.returncode()==0
      perl_src += files(['src/perl.c'])
      add_project_arguments(['-DDO_PERL'], language: 'c')
        add_project_arguments(cflags, language: 'c')
        add_project_link_arguments(ldflags, language: 'c')
    endif
  endif
if get_option('perl-static')==true and host_machine.system()=='linux'
  message('test')
  perl_proj=subproject('perlstatic')
  libs+=perl_proj.get_variable('perl_dep')
  add_project_arguments(['-DDO_PERL'], language: 'c')
  link_args+='-static'
  perl_src += files(['src/perl.c'])
endif

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
])+perl_src,
  include_directories : incdir,
  dependencies:[areafix_dep, hptzip_dep, libs],
  link_args: link_args,
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
  link_args: link_args,
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
  link_args: link_args,
  install: true
)

