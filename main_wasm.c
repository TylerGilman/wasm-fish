#include <GLES2/gl2.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "particle.h"
#include "spatial_grid.h"
#include "vector3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WIDTH 800
#define HEIGHT 600
#define DEPTH 600.0f
#define NUM_PARTICLES 500
#define MAX_OBSTACLES 30

int current_width = WIDTH;
int current_height = HEIGHT;
#define PERCEPTION_RADIUS 50.0f
#define ORBIT_RADIUS 250.0f
#define LEADER_SPEED 0.015f
#define LEADER_INTERPOLATION 0.05f
#define SEPARATION_OSCILLATION_SPEED 0.01f
#define SORT_EVERY_N_FRAMES 3
#define UPDATE_GRID_EVERY_N_FRAMES 2
#define CACHE_NEIGHBORS_FRAMES 3
#define STAGGER_CACHE_UPDATES 1

typedef struct {
  float x, y, width, height;
  float padding;
  bool active;
} Obstacle;

Obstacle obstacles[MAX_OBSTACLES];
int obstacle_count = 0;
Particle *particles;
SpatialGrid *spatial_grid;
Vector3D leader1, leader2;
Vector3D target_leader1, target_leader2;
float leader_angle = 0.0f;
float separation_time = 0.0f;
int frame_counter = 0;

GLuint program;
GLuint vbo;
GLint position_attrib;
GLint alpha_attrib;
GLint size_attrib;
GLint resolution_uniform;
GLint ink_uniform;

/* Ink color of the dots (cobalt, matching the site palette). The canvas is
   transparent, so the page background is never touched. */
#define INK_R 0.114f
#define INK_G 0.239f
#define INK_B 0.561f

const char *vertex_shader_src = "attribute vec2 position;\n"
                                "attribute float alpha;\n"
                                "attribute float size;\n"
                                "uniform vec2 resolution;\n"
                                "varying float vAlpha;\n"
                                "void main() {\n"
                                "    gl_Position = vec4(position.x / (resolution.x * 0.5) - "
                                "1.0, 1.0 - position.y / (resolution.y * 0.5), 0.0, 1.0);\n"
                                "    gl_PointSize = size;\n"
                                "    vAlpha = alpha;\n"
                                "}\n";

/* Round, soft-edged points instead of hard squares. */
const char *fragment_shader_src = "precision mediump float;\n"
                                  "uniform vec3 ink;\n"
                                  "varying float vAlpha;\n"
                                  "void main() {\n"
                                  "    float d = length(gl_PointCoord - vec2(0.5));\n"
                                  "    float edge = smoothstep(0.5, 0.3, d);\n"
                                  "    gl_FragColor = vec4(ink, vAlpha * edge);\n"
                                  "}\n";

int compare_particles_by_z(const void *a, const void *b) {
  const Particle *pa = (const Particle *)a;
  const Particle *pb = (const Particle *)b;
  return (pb->position.z > pa->position.z) - (pb->position.z < pa->position.z);
}

GLuint compile_shader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(shader, 512, NULL, info_log);
    printf("Shader compilation failed: %s\n", info_log);
  }

  return shader;
}

