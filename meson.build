project('retro-demoeffects', 'cpp',
  default_options: ['warning_level=1', 'buildtype=debugoptimized'])

sdl3 = dependency('sdl3')
fs = import('fs')

link_args = []
if host_machine.system() == 'darwin'
  link_args += ['-Wl,-undefined,dynamic_lookup']
endif

demos = run_command(
  'find', meson.current_source_dir() / 'src',
  '-type', 'f', '-name', '*.cpp',
  check: true,
).stdout().strip().split('\n')

foreach demo : demos
  executable(fs.stem(demo), demo,
    dependencies: sdl3,
    link_args: link_args)
endforeach
