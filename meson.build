project('retro-demoeffects', 'cpp',
  default_options: ['warning_level=1', 'buildtype=debug'])

sdl2 = dependency('sdl2')

link_args = []
if host_machine.system() == 'darwin'
  link_args += ['-Wl,-undefined,dynamic_lookup']
endif

demos = [
  'blobs',
  'bump',
  'conio',
  'distort',
  'distortlogo',
  'dotball',
  'dotflag',
  'dotmorph',
  'dottorus',
  'dottorus2',
  'dottunnel',
  'dottunnel2',
  'environcube',
  'fade',
  'fire',
  'firecube',
  'firelogo',
  'flatcube',
  'flatshadedcube',
  'font',
  'glenzcube',
  'glenzshadedcube',
  'gouraudcube',
  'hiddenlinecube',
  'julia',
  'lens',
  'linedance',
  'linedance2',
  'linedance3',
  'maskdemo',
  'mandelbrot',
  'metaballs',
  'metaballs2',
  'paint',
  'particles',
  'phongcube',
  'plane',
  'plasma',
  'plasma2',
  'plasmacube',
  'ripples',
  'rototunnel',
  'rotozoom',
  'scroller',
  'scrollercube',
  'shadebobs',
  'stars',
  'stars2',
  'stars3',
  'template_main',
  'template_retromain',
  'texturecube',
  'twister',
  'twister2',
  'voxel',
  'water',
  'whitenoise',
  'wirecube',
  'wormhole',
  'xorcircles',
]

foreach demo : demos
  executable(demo, 'src' / demo + '.cpp',
    dependencies: sdl2,
    link_args: link_args)
endforeach
