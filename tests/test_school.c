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
    assert(isfinite(b->turn) && isfinite(b->pelvic_turn) && isfinite(b->speed));
    for (int j = 1; j < FISH_JOINTS; ++j) {
      float distance = hypotf(b->joints[j].x - b->joints[j - 1].x,
                              b->joints[j].y - b->joints[j - 1].y);
      assert(fabsf(distance - b->length / (FISH_JOINTS - 1)) < .003f);
      float a = b->angles[j] - b->angles[j - 1];
      assert(fabsf(atan2f(sinf(a), cosf(a))) <= FISH_MAX_BEND + .00001f);
    }
  }
}

static FishBody swim_arc(float angular_speed) {
  FishBody body;
  fish_body_init(&body, 0, 0, 0, 70, 1, 0);
  float x = 0, y = 0, heading = 0;
  for (int i = 0; i < 180; ++i) {
    heading += angular_speed * SCHOOL_STEP;
    float vx = cosf(heading) * 90, vy = sinf(heading) * 90;
    x += vx * SCHOOL_STEP;
    y += vy * SCHOOL_STEP;
    fish_body_resolve(&body, x, y, vx, vy, SCHOOL_STEP);
  }
  return body;
}

/* Read the visible fin direction relative to the local tailward body axis. */
static float fin_spread(const FishBody *body, int joint, int side) {
  FishFin fin;
  assert(fish_fin_shape(body, joint, side, &fin));
  float dx = fin.tip.x - fin.attachment.x, dy = fin.tip.y - fin.attachment.y;
  float c = cosf(body->angles[joint]), s = sinf(body->angles[joint]);
  return atan2f(side * (-s * dx + c * dy), -(c * dx + s * dy));
}

static FishPoint fin_center(const FishFin *fin, int station) {
  FishPoint a = fin->outline[station];
  FishPoint b = fin->outline[FISH_FIN_POINTS - 1 - station];
  return (FishPoint){(a.x + b.x) * .5f, (a.y + b.y) * .5f};
}

