project('retro-demoeffects', 'cpp',
  default_options: ['warning_level=1', 'buildtype=debugoptimized', 'cpp_std=c++17'])

sdl3 = dependency('sdl3')
fs = import('fs')

deps = [sdl3]
link_args = []
if host_machine.system() == 'darwin'
  # The key warmup in retro.h builds a key event with CoreGraphics and hands
  # it to the menu through the Objective-C runtime
  deps += dependency('appleframeworks', modules: ['ApplicationServices'])
  # The DEMO_ callbacks a demo leaves out are weak and stay undefined
  link_args += ['-lobjc', '-Wl,-undefined,dynamic_lookup']
endif

demos = run_command(
  'find', meson.current_source_dir() / 'src',
  '-type', 'f', '-name', '*.cpp',
  check: true,
).stdout().strip().split('\n')

foreach demo : demos
  executable(fs.stem(demo), demo,
    dependencies: deps,
    link_args: link_args)
endforeach
