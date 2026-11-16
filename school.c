#include "school.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static float clamp(float x, float lo, float hi) {
  return fminf(hi, fmaxf(lo, x));
}
static float random_unit(School *s) {
  uint32_t x = s->random_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s->random_state = x;
  return (float)(x >> 8) / 16777216;
}

static void spawn_fish(School *s, int i) {
  Particle *p = &s->particles[i];
  float angle = random_unit(s) * 6.283185307f;
  p->position = vec3_create(30 + random_unit(s) * fmaxf(1, s->width - 60),
                            30 + random_unit(s) * fmaxf(1, s->height - 60),
                            random_unit(s) * SCHOOL_DEPTH);
  p->max_speed = 0.7f + random_unit(s) * 0.6f;
  p->max_force = 0.035f;
  p->velocity =
      vec3_create(cosf(angle) * p->max_speed, sinf(angle) * p->max_speed,
                  (random_unit(s) - 0.5f) * 0.5f);
  p->acceleration = vec3_create(0, 0, 0);
  float length = 58 + random_unit(s) * 24;
  float girth = 0.8f + random_unit(s) * 0.4f;
  fish_body_init(&s->bodies[i], p->position.x, p->position.y, angle, length,
                 girth, random_unit(s) * 6.283185307f);
}

static void rebuild_grid(School *s) {
  spatial_grid_destroy(s->grid);
  s->radius = clamp(fminf(s->width, s->height) * 0.23f, 70, 150);
  s->grid = spatial_grid_create(s->width, s->height, SCHOOL_DEPTH,
                                s->radius * 1.5f, SCHOOL_MAX_FISH);
}

School *school_create(float width, float height, int count, uint32_t seed) {
  School *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;
  s->width = fmaxf(100, width);
  s->height = fmaxf(100, height);
  s->speed = 1;
  s->scale = 1;
  s->pointer_x = s->width * 0.5f;
  s->pointer_y = s->height * 0.5f;
  s->count = (int)clamp(count, 1, SCHOOL_MAX_FISH);
  rebuild_grid(s);
  school_reset(s, seed);
  return s;
}

void school_destroy(School *s) {
  if (!s)
    return;
  spatial_grid_destroy(s->grid);
  free(s);
}

void school_reset(School *s, uint32_t seed) {
  s->seed = seed ? seed : 1;
  s->random_state = s->seed;
  s->time = s->accumulator = 0;
  for (int i = 0; i < s->count; ++i)
    spawn_fish(s, i);
}

void school_set_count(School *s, int count) {
  int next = (int)clamp(count, 1, SCHOOL_MAX_FISH);
  for (int i = s->count; i < next; ++i)
    spawn_fish(s, i);
  s->count = next;
}

void school_resize(School *s, float width, float height) {
  width = fmaxf(100, width);
  height = fmaxf(100, height);
  float rx = width / s->width, ry = height / s->height;
  s->width = width;
  s->height = height;
  for (int i = 0; i < s->count; ++i) {
    Particle *p = &s->particles[i];
    FishBody *b = &s->bodies[i];
    p->position.x *= rx;
    p->position.y *= ry;
    fish_body_init(b, p->position.x, p->position.y, b->angles[0], b->length,
                   b->girth, b->phase);
  }
  rebuild_grid(s);
}

void school_set_obstacle(School *s, int index, float x, float y, float width,
                         float height, float padding) {
  if (index < 0 || index >= SCHOOL_MAX_OBSTACLES)
    return;
  s->obstacles[index] = (SchoolObstacle){x,
                                         y,
                                         fmaxf(0, width),
                                         fmaxf(0, height),
                                         fmaxf(0, padding),
                                         width > 0 && height > 0};
}

void school_clear_obstacles(School *s) {
  memset(s->obstacles, 0, sizeof(s->obstacles));
}

