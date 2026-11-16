#ifndef WASM_FISH_BODY_H
#define WASM_FISH_BODY_H

#define FISH_JOINTS 12
#define FISH_BODY_JOINTS 10
#define FISH_MAX_BEND 0.3926990817f
#define FISH_FIN_STATIONS 9
#define FISH_FIN_POINTS (FISH_FIN_STATIONS * 2 - 1)

typedef struct {
  float x, y;
} FishPoint;
typedef struct {
  float heading, spread, spread_rate, flex, flex_rate;
} FishFinPose;
typedef struct {
  FishPoint joints[FISH_JOINTS];
  float angles[FISH_JOINTS];
  float length, girth, phase;
  float speed, turn, pelvic_turn;
  FishFinPose fins[2][2]; /* Pectoral/pelvic, then left/right. */
} FishBody;

typedef struct {
  /* Leading edge root to tip, then trailing edge tip to root. */
  FishPoint outline[FISH_FIN_POINTS];
  FishPoint attachment, tip;
} FishFin;

/* Based on Argonaut's angle-constrained chain. See THIRD_PARTY_NOTICES.md. */
void fish_body_init(FishBody *body, float x, float y, float heading,
                    float length, float girth, float phase);
/* Resolver velocities use CSS pixels per second. */
void fish_body_resolve(FishBody *body, float x, float y, float velocity_x,
                       float velocity_y, float dt);
float fish_body_radius(const FishBody *body, int joint);
/* Fin geometry reads the current pose without advancing the simulation.
   Positive turns are clockwise on screen.
   side +1 is the fish's right fin; -1 is its left fin. */
int fish_fin_shape(const FishBody *body, int joint, int side, FishFin *fin);
#endif
