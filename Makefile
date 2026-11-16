CC ?= cc
EMCC ?= emcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror
CORE = fish.c school.c particle.c spatial_grid.c vector3d.c
HEADERS = fish.h school.h particle.h spatial_grid.h vector3d.h
EXPORTS = '["_main","_resize_canvas","_set_count","_set_speed","_set_scale","_set_debug","_set_depth","_set_palette","_set_paused","_set_follow","_set_pointer","_reset_school","_set_obstacle","_clear_obstacles","_get_fish_count","_get_simulation_time","_get_frame_ms","_get_dropped_triangles"]'

.PHONY: all wasm spaces test sanitize serve clean
all: wasm
wasm: web/fish.js
web/fish.js: main_wasm.c $(CORE) $(HEADERS) Makefile
	$(EMCC) $(CFLAGS) -O3 main_wasm.c $(CORE) -lm \
		-s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 \
		-s MODULARIZE=1 -s EXPORT_NAME=createFishSchool -s ENVIRONMENT=web \
		-s INCOMING_MODULE_JS_API='["canvas","print","printErr","wasmBinary","locateFile","onAbort","onRuntimeInitialized","instantiateWasm","noInitialRun"]' \
		-s FILESYSTEM=0 -s NO_EXIT_RUNTIME=1 -s INITIAL_MEMORY=16777216 \
		-s STACK_SIZE=262144 -s EXPORTED_FUNCTIONS=$(EXPORTS) -o $@
build/test-school: tests/test_school.c $(CORE) $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) -I. tests/test_school.c $(CORE) -lm -o $@
test: build/test-school
	./build/test-school
spaces: wasm
	node scripts/package-spaces.mjs
sanitize:
	mkdir -p build
	$(CC) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I. tests/test_school.c $(CORE) -lm -o build/test-school-sanitize
	./build/test-school-sanitize
serve:
	python3 -m http.server 8090 --bind 127.0.0.1 --directory web
clean:
	rm -f web/fish.js web/fish.wasm build/test-school build/test-school-sanitize
