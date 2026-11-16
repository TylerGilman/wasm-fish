#include "fish.h"
#include <math.h>

static float clamp(float x, float lo, float hi) {
  return fminf(hi, fmaxf(lo, x));
}
static float angle_delta(float angle, float anchor) {
  return atan2f(sinf(angle - anchor), cosf(angle - anchor));
}

/* Exact critically damped response for a target held over this step. */
static void spring(float *value, float *rate, float target, float frequency,
                   float dt) {
  float error = *value - target;
  float offset = (*rate + frequency * error) * dt;
  float decay = expf(-frequency * dt);
  *value = target + (error + offset) * decay;
  *rate = (*rate - frequency * offset) * decay;
}

static void resolve_fins(FishBody *body, float dt) {
  float glide = clamp(body->speed / (body->length * 1.4f), 0, 1);
  for (int pair = 0; pair < 2; ++pair) {
    int joint = pair == 0 ? 3 : 7;
    float turn = pair == 0 ? body->turn : body->pelvic_turn * .55f;
    for (int index = 0; index < 2; ++index) {
      int side = index == 0 ? -1 : 1;
      FishFinPose *pose = &body->fins[pair][index];
      float previous_heading = pose->heading;
      float lag = pair == 0 ? .08f : .12f;
      pose->heading += angle_delta(body->angles[joint], pose->heading) *
                       (1 - expf(-dt / lag));
      pose->heading = atan2f(sinf(pose->heading), cosf(pose->heading));
      float angular_speed = angle_delta(pose->heading, previous_heading) / dt;
      float inside = side * turn;
      float target_spread = (pair == 0 ? .54f : .40f) + .8f * inside +
                            .12f * fabsf(turn) - .08f * glide;
      target_spread = clamp(target_spread, .28f, 1.30f);
      spring(&pose->spread, &pose->spread_rate, target_spread,
             pair == 0 ? 12 : 9, dt);

      /* The flexible tip trails changes at the root, then curls into a turn.
         Its slower response absorbs rapid steering changes and body waves. */
      float target_flex = .95f * inside + .12f * side * angular_speed -
                          .20f * pose->spread_rate;
      target_flex += .035f * sinf(body->phase * .7f + side * .6f);
      target_flex = clamp(target_flex, fmaxf(-.95f, -.12f - pose->spread),
                          fminf(.95f, 1.95f - pose->spread));
      spring(&pose->flex, &pose->flex_rate, target_flex, pair == 0 ? 7 : 5.5f,
             dt);
    }
  }
}

void fish_body_init(FishBody *body, float x, float y, float heading,
                    float length, float girth, float phase) {
  body->length = fmaxf(12, length);
  body->girth = clamp(girth, 0.5f, 1.6f);
  body->phase = phase;
  body->speed = body->turn = body->pelvic_turn = 0;
  for (int pair = 0; pair < 2; ++pair)
    for (int side = 0; side < 2; ++side)
      body->fins[pair][side] =
          (FishFinPose){.heading = heading, .spread = pair == 0 ? .54f : .40f};
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
  resolve_fins(body, dt);
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

int fish_fin_shape(const FishBody *body, int joint, int side, FishFin *fin) {
  if (!body || !fin || joint < 0 || joint >= FISH_BODY_JOINTS ||
      (side != -1 && side != 1))
    return 0;
  int pair = joint < 5 ? 0 : 1;
  const FishFinPose *pose = &body->fins[pair][side == -1 ? 0 : 1];
  FishPoint forward = {cosf(body->angles[joint]), sinf(body->angles[joint])};
  FishPoint outward = times((FishPoint){-forward.y, forward.x}, (float)side);
  float heading = pose->heading + angle_delta(body->angles[joint],
                                              body->angles[pair == 0 ? 3 : 7]);
  float length = body->length * (pair == 0 ? .24f : .14f);
  float chord = length * .24f;
  float flex = clamp(pose->flex, -.95f, .95f);
  fin->attachment = plus(body->joints[joint],
                         times(outward, fish_body_radius(body, joint) * .9f));
  FishPoint center = fin->attachment;
  for (int i = 0; i < FISH_FIN_STATIONS; ++i) {
    float t = i / (float)(FISH_FIN_STATIONS - 1);
    if (i) {
      float middle = (i - .5f) / (FISH_FIN_STATIONS - 1);
      float angle = heading + 3.141592654f -
                    side * (pose->spread + flex * middle * middle);
      FishPoint tangent = {cosf(angle), sinf(angle)};
      center = plus(center, times(tangent, length / (FISH_FIN_STATIONS - 1)));
    }
    float angle = heading + 3.141592654f - side * (pose->spread + flex * t * t);
    FishPoint normal = {sinf(angle) * side, -cosf(angle) * side};
    float width =
        chord * .5f * powf(1 - t, .7f) * (.78f + .4f * sinf(t * 3.141592654f));
    fin->outline[i] = plus(center, times(normal, width));
    fin->outline[FISH_FIN_POINTS - 1 - i] = plus(center, times(normal, -width));
  }
  fin->tip = center;
  return 1;
}