static Vector3D avoidance(School *s, Particle *p, FishBody *body) {
  Vector3D result = vec3_create(0, 0, 0);
  float clearance = body->length * s->scale * 0.7f;
  for (int i = 0; i < SCHOOL_MAX_OBSTACLES; ++i) {
    SchoolObstacle *o = &s->obstacles[i];
    if (!o->active)
      continue;
    float pad = o->padding + clearance;
    float left = o->x - pad, right = o->x + o->width + pad;
    float top = o->y - pad, bottom = o->y + o->height + pad;
    float x = p->position.x + p->velocity.x * 22;
    float y = p->position.y + p->velocity.y * 22;
    if (x < left || x > right || y < top || y > bottom)
      continue;
    /* An obstacle at a screen edge must not steer fish out of the world. */
    float distances[4] = {left >= 1 ? x - left : INFINITY,
                          right <= s->width - 1 ? right - x : INFINITY,
                          top >= 1 ? y - top : INFINITY,
                          bottom <= s->height - 1 ? bottom - y : INFINITY};
    int side = 0;
    for (int k = 1; k < 4; ++k)
      if (distances[k] < distances[side])
        side = k;
    if (!isfinite(distances[side]))
      continue;
    float force = p->max_force * 5;
    if (side == 0)
      result.x -= force;
    if (side == 1)
      result.x += force;
    if (side == 2)
      result.y -= force;
    if (side == 3)
      result.y += force;
  }
  float margin = fminf(80, fminf(s->width, s->height) * 0.18f);
  float force = p->max_force * 2.2f;
  if (p->position.x < margin)
    result.x += force * (1 - p->position.x / margin);
  if (p->position.x > s->width - margin)
    result.x -= force * (1 - (s->width - p->position.x) / margin);
  if (p->position.y < margin)
    result.y += force * (1 - p->position.y / margin);
  if (p->position.y > s->height - margin)
    result.y -= force * (1 - (s->height - p->position.y) / margin);
  if (p->position.z < 60)
    result.z += force * 0.4f;
  if (p->position.z > SCHOOL_DEPTH - 60)
    result.z -= force * 0.4f;
  return result;
}

static void step(School *s) {
  s->time += SCHOOL_STEP;
  float a = s->time * 0.18f;
  Vector3D leaders[2] = {{s->width * (0.52f + cosf(a) * 0.28f),
                          s->height * (0.5f + sinf(a) * 0.24f),
                          SCHOOL_DEPTH * (0.5f + sinf(a * 1.5f) * 0.25f)},
                         {s->width * (0.52f - cosf(a) * 0.28f),
                          s->height * (0.5f - sinf(a) * 0.24f),
                          SCHOOL_DEPTH * (0.5f + cosf(a * 1.5f) * 0.25f)}};
  if (s->follow_pointer) {
    leaders[0].x = leaders[1].x = s->pointer_x;
    leaders[0].y = leaders[1].y = s->pointer_y;
  }
  /* All fish read one frame snapshot, independent of update order. */
  Particle previous[SCHOOL_MAX_FISH];
  memcpy(previous, s->particles, sizeof(Particle) * s->count);
  spatial_grid_update(s->grid, previous, s->count);
  for (int i = 0; i < s->count; ++i) {
    Particle *p = &s->particles[i];
    int *neighbors, neighbor_count;
    spatial_grid_query_neighbors(s->grid, previous, &p->position, s->radius,
                                 &neighbors, &neighbor_count);
    particle_flock_optimized(p, i, previous, neighbors, neighbor_count,
                             &leaders[0], &leaders[1], s->radius, 1.4f, false,
                             false);
    Vector3D steer = avoidance(s, p, &s->bodies[i]);
    vec3_add(&p->acceleration, &steer);
    particle_update(p);
    if (p->position.x < 1 || p->position.x > s->width - 1) {
      p->position.x = clamp(p->position.x, 1, s->width - 1);
      p->velocity.x *= -1;
    }
    if (p->position.y < 1 || p->position.y > s->height - 1) {
      p->position.y = clamp(p->position.y, 1, s->height - 1);
      p->velocity.y *= -1;
    }
    if (p->position.z < 1 || p->position.z > SCHOOL_DEPTH - 1) {
      p->position.z = clamp(p->position.z, 1, SCHOOL_DEPTH - 1);
      p->velocity.z *= -1;
    }
    fish_body_resolve(&s->bodies[i], p->position.x, p->position.y,
                      p->velocity.x / SCHOOL_STEP, p->velocity.y / SCHOOL_STEP,
                      SCHOOL_STEP);
  }
}

void school_advance(School *s, float frame_seconds) {
  if (s->paused || !isfinite(frame_seconds) || frame_seconds <= 0)
    return;
  float speed = isfinite(s->speed) ? clamp(s->speed, 0, 3) : 1;
  s->accumulator += fminf(frame_seconds * speed, 0.25f);
  while (s->accumulator + 0.000001f >= SCHOOL_STEP) {
    step(s);
    s->accumulator = fmaxf(0, s->accumulator - SCHOOL_STEP);
  }
}

void school_draw_order(const School *s, int *indices) {
  for (int i = 0; i < s->count; ++i) {
    int j = i;
    while (j > 0 && s->particles[indices[j - 1]].position.z <
                        s->particles[i].position.z) {
      indices[j] = indices[j - 1];
      --j;
    }
    indices[j] = i;
  }
}
