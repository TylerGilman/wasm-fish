# Source credits

The flocking core comes from [TylerGilman/opengl_flock](https://github.com/TylerGilman/opengl_flock), commit `beede0a53768660bdebf5b4a7b541fce54c36004`.
This repo retains that project history.
`particle.c`, `vector3d.c`, and `spatial_grid.c` keep the separation, alignment, cohesion, and spatial grid code.

[TylerGilman/js-animals](https://github.com/TylerGilman/js-animals) is the visual and interaction reference for flat fish built from constrained points.

`fish.c` adapts the angle-constrained chain method and body-width profile from [argonautcode/animal-proc-anim](https://github.com/argonautcode/animal-proc-anim), commit `cb171e8c386494a9d92dd47ea3422f84f7fbaa8d`.
The relevant source files are `Chain.pde`, `Util.pde`, and `Fish.pde`.
The C implementation adds a fixed simulation step, a small active swimming wave, and a WebGL triangle renderer.
The upstream MIT license follows.

```text
MIT License

Copyright (c) 2024 argonaut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
