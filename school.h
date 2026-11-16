#ifndef WASM_FISH_SCHOOL_H
#define WASM_FISH_SCHOOL_H
#include "fish.h"
#include "particle.h"
#include "spatial_grid.h"
#include <stdint.h>

#define SCHOOL_MAX_FISH 128
#define SCHOOL_MAX_OBSTACLES 30
#define SCHOOL_DEPTH 480.0f
#define SCHOOL_STEP (1.0f / 60.0f)

typedef struct {
  float x, y, width, height, padding;
  int active;
} SchoolObstacle;
typedef struct {
  Particle particles[SCHOOL_MAX_FISH];
  FishBody bodies[SCHOOL_MAX_FISH];
  SchoolObstacle obstacles[SCHOOL_MAX_OBSTACLES];
  SpatialGrid *grid;
  float width, height, radius, time, accumulator, speed, scale;
  float pointer_x, pointer_y;
  int count, paused, follow_pointer;
  uint32_t seed, random_state;
} School;

School *school_create(float width, float height, int count, uint32_t seed);
void school_destroy(School *school);
void school_reset(School *school, uint32_t seed);
void school_resize(School *school, float width, float height);
void school_set_count(School *school, int count);
void school_set_obstacle(School *school, int index, float x, float y,
                         float width, float height, float padding);
void school_clear_obstacles(School *school);
void school_advance(School *school, float frame_seconds);
/* Draw indices leave the simulation and each fish's spine untouched. */
void school_draw_order(const School *school, int *indices);
#endif
