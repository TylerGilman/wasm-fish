#ifndef WASM_FISH_BODY_H
#define WASM_FISH_BODY_H

#define FISH_JOINTS 12
#define FISH_BODY_JOINTS 10
#define FISH_MAX_BEND 0.3926990817f

typedef struct {
  float x, y;
} FishPoint;
typedef struct {
  FishPoint joints[FISH_JOINTS];
  float angles[FISH_JOINTS];
  float length, girth, phase;
} FishBody;

/* Based on Argonaut's angle-constrained chain. See THIRD_PARTY_NOTICES.md. */
void fish_body_init(FishBody *body, float x, float y, float heading,
                    float length, float girth, float phase);
void fish_body_resolve(FishBody *body, float x, float y, float velocity_x,
                       float velocity_y, float dt);
float fish_body_radius(const FishBody *body, int joint);
#endif
