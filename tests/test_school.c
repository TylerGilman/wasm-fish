#include "school.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void constraints(const School *s) {
  for (int i = 0; i < s->count; ++i) {
    const Particle *p = &s->particles[i];
    const FishBody *b = &s->bodies[i];
    assert(isfinite(p->position.x) && isfinite(p->position.y) &&
           isfinite(p->position.z));
    assert(p->position.x >= 0 && p->position.x <= s->width);
    assert(p->position.y >= 0 && p->position.y <= s->height);
    assert(p->position.z >= 0 && p->position.z <= SCHOOL_DEPTH);
    assert(fabsf(b->joints[0].x - p->position.x) < .001f);
    assert(fabsf(b->joints[0].y - p->position.y) < .001f);
    for (int j = 1; j < FISH_JOINTS; ++j) {
      float distance = hypotf(b->joints[j].x - b->joints[j - 1].x,
                              b->joints[j].y - b->joints[j - 1].y);
      assert(fabsf(distance - b->length / (FISH_JOINTS - 1)) < .003f);
      float a = b->angles[j] - b->angles[j - 1];
      assert(fabsf(atan2f(sinf(a), cosf(a))) <= FISH_MAX_BEND + .00001f);
    }
  }
}

int main(void) {
  School *a = school_create(1440, 900, 40, 42),
         *b = school_create(1440, 900, 40, 42);
  assert(a && b);
  for (int i = 0; i < 120; ++i)
    school_advance(a, 1.0f / 60);
  for (int i = 0; i < 240; ++i)
    school_advance(b, 1.0f / 120);
  for (int i = 0; i < a->count; ++i) {
    assert(fabsf(a->particles[i].position.x - b->particles[i].position.x) <
           .001f);
    assert(fabsf(a->particles[i].position.z - b->particles[i].position.z) <
           .001f);
  }
  constraints(a);
  constraints(b);
  float x = a->particles[0].position.x, time = a->time;
  a->paused = 1;
  school_advance(a, 1);
  assert(a->time == time && a->particles[0].position.x == x);
  a->paused = 0;
  int order[SCHOOL_MAX_FISH];
  school_draw_order(a, order);
  assert(a->particles[0].position.x == x);
  for (int i = 1; i < a->count; ++i)
    assert(a->particles[order[i - 1]].position.z >=
           a->particles[order[i]].position.z);
  school_set_count(a, 999);
  assert(a->count == SCHOOL_MAX_FISH);
  school_set_obstacle(a, 0, 400, 200, 300, 200, 20);
  for (int i = 0; i < 1200; ++i) {
    school_advance(a, SCHOOL_STEP);
    constraints(a);
  }
  school_resize(a, 390, 844);
  constraints(a);
  for (int i = 0; i < 600; ++i) {
    school_advance(a, SCHOOL_STEP);
    constraints(a);
  }
  school_set_count(a, 0);
  assert(a->count == 1);
  a->follow_pointer = 1;
  a->pointer_x = 600;
  a->pointer_y = 400;
  school_resize(a, 1000, 800);
  a->particles[0].position = vec3_create(350, 400, 240);
  a->particles[0].velocity = vec3_create(1, 0, 0);
  school_clear_obstacles(a);
  school_set_obstacle(a, 0, 400, 200, 300, 400, 20);
  school_advance(a, SCHOOL_STEP);
  assert(a->particles[0].velocity.x < 1); /* Steer away from the rectangle. */
  a->particles[0].position = vec3_create(5, 300, 240);
  a->particles[0].velocity = vec3_create(-1, 0, 0);
  a->particles[0].max_speed = 1.3f;
  school_clear_obstacles(a);
  school_set_obstacle(a, 0, 0, 0, 300, 600, 20);
  school_advance(a, SCHOOL_STEP);
  assert(a->particles[0].velocity.x > -1); /* Escape into the viewport. */
  school_reset(a, 17);
  constraints(a);
  school_advance(a, NAN);
  constraints(a);
  school_destroy(a);
  school_destroy(b);
  puts("passed: frame-rate independence, pause, depth order, constraints, "
       "count limits, resize, obstacle steering, reset");
  return 0;
}
