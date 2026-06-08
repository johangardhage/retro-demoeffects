# retro-demoeffects

Classic demo effects using software rendering

![Screenshot](/screenshots/rototunnel.png "rototunnel")

## Prerequisites

To build the demo programs, you must first install the following tools:

- [Meson](https://mesonbuild.com/)
- [Ninja](https://ninja-build.org/)
- [GCC](https://gcc.gnu.org/) or [Clang](https://clang.llvm.org/)
- [SDL3](https://www.libsdl.org/)

### Install dependencies

#### openSUSE

`$ sudo zypper install meson ninja gcc-c++ SDL3-devel`

#### Ubuntu

`$ sudo apt install meson ninja-build g++ libsdl3-dev`

#### macOS

`$ brew install meson ninja pkg-config sdl3`

## Build instructions

To build the demo programs, run:

```
$ meson setup build
$ ninja -C build
```

The `build` directory will contain the demo programs.

## Usage

```
Usage: demo [OPTION]...

Options:
 -h, --help           Display this text and exit
 -w, --window         Render in a window
     --fullwindow     Render in a fullscreen window
 -f, --fullscreen     Render in fullscreen
 -v, --vsync          Enable sync to vertical refresh
     --novsync        Disable sync to vertical refresh
 -l, --linear         Render using linear filtering
     --nolinear       Render using nearest pixel sampling
 -c, --showcursor     Show mouse cursor
     --nocursor       Hide mouse cursor
     --showfps        Show frame rate in window title
     --nofps          Hide frame rate
     --capfps=VALUE   Limit frame rate to the specified VALUE
```

## License

Licensed under MIT license. See [LICENSE](LICENSE) for more information.

## Authors

* Johan Gardhage

## Screenshots

![Screenshot](/screenshots/texmapflatshademask.png "texmapflatshademask")
![Screenshot](/screenshots/texmapgouraudshademask.png "texmapgouraudshademask")
![Screenshot](/screenshots/texmapenvmapbumpmask.png "texmapenvmapbumpmask")
![Screenshot](/screenshots/envmapmask2.png "envmapmask2")
![Screenshot](/screenshots/envmapmask.png "envmapmask")
![Screenshot](/screenshots/envmapbumpmask.png "envmapbumpmask")
![Screenshot](/screenshots/lens.png "lens")
![Screenshot](/screenshots/water.png "water")
![Screenshot](/screenshots/voxel.png "voxel")
![Screenshot](/screenshots/mandelbrot.png "mandelbrot")
![Screenshot](/screenshots/plasma.png "plasma")
![Screenshot](/screenshots/xorcircles.png "xorcircles")
![Screenshot](/screenshots/fire.png "fire")
![Screenshot](/screenshots/dotflag.png "dotflag")
![Screenshot](/screenshots/dotball.png "dotball")
![Screenshot](/screenshots/dottunnel.png "dottunnel")
![Screenshot](/screenshots/firecube.png "firecube")
![Screenshot](/screenshots/glenzshadedcube.png "glenzshadedcube")
![Screenshot](/screenshots/plasmacube.png "plasmacube")
![Screenshot](/screenshots/gouraudcube.png "gouraudcube")
![Screenshot](/screenshots/phongcube.png "phongcube")
![Screenshot](/screenshots/environcube.png "environcube")
![Screenshot](/screenshots/metaballs.png "metaballs")
![Screenshot](/screenshots/blobs.png "blobs")
![Screenshot](/screenshots/shadebobs.png "shadebobs")
![Screenshot](/screenshots/bump.png "bump")
![Screenshot](/screenshots/twister.png "twister")
![Screenshot](/screenshots/scroller.png "scroller")
