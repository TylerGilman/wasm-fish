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
  float spacing = body->length / (FISH_JOINTS - 1);
  for (int i = 0; i < FISH_JOINTS; ++i) {
    body->joints[i] = (FishPoint){x - cosf(heading) * spacing * i,
                                  y - sinf(heading) * spacing * i};
    body->angles[i] = heading;
  }
}

void fish_body_resolve(FishBody *body, float x, float y, float velocity_x,
                       float velocity_y, float dt) {
  float speed = hypotf(velocity_x, velocity_y);
  float heading = body->angles[0];
  if (speed > 0.001f) {
    float desired = atan2f(velocity_y, velocity_x);
    heading += clamp(angle_delta(desired, heading), -3.8f * dt, 3.8f * dt);
  }
  body->phase = fmodf(body->phase + dt * (5.5f + speed * 3), 6.283185307f);
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