static float signed_area(FishPoint a, FishPoint b, FishPoint c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static float fin_curve(const FishBody *body, int joint, int side) {
  FishFin fin;
  assert(fish_fin_shape(body, joint, side, &fin));
  FishPoint a = fin_center(&fin, 0), b = fin_center(&fin, 1);
  FishPoint c = fin_center(&fin, FISH_FIN_STATIONS - 2), d = fin.tip;
  float angle = atan2f(d.y - c.y, d.x - c.x) - atan2f(b.y - a.y, b.x - a.x);
  return atan2f(sinf(angle), cosf(angle));
}

static void fin_geometry(const FishBody *body, int joint, int side) {
  FishFin fin;
  assert(fish_fin_shape(body, joint, side, &fin));
  float length = 0, widest = 0, winding = 0;
  for (int i = 0; i < FISH_FIN_POINTS; ++i)
    assert(isfinite(fin.outline[i].x) && isfinite(fin.outline[i].y));
  for (int i = 0; i < FISH_FIN_STATIONS; ++i) {
    FishPoint a = fin.outline[i], b = fin.outline[FISH_FIN_POINTS - 1 - i];
    widest = fmaxf(widest, hypotf(a.x - b.x, a.y - b.y));
    if (i == FISH_FIN_STATIONS - 1)
      continue;
    FishPoint center = fin_center(&fin, i), next = fin_center(&fin, i + 1);
    length += hypotf(next.x - center.x, next.y - center.y);
    float area = signed_area(a, b, fin.outline[i + 1]);
    if (i == 0)
      winding = area;
    assert(area * winding > 0); /* The blade must not fold through itself. */
    if (i < FISH_FIN_STATIONS - 2)
      assert(signed_area(fin.outline[i + 1], b,
                         fin.outline[FISH_FIN_POINTS - 2 - i]) *
                 winding >
             0);
  }
  assert(length > body->length * .1f && length < body->length * .3f);
  assert(widest / length < .25f);
}

static void fin_motion(void) {
  FishBody straight = swim_arc(0), right = swim_arc(.9f), left = swim_arc(-.9f);
  FishBody wide = swim_arc(.15f);
  assert(fabsf(fin_spread(&straight, 3, 1) - fin_spread(&straight, 3, -1)) <
         .06f);
  assert(fin_spread(&right, 3, 1) > fin_spread(&right, 3, -1) + .3f);
  assert(fin_spread(&left, 3, -1) > fin_spread(&left, 3, 1) + .3f);
  float tight_difference = fin_spread(&right, 3, 1) - fin_spread(&right, 3, -1);
  float wide_difference = fin_spread(&wide, 3, 1) - fin_spread(&wide, 3, -1);
  assert(tight_difference > wide_difference + .2f);
  assert(fabsf(fin_curve(&right, 3, 1)) >
         fabsf(fin_curve(&straight, 3, 1)) + .35f);
  assert(fabsf(fin_curve(&right, 3, 1)) > fabsf(fin_curve(&wide, 3, 1)) + .2f);
  assert(fabsf(fin_curve(&left, 3, -1)) >
         fabsf(fin_curve(&straight, 3, -1)) + .35f);
  assert(fabsf(fin_curve(&right, 3, -1)) >
         fabsf(fin_curve(&straight, 3, -1)) + .15f);

  for (int side = -1; side <= 1; side += 2) {
    fin_geometry(&straight, 3, side);
    fin_geometry(&right, 3, side);
    fin_geometry(&left, 3, side);
  }

  FishBody transient;
  fish_body_init(&transient, 0, 0, 0, 70, 1, 0);
  fish_body_resolve(&transient, 1.5f, .05f, cosf(.04f) * 90, sinf(.04f) * 90,
                    SCHOOL_STEP);
  assert(transient.turn > 0 && transient.turn < .2f);
  assert(transient.pelvic_turn > 0 && transient.pelvic_turn < transient.turn);
  float heading = transient.angles[0];
  for (int i = 0; i < 180; ++i)
    fish_body_resolve(&transient, 1.5f, .05f, cosf(heading) * 90,
                      sinf(heading) * 90, SCHOOL_STEP);
  assert(fabsf(transient.turn) < .001f && fabsf(transient.pelvic_turn) < .001f);
  fish_body_resolve(&transient, 1.5f, .05f, 0, 0, SCHOOL_STEP);
  assert(isfinite(fin_spread(&transient, 3, 1)));

  /* Turn reversals and the +/- pi heading boundary must not snap the tips. */
  FishBody reversing;
  fish_body_init(&reversing, 200, 200, 3.12f, 70, 1, 0);
  float x = 200, y = 200, direction = 3.12f;
  float previous[2][2] = {{0}};
  for (int step = 0; step < 1200; ++step) {
    float rate = step % 240 < 120 ? 2.4f : -2.4f;
    direction += rate * SCHOOL_STEP;
    float vx = cosf(direction) * 90, vy = sinf(direction) * 90;
    x += vx * SCHOOL_STEP;
    y += vy * SCHOOL_STEP;
    fish_body_resolve(&reversing, x, y, vx, vy, SCHOOL_STEP);
    for (int pair = 0; pair < 2; ++pair)
      for (int index = 0; index < 2; ++index) {
        int joint = pair == 0 ? 3 : 7, side = index == 0 ? -1 : 1;
        fin_geometry(&reversing, joint, side);
        FishFin fin;
        assert(fish_fin_shape(&reversing, joint, side, &fin));
        float angle =
            atan2f(fin.tip.y - fin.attachment.y, fin.tip.x - fin.attachment.x);
        float change = angle - previous[pair][index];
        if (step)
          assert(fabsf(atan2f(sinf(change), cosf(change))) < .15f);
        previous[pair][index] = angle;
      }
  }
}

int main(void) {
  fin_motion();
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
    for (int side = -1; side <= 1; side += 2) {
      FishFin left, right;
      assert(fish_fin_shape(&a->bodies[i], 3, side, &left));
      assert(fish_fin_shape(&b->bodies[i], 3, side, &right));
      for (int point = 0; point < FISH_FIN_POINTS; ++point) {
        assert(fabsf(left.outline[point].x - right.outline[point].x) < .001f);
        assert(fabsf(left.outline[point].y - right.outline[point].y) < .001f);
      }
    }
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
       "count limits, resize, obstacle steering, reset, flexible fins, "
       "turn reversals, fin mesh winding");
  return 0;
}
