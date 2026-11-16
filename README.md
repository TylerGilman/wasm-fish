# wasm-fish

Procedural 2D fish with 3D schooling, written in C and compiled to WebAssembly.

The body starts from the constrained-point idea in `js-animals`.
The chain solver and body-width profile use Argonaut's cleaner method.
The movement core comes from `opengl_flock`.
See [source credits and licenses](THIRD_PARTY_NOTICES.md).

![Desktop demo](docs/preview-desktop.webp)

## Run the demo

Install [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) and activate its shell environment.
The build is tested with Emscripten 7.0.0.

```sh
make wasm
make serve
```

Open http://127.0.0.1:8090/.
You can also pass the compiler path with `make wasm EMCC=/path/to/emcc`.
The generated `web/fish.js` and `web/fish.wasm` files are ignored by git.
GitHub Actions also builds these files.
Download its `wasm-fish-web` artifact to try the compiled demo without a compiler.
Unzip it and serve its `web` directory with a local HTTP server.

## Controls

- Fish count: 1 to 128. The defaults are 40 on desktop and 16 on mobile.
- Swim speed: changes simulation time, including body motion.
- Fish size: changes the rendered size and obstacle clearance.
- Depth fade: changes size and opacity with the fish's Z position.
- Show spines: shows the 12 constrained points and links.
- Follow pointer: guides the school toward the pointer.
- Pause and New school: pause movement or generate another seeded school.

Mobile controls start closed to leave more space for the fish.
Reduced-motion mode starts paused.
The demo pauses when the page is hidden.

## How it works

Each fish has one 3D boid and one 2D body.
The boid moves the head through X, Y, and Z.
The body follows the head in the drawing plane.
Adjacent spine points stay a fixed distance apart and bend by at most 22.5 degrees.
The first ten points define the body width.
The last two guide the tail.

The renderer builds a smooth outline, fins, eyes, and a forked tail from those points.
It draws the shapes in one batched WebGL 2 triangle call.
The canvas background is transparent.
Depth changes size, opacity, and draw order, as it does in the original flock.
This is not a perspective camera or a solid 3D fish mesh.

The simulation runs at a fixed 60 steps per second.
Each step reads a shared snapshot of the school.
Depth sorting changes draw indices rather than moving fish between slots.
This keeps every fish attached to its own body history.
The seed controls the initial positions, body lengths, widths, and swimming phases.

## Checks

```sh
make test
make sanitize
# Start make serve in another terminal before browser checks.
node scripts/check-demo.mjs
node scripts/check-demo.mjs http://127.0.0.1:8090/ 390 844
node scripts/check-demo.mjs http://127.0.0.1:8090/ 390 844 reduce
```

Native checks exercise joint distances, bend limits, frame-rate independence,
pause, depth order, count limits, resize, reset, and obstacle steering.
Browser checks need Node.js 22+ and Google Chrome.
They exercise the compiled WASM through the demo controls.
Captures are written to `build/`.
The CPU time label measures the C simulation and draw submission.
It does not measure the complete GPU frame time.

## Integration boundary

### Spaces package

[Open the Spaces demo](https://chatgpt.com/space/page_9bf34313dac88191a1a73e5891059dca).

`make spaces` writes `build/wasm-fish-spaces.html`.
This file includes the compiled WASM bytes and needs no external asset requests.
It fits the Page embed limit of 256 KiB.
The C/WASM engine is the same engine used in the standalone demo.
The Page version also saves control choices in widget state.

### Standalone module

The original flock repo and the Nereus site are separate from this project.
This demo does not deploy or change either one.

The module factory is `createFishSchool({canvas})`.
Use a canvas with `id="canvas"` and a defined CSS size.
The module exposes `_resize_canvas`, `_set_count`, `_set_speed`, `_set_scale`,
`_set_debug`, `_set_depth`, `_set_paused`, `_set_follow`, `_set_pointer`,
`_reset_school`, `_set_obstacle`, and `_clear_obstacles`.
Coordinates use CSS pixels.
The device pixel ratio is capped at 2 for the drawing buffer.

Obstacle rectangles use the original flock integration pattern.
They add soft steering with clearance for the fish body.
They are not a hard collision solver.

## Source layout

- `fish.c`: body chain and width profile.
- `school.c`: fixed-step school state, boundaries, and rectangle avoidance.
- `particle.c`, `vector3d.c`, `spatial_grid.c`: flock core.
- `main_wasm.c`: WebGL renderer and exported browser controls.
- `web/`: plain HTML, CSS, and JavaScript demo.
- `tests/`: native behavior checks.
- `scripts/`: browser behavior checks.
