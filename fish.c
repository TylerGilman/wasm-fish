#include "fish.h"
#include <math.h>

static float clamp(float x, float lo, float hi) {
  return fminf(hi, fmaxf(lo, x));
}
static float angle_delta(float angle, float anchor) {
  return atan2f(sinf(angle - anchor), cosf(angle - anchor));
}

void fish_body_init(FishBody *body, float x, float y, float heading,
                    float length, float girth, float phase) {
  body->length = fmaxf(12, length);
  body->girth = clamp(girth, 0.5f, 1.6f);
  body->phase = phase;
  body->speed = body->turn = body->pelvic_turn = 0;
  float spacing = body->length / (FISH_JOINTS - 1);
  for (int i = 0; i < FISH_JOINTS; ++i) {
    body->joints[i] = (FishPoint){x - cosf(heading) * spacing * i,
                                  y - sinf(heading) * spacing * i};
    body->angles[i] = heading;
  }
}

void fish_body_resolve(FishBody *body, float x, float y, float velocity_x,
                       float velocity_y, float dt) {
  if (!isfinite(dt) || dt <= 0)
    return;
  float speed = hypotf(velocity_x, velocity_y);
  float heading = body->angles[0];
  if (speed > 0.001f) {
    float desired = atan2f(velocity_y, velocity_x);
    heading += clamp(angle_delta(desired, heading), -3.8f * dt, 3.8f * dt);
  }
  /* Curvature in body lengths: length / turn radius, with a low-speed guard.
     A signed signal distinguishes the inside and outside of the turn. */
  float angular_speed = angle_delta(heading, body->angles[0]) / dt;
  float target_turn = clamp(
      angular_speed * body->length / fmaxf(speed, body->length * .35f), -1, 1);
  body->speed = speed;
  body->turn += (target_turn - body->turn) * (1 - expf(-dt / .12f));
  body->pelvic_turn +=
      (body->turn - body->pelvic_turn) * (1 - expf(-dt / .28f));
  body->phase = fmodf(body->phase + dt * (5.5f + speed * .05f), 6.283185307f);
  body->joints[0] = (FishPoint){x, y};
  body->angles[0] = heading;
  float spacing = body->length / (FISH_JOINTS - 1);
  for (int i = 1; i < FISH_JOINTS; ++i) {
    FishPoint anchor = body->joints[i - 1], old = body->joints[i];
    float dx = anchor.x - old.x, dy = anchor.y - old.y;
    float angle = hypotf(dx, dy) > 0.0001f ? atan2f(dy, dx) : body->angles[i];
    float t = (float)i / (FISH_JOINTS - 1);
    angle += sinf(body->phase - t * 4.2f) * 0.025f * t;
    float bend = clamp(angle_delta(angle, body->angles[i - 1]), -FISH_MAX_BEND,
                       FISH_MAX_BEND);
    angle = body->angles[i - 1] + bend;
    body->angles[i] = atan2f(sinf(angle), cosf(angle));
    body->joints[i] = (FishPoint){anchor.x - cosf(angle) * spacing,
                                  anchor.y - sinf(angle) * spacing};
  }
}

float fish_body_radius(const FishBody *body, int joint) {
  /* Normalized from the profile in Argonaut's Fish.pde. */
  static const float widths[FISH_BODY_JOINTS] = {68, 81, 84, 83, 77,
                                                 64, 51, 38, 32, 19};
  if (joint < 0 || joint >= FISH_BODY_JOINTS)
    return 0;
  return body->length * 0.115f * body->girth * widths[joint] / 84;
}

static FishPoint plus(FishPoint a, FishPoint b) {
  return (FishPoint){a.x + b.x, a.y + b.y};
}

static FishPoint times(FishPoint p, float scale) {
  return (FishPoint){p.x * scale, p.y * scale};
}

static FishPoint quadratic(FishPoint a, FishPoint control, FishPoint b,
                           float t) {
  float u = 1 - t;
  return plus(plus(times(a, u * u), times(control, 2 * u * t)),
              times(b, t * t));
}

int fish_fin_shape(const FishBody *body, int joint, int side, FishFin *fin) {
  if (!body || !fin || joint < 0 || joint >= FISH_BODY_JOINTS ||
      (side != -1 && side != 1))
    return 0;
  int pectoral = joint < 5;
  float turn = pectoral ? body->turn : body->pelvic_turn * .55f;
  float inside = side * turn;
  float glide = clamp(body->speed / (body->length * 1.4f), 0, 1);
  float spread = (pectoral ? .48f : .36f) + .65f * inside + .15f * fabsf(turn);
  spread -= .10f * glide * (1 - fabsf(turn));
  spread += .025f * sinf(body->phase + side * 1.2f) * (1 - .65f * fabsf(turn));
  spread = clamp(spread, .12f, 1.35f);

  FishPoint forward = {cosf(body->angles[joint]), sinf(body->angles[joint])};
  FishPoint outward = times((FishPoint){-forward.y, forward.x}, (float)side);
  FishPoint axis =
      plus(times(forward, -cosf(spread)), times(outward, sinf(spread)));
  float length = body->length * (pectoral ? .22f : .12f);
  float chord = length * (pectoral ? .24f : .26f);
  fin->attachment = plus(body->joints[joint],
                         times(outward, fish_body_radius(body, joint) * .8f));
  fin->tip = plus(fin->attachment, times(axis, length));
  FishPoint front = plus(fin->attachment, times(forward, chord * .5f));
  FishPoint back = plus(fin->attachment, times(forward, -chord * .5f));
  FishPoint control_a = plus(plus(fin->attachment, times(axis, length * .55f)),
                             times(forward, chord * .65f));
  FishPoint control_b = plus(plus(fin->attachment, times(axis, length * .58f)),
                             times(forward, -chord * .35f));
  fin->outline[0] = front;
  for (int i = 1; i <= 8; ++i) {
    float t = i / 8.0f;
    fin->outline[i] = quadratic(front, control_a, fin->tip, t);
    fin->outline[8 + i] = quadratic(fin->tip, control_b, back, t);
  }
  return 1;
}
