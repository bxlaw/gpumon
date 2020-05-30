# gpumon
ncurses based gpu monitor for amdgpu/linux

![screenshot](https://github.com/bxlaw/gpumon/blob/master/Screenshot%20from%202020-05-30%2013-35-36.png)

## dependencies
- CMake + C++17 compiler
- ncurses

## build instructions
gpumon is built in the usual CMake way:
```bash
mkdir build && cd build && cmake .. && make
```

Run `./gpumon` to start. Quit by pressing the `q` key, the `Esc` key, `Ctrl-C` or `Ctrl-D`.

Passing the argument `-u n` sets the update interval to `n` seconds. Negative `n` will only update on key presses.