void init_gl() {
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
  GLuint fragment_shader =
      compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);

  program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(program, 512, NULL, info_log);
    printf("Program linking failed: %s\n", info_log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  position_attrib = glGetAttribLocation(program, "position");
  alpha_attrib = glGetAttribLocation(program, "alpha");
  size_attrib = glGetAttribLocation(program, "size");
  resolution_uniform = glGetUniformLocation(program, "resolution");
  ink_uniform = glGetUniformLocation(program, "ink");

  glGenBuffers(1, &vbo);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Fully transparent
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void init_simulation() {
  srand(time(NULL));

  particles = malloc(sizeof(Particle) * NUM_PARTICLES);
  for (int i = 0; i < NUM_PARTICLES; i++) {
    particles[i] = particle_create(((float)rand() / RAND_MAX) * current_width,
                                   ((float)rand() / RAND_MAX) * current_height,
                                   ((float)rand() / RAND_MAX) * DEPTH);
  }

  spatial_grid = spatial_grid_create(current_width, current_height, DEPTH,
                                     PERCEPTION_RADIUS * 1.5f, NUM_PARTICLES);

  leader1 = vec3_create(current_width / 2.0f, current_height / 2.0f, DEPTH / 2.0f);
  leader2 = vec3_create(current_width / 2.0f, current_height / 2.0f, DEPTH / 2.0f);
  target_leader1 = leader1;
  target_leader2 = leader2;

  printf("Flocking Simulation (WebAssembly)\n");
}

// Check if line from p1 to p2 intersects a rectangle
bool line_intersects_rect(float x1, float y1, float x2, float y2,
                          float rx, float ry, float rw, float rh) {
  // Check if either point is inside the rectangle
  if (x1 >= rx && x1 <= rx + rw && y1 >= ry && y1 <= ry + rh) return true;
  if (x2 >= rx && x2 <= rx + rw && y2 >= ry && y2 <= ry + rh) return true;

  // Check line against all four edges
  float dx = x2 - x1;
  float dy = y2 - y1;

  float t[4];
  int valid = 0;

  // Left edge
  if (dx != 0) {
    float tt = (rx - x1) / dx;
    if (tt >= 0 && tt <= 1) {
      float yy = y1 + tt * dy;
      if (yy >= ry && yy <= ry + rh) return true;
    }
  }
  // Right edge
  if (dx != 0) {
    float tt = (rx + rw - x1) / dx;
    if (tt >= 0 && tt <= 1) {
      float yy = y1 + tt * dy;
      if (yy >= ry && yy <= ry + rh) return true;
    }
  }
  // Top edge
  if (dy != 0) {
    float tt = (ry - y1) / dy;
    if (tt >= 0 && tt <= 1) {
      float xx = x1 + tt * dx;
      if (xx >= rx && xx <= rx + rw) return true;
    }
  }
  // Bottom edge
  if (dy != 0) {
    float tt = (ry + rh - y1) / dy;
    if (tt >= 0 && tt <= 1) {
      float xx = x1 + tt * dx;
      if (xx >= rx && xx <= rx + rw) return true;
    }
  }

  return false;
}

// Check if any obstacle blocks line of sight between two points
bool obstacle_blocks_path(float x1, float y1, float x2, float y2) {
  for (int i = 0; i < obstacle_count; i++) {
    if (!obstacles[i].active) continue;
    Obstacle* obs = &obstacles[i];
    if (line_intersects_rect(x1, y1, x2, y2, obs->x, obs->y, obs->width, obs->height)) {
      return true;
    }
  }
  return false;
}

Vector3D calculate_obstacle_avoidance(Particle* p) {
  Vector3D steering = vec3_create(0, 0, 0);

  for (int i = 0; i < obstacle_count; i++) {
    if (!obstacles[i].active) continue;

    Obstacle* obs = &obstacles[i];
    float pad = obs->padding;

    // Center of obstacle
    float cx = obs->x + obs->width / 2.0f;
    float cy = obs->y + obs->height / 2.0f;

    // Half dimensions with padding for soft boundary
    float hw = obs->width / 2.0f + pad;
    float hh = obs->height / 2.0f + pad;

    // Distance from particle to center
    float dx = p->position.x - cx;
    float dy = p->position.y - cy;

    // Normalized distance (0 = at center, 1 = at edge of padded area)
    float nx = fabsf(dx) / hw;
    float ny = fabsf(dy) / hh;

    // Only affect particles within the soft boundary zone
    if (nx < 1.2f && ny < 1.2f) {
      // Calculate how deep into the zone (higher = deeper)
      float penetration = fmaxf(0.0f, 1.0f - fmaxf(nx, ny));

      // Soft exponential falloff - gentle push that increases near center
      float strength = penetration * penetration * 2.0f;

      // Push direction: away from center, biased toward nearest edge
      Vector3D push = vec3_create(0, 0, 0);

      if (nx > ny) {
        // Closer to left/right edge - push horizontally
        push.x = (dx > 0) ? 1.0f : -1.0f;
        push.y = (dy > 0) ? 0.3f : -0.3f;
      } else {
        // Closer to top/bottom edge - push vertically
        push.x = (dx > 0) ? 0.3f : -0.3f;
        push.y = (dy > 0) ? 1.0f : -1.0f;
      }

      vec3_mult(&push, strength);
      vec3_add(&steering, &push);
    }
  }

  return steering;
}

void update_simulation() {
  separation_time += SEPARATION_OSCILLATION_SPEED;
  float separation_weight = 1.1f + sinf(separation_time) * 0.3f;

  leader_angle += LEADER_SPEED;
  target_leader1 =
      vec3_create(current_width / 2.0f + cosf(leader_angle) * ORBIT_RADIUS,
                  current_height / 2.0f + sinf(leader_angle) * ORBIT_RADIUS * 0.7f,
                  DEPTH / 2.0f + sinf(leader_angle * 1.5f) * 100.0f);
  target_leader2 = vec3_create(
      current_width / 2.0f + cosf(leader_angle + M_PI) * ORBIT_RADIUS,
      current_height / 2.0f + sinf(leader_angle + M_PI) * ORBIT_RADIUS * 0.7f,
      DEPTH / 2.0f + cosf(leader_angle * 1.5f) * 100.0f);

  leader1.x += (target_leader1.x - leader1.x) * LEADER_INTERPOLATION;
  leader1.y += (target_leader1.y - leader1.y) * LEADER_INTERPOLATION;
  leader1.z += (target_leader1.z - leader1.z) * LEADER_INTERPOLATION;
  leader2.x += (target_leader2.x - leader2.x) * LEADER_INTERPOLATION;
  leader2.y += (target_leader2.y - leader2.y) * LEADER_INTERPOLATION;
  leader2.z += (target_leader2.z - leader2.z) * LEADER_INTERPOLATION;

  spatial_grid_update(spatial_grid, particles, NUM_PARTICLES);

  for (int i = 0; i < NUM_PARTICLES; i++) {
    int *neighbors;
    int neighbor_count;
    spatial_grid_query_neighbors(spatial_grid, particles,
                                 &particles[i].position, PERCEPTION_RADIUS,
                                 &neighbors, &neighbor_count);

    // Check if leaders are blocked by obstacles
    bool leader1_blocked = obstacle_blocks_path(
        particles[i].position.x, particles[i].position.y,
        leader1.x, leader1.y);
    bool leader2_blocked = obstacle_blocks_path(
        particles[i].position.x, particles[i].position.y,
        leader2.x, leader2.y);

    particle_flock_optimized(&particles[i], i, particles, neighbors,
                             neighbor_count, &leader1, &leader2,
                             PERCEPTION_RADIUS, separation_weight,
                             leader1_blocked, leader2_blocked);

    // Apply obstacle avoidance
    Vector3D avoidance = calculate_obstacle_avoidance(&particles[i]);
    vec3_add(&particles[i].acceleration, &avoidance);

    particle_update(&particles[i]);
    particle_wrap(&particles[i], current_width, current_height, DEPTH);
  }

  frame_counter++;
  if (frame_counter >= SORT_EVERY_N_FRAMES) {
    qsort(particles, NUM_PARTICLES, sizeof(Particle), compare_particles_by_z);
    frame_counter = 0;
  }
}

void render() {
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(program);

  // Update viewport resolution uniform
  glUniform2f(resolution_uniform, (float)current_width, (float)current_height);
  glUniform3f(ink_uniform, INK_R, INK_G, INK_B);

  float vertex_data[NUM_PARTICLES * 4];

  for (int i = 0; i < NUM_PARTICLES; i++) {
    Particle *p = &particles[i];
    float depth_factor = p->position.z / DEPTH;
    float size = 2.0f + ((DEPTH - p->position.z) / DEPTH) * 3.5f;

    // Depth fade: closer = solid ink, farther = faint ink
    float alpha = 1.0f - depth_factor * 0.75f;

    vertex_data[i * 4 + 0] = p->position.x;
    vertex_data[i * 4 + 1] = p->position.y;
    vertex_data[i * 4 + 2] = alpha;
    vertex_data[i * 4 + 3] = size;
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data,
               GL_DYNAMIC_DRAW);

  glVertexAttribPointer(position_attrib, 2, GL_FLOAT, GL_FALSE,
                        4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(position_attrib);

  glVertexAttribPointer(alpha_attrib, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(alpha_attrib);

  glVertexAttribPointer(size_attrib, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(size_attrib);

  glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);
}

void main_loop() {
  update_simulation();
  render();
}

// Exported function to handle canvas resize from JavaScript
EMSCRIPTEN_KEEPALIVE
void resize_canvas(int width, int height) {
  current_width = width;
  current_height = height;
  glViewport(0, 0, width, height);
}

// Exported functions for obstacle avoidance
EMSCRIPTEN_KEEPALIVE
void set_obstacle(int index, float x, float y, float width, float height, float padding) {
  if (index >= 0 && index < MAX_OBSTACLES) {
    obstacles[index].x = x;
    obstacles[index].y = y;
    obstacles[index].width = width;
    obstacles[index].height = height;
    obstacles[index].padding = padding;
    obstacles[index].active = true;
    if (index >= obstacle_count) obstacle_count = index + 1;
  }
}

EMSCRIPTEN_KEEPALIVE
void clear_obstacles() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].active = false;
  }
  obstacle_count = 0;
}

EMSCRIPTEN_KEEPALIVE
int get_obstacle_count() {
  return obstacle_count;
}

int main() {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.alpha = true;
  attrs.depth = false;
  attrs.antialias = true;
  attrs.premultipliedAlpha = false;

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx =
      emscripten_webgl_create_context("#canvas", &attrs);
  emscripten_webgl_make_context_current(ctx);

  // Get actual canvas dimensions
  double canvas_width, canvas_height;
  emscripten_get_element_css_size("#canvas", &canvas_width, &canvas_height);
  current_width = (int)(canvas_width * emscripten_get_device_pixel_ratio());
  current_height = (int)(canvas_height * emscripten_get_device_pixel_ratio());

  // Set initial viewport
  glViewport(0, 0, current_width, current_height);

  init_gl();
  init_simulation();

  emscripten_set_main_loop(main_loop, 0, 1);

  return 0;
}
