#include "school.h"
#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
  float x, y, r, g, b, a;
} Vertex;
typedef struct {
  float r, g, b, a;
} Color;
#define MAX_VERTICES 300000
static Vertex vertices[MAX_VERTICES];
static int vertex_count, debug_spines, depth_enabled = 1;
static int dropped_triangles;
static School *school;
static GLuint program, buffer;
static GLint resolution;
static double last_time;
static float frame_ms;

static FishPoint add(FishPoint a, FishPoint b) {
  return (FishPoint){a.x + b.x, a.y + b.y};
}
static FishPoint mul(FishPoint p, float x) {
  return (FishPoint){p.x * x, p.y * x};
}
static FishPoint direction(float angle) {
  return (FishPoint){cosf(angle), sinf(angle)};
}
static void vertex(FishPoint p, Color c) {
  if (vertex_count < MAX_VERTICES)
    vertices[vertex_count++] = (Vertex){p.x, p.y, c.r, c.g, c.b, c.a};
}
static void triangle(FishPoint a, FishPoint b, FishPoint c, Color ink) {
  if (vertex_count + 3 > MAX_VERTICES) {
    dropped_triangles++;
    return;
  }
  vertex(a, ink);
  vertex(b, ink);
  vertex(c, ink);
}
static void line(FishPoint a, FishPoint b, float width, Color ink) {
  float dx = b.x - a.x, dy = b.y - a.y, length = hypotf(dx, dy);
  if (length < 0.0001f)
    return;
  FishPoint n = {-dy / length * width * 0.5f, dx / length * width * 0.5f};
  triangle(add(a, n), add(a, mul(n, -1)), add(b, n), ink);
  triangle(add(b, n), add(a, mul(n, -1)), add(b, mul(n, -1)), ink);
}
static void circle(FishPoint center, float radius, Color ink) {
  for (int i = 0; i < 12; ++i) {
    triangle(center, add(center, mul(direction(i * 6.283185307f / 12), radius)),
             add(center, mul(direction((i + 1) * 6.283185307f / 12), radius)),
             ink);
  }
}
static FishPoint transform(FishPoint p, FishPoint head, float scale) {
  return (FishPoint){head.x + (p.x - head.x) * scale,
                     head.y + (p.y - head.y) * scale};
}
static FishPoint catmull(FishPoint a, FishPoint b, FishPoint c, FishPoint d,
                         float t) {
  float t2 = t * t, t3 = t2 * t;
  return (FishPoint){.5f * ((2 * b.x) + (-a.x + c.x) * t +
                            (2 * a.x - 5 * b.x + 4 * c.x - d.x) * t2 +
                            (-a.x + 3 * b.x - 3 * c.x + d.x) * t3),
                     .5f * ((2 * b.y) + (-a.y + c.y) * t +
                            (2 * a.y - 5 * b.y + 4 * c.y - d.y) * t2 +
                            (-a.y + 3 * b.y - 3 * c.y + d.y) * t3)};
}
static void polygon(FishPoint *p, int count, Color fill, Color stroke,
                    float width) {
  for (int i = 1; i < count - 1; ++i)
    triangle(p[0], p[i], p[i + 1], fill);
  for (int i = 0; i < count; ++i)
    line(p[i], p[(i + 1) % count], width, stroke);
}

static void draw_fin(const FishBody *b, int joint, int side, float scale,
                     Color fill, Color stroke) {
  FishPoint head = b->joints[0], base = b->joints[joint];
  FishPoint f = direction(b->angles[joint]);
  FishPoint n = mul((FishPoint){-f.y, f.x}, (float)side);
  float r = fish_body_radius(b, joint),
        length = b->length * (joint == 3 ? .22f : .12f);
  float flap = 1 + .10f * sinf(b->phase + side * .7f);
  FishPoint a = add(base, mul(n, r * .65f));
  FishPoint tip =
      add(add(base, mul(n, r + length * flap)), mul(f, -length * .45f));
  FishPoint back = add(a, mul(f, -length * .8f));
  FishPoint control_a = add(add(a, mul(n, length * .8f)), mul(f, length * .2f));
  FishPoint control_b =
      add(add(back, mul(n, length * .8f)), mul(f, -length * .15f));
  FishPoint p[17];
  p[0] = a;
  for (int i = 1; i <= 8; ++i) {
    float t = i / 8.0f, u = 1 - t;
    p[i] = add(add(mul(a, u * u), mul(control_a, 2 * u * t)), mul(tip, t * t));
    p[8 + i] =
        add(add(mul(tip, u * u), mul(control_b, 2 * u * t)), mul(back, t * t));
  }
  for (int i = 0; i < 17; ++i)
    p[i] = transform(p[i], head, scale);
  polygon(p, 17, fill, stroke, .75f * scale);
}

static void draw_fish(int index) {
  const FishBody *b = &school->bodies[index];
  float z = school->particles[index].position.z / SCHOOL_DEPTH;
  float scale = school->scale * (depth_enabled ? 1.2f - z * .65f : 1);
  float alpha = depth_enabled ? 1 - z * .65f : 1;
  Color stroke = {.114f, .239f, .561f, .88f * alpha};
  Color fill = {.114f, .239f, .561f, .22f * alpha};
  Color fin_fill = {.114f, .239f, .561f, .13f * alpha};
  FishPoint head = b->joints[0];
  draw_fin(b, 3, -1, scale, fin_fill, stroke);
  draw_fin(b, 3, 1, scale, fin_fill, stroke);
  draw_fin(b, 7, -1, scale, fin_fill, stroke);
  draw_fin(b, 7, 1, scale, fin_fill, stroke);

  FishPoint f = direction(b->angles[11]), n = {-f.y, f.x};
  float tail_width = b->length * .11f, spacing = b->length / (FISH_JOINTS - 1);
  FishPoint tail[5] = {
      b->joints[9],
      add(add(b->joints[11], mul(n, tail_width)), mul(f, -spacing * .35f)),
      add(b->joints[11], mul(f, spacing * .55f)),
      add(add(b->joints[11], mul(n, -tail_width)), mul(f, -spacing * .35f)),
      b->joints[9]};
  for (int i = 0; i < 5; ++i)
    tail[i] = transform(tail[i], head, scale);
  triangle(tail[0], tail[1], tail[2], fin_fill);
  triangle(tail[0], tail[2], tail[3], fin_fill);
  for (int i = 0; i < 4; ++i)
    line(tail[i], tail[i + 1], .8f * scale, stroke);

  FishPoint left[FISH_BODY_JOINTS], right[FISH_BODY_JOINTS];
  for (int i = 0; i < FISH_BODY_JOINTS; ++i) {
    FishPoint normal = direction(b->angles[i] + 1.570796327f);
    float r = fish_body_radius(b, i);
    left[i] = transform(add(b->joints[i], mul(normal, r)), head, scale);
    right[i] = transform(add(b->joints[i], mul(normal, -r)), head, scale);
  }
  FishPoint last_left = left[0], last_right = right[0];
  for (int i = 0; i < FISH_BODY_JOINTS - 1; ++i) {
    int a = i > 0 ? i - 1 : i, d = i + 2 < FISH_BODY_JOINTS ? i + 2 : i + 1;
    for (int k = 1; k <= 3; ++k) {
      float t = k / 3.0f;
      FishPoint l = catmull(left[a], left[i], left[i + 1], left[d], t);
      FishPoint r = catmull(right[a], right[i], right[i + 1], right[d], t);
      triangle(last_left, last_right, l, fill);
      triangle(l, last_right, r, fill);
      line(last_left, l, .9f * scale, stroke);
      line(last_right, r, .9f * scale, stroke);
      last_left = l;
      last_right = r;
    }
  }
  line(last_left, last_right, .8f * scale, stroke);
  float radius = fish_body_radius(b, 0) * scale, heading = b->angles[0];
  for (int i = 0; i < 12; ++i) {
    FishPoint a =
        add(head, mul(direction(heading - 1.570796327f + i * 3.141592654f / 12),
                      radius));
    FishPoint c =
        add(head,
            mul(direction(heading - 1.570796327f + (i + 1) * 3.141592654f / 12),
                radius));
    triangle(head, a, c, fill);
    line(a, c, .9f * scale, stroke);
  }
  FishPoint forward = direction(heading), normal = {-forward.y, forward.x};
  for (int side = -1; side <= 1; side += 2) {
    FishPoint eye = add(add(head, mul(forward, radius * .2f)),
                        mul(normal, radius * .65f * side));
    circle(eye, fmaxf(.65f, 1.15f * scale), stroke);
  }
  if (debug_spines) {
    Color debug = {.48f, .20f, .28f, .95f};
    for (int i = 0; i < FISH_JOINTS; ++i) {
      FishPoint p = transform(b->joints[i], head, scale);
      if (i)
        line(transform(b->joints[i - 1], head, scale), p, scale, debug);
      circle(p, 1.8f * scale, debug);
    }
  }
}

static GLuint shader(GLenum type, const char *source) {
  GLuint id = glCreateShader(type);
  glShaderSource(id, 1, &source, NULL);
  glCompileShader(id);
  GLint ok;
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char message[1024];
    glGetShaderInfoLog(id, sizeof(message), NULL, message);
    fprintf(stderr, "Shader: %s\n", message);
    return 0;
  }
  return id;
}

static int setup_renderer(void) {
  const char *vs =
      "#version 300 es\nlayout(location=0) in vec2 position;\n"
      "layout(location=1) in vec4 color;uniform vec2 resolution;out vec4 ink;\n"
      "void "
      "main(){gl_Position=vec4(position.x/resolution.x*2.-1.,1.-position.y/"
      "resolution.y*2.,0.,1.);ink=color;}";
  const char *fs = "#version 300 es\nprecision mediump float;in vec4 ink;out "
                   "vec4 color;void main(){color=ink;}";
  GLuint vert = shader(GL_VERTEX_SHADER, vs),
         frag = shader(GL_FRAGMENT_SHADER, fs);
  if (!vert || !frag)
    return 0;
  program = glCreateProgram();
  glAttachShader(program, vert);
  glAttachShader(program, frag);
  glLinkProgram(program);
  glDeleteShader(vert);
  glDeleteShader(frag);
  GLint ok;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok)
    return 0;
  resolution = glGetUniformLocation(program, "resolution");
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, x));
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, r));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0, 0, 0, 0);
  return 1;
}

static EM_BOOL frame(double timestamp, void *unused) {
  (void)unused;
  double start = emscripten_get_now();
  float dt = last_time ? (float)((timestamp - last_time) / 1000) : SCHOOL_STEP;
  last_time = timestamp;
  school_advance(school, dt);
  vertex_count = 0;
  dropped_triangles = 0;
  int order[SCHOOL_MAX_FISH];
  school_draw_order(school, order);
  for (int i = 0; i < school->count; ++i)
    draw_fish(order[i]);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(program);
  glUniform2f(resolution, school->width, school->height);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex), vertices,
               GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, vertex_count);
  frame_ms = frame_ms * .9f + (float)(emscripten_get_now() - start) * .1f;
  return EM_TRUE;
}

EMSCRIPTEN_KEEPALIVE void resize_canvas(int width, int height, float dpr) {
  if (!school || width < 1 || height < 1)
    return;
  dpr = fminf(2, fmaxf(1, dpr));
  emscripten_set_canvas_element_size("#canvas", (int)(width * dpr),
                                     (int)(height * dpr));
  glViewport(0, 0, (int)(width * dpr), (int)(height * dpr));
  school_resize(school, (float)width, (float)height);
}
EMSCRIPTEN_KEEPALIVE void set_count(int count) {
  if (school)
    school_set_count(school, count);
}
EMSCRIPTEN_KEEPALIVE void set_speed(float speed) {
  if (school)
    school->speed = fminf(3, fmaxf(0, speed));
}
EMSCRIPTEN_KEEPALIVE void set_scale(float scale) {
  if (school)
    school->scale = fminf(2, fmaxf(.5f, scale));
}
EMSCRIPTEN_KEEPALIVE void set_debug(int enabled) {
  debug_spines = enabled != 0;
}
EMSCRIPTEN_KEEPALIVE void set_depth(int enabled) {
  depth_enabled = enabled != 0;
}
EMSCRIPTEN_KEEPALIVE void set_paused(int paused) {
  if (school)
    school->paused = paused != 0;
}
EMSCRIPTEN_KEEPALIVE void set_follow(int enabled) {
  if (school)
    school->follow_pointer = enabled != 0;
}
EMSCRIPTEN_KEEPALIVE void set_pointer(float x, float y) {
  if (school) {
    school->pointer_x = x;
    school->pointer_y = y;
  }
}
EMSCRIPTEN_KEEPALIVE void reset_school(int seed) {
  if (school)
    school_reset(school, (uint32_t)seed);
}
EMSCRIPTEN_KEEPALIVE void set_obstacle(int index, float x, float y, float w,
                                       float h, float pad) {
  if (school)
    school_set_obstacle(school, index, x, y, w, h, pad);
}
EMSCRIPTEN_KEEPALIVE void clear_obstacles(void) {
  if (school)
    school_clear_obstacles(school);
}
EMSCRIPTEN_KEEPALIVE int get_fish_count(void) {
  return school ? school->count : 0;
}
EMSCRIPTEN_KEEPALIVE float get_simulation_time(void) {
  return school ? school->time : 0;
}
EMSCRIPTEN_KEEPALIVE float get_frame_ms(void) { return frame_ms; }
EMSCRIPTEN_KEEPALIVE int get_dropped_triangles(void) {
  return dropped_triangles;
}

int main(void) {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;
  attrs.alpha = EM_TRUE;
  attrs.depth = EM_FALSE;
  attrs.antialias = EM_TRUE;
  attrs.premultipliedAlpha = EM_TRUE;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx =
      emscripten_webgl_create_context("#canvas", &attrs);
  if (ctx <= 0 ||
      emscripten_webgl_make_context_current(ctx) != EMSCRIPTEN_RESULT_SUCCESS) {
    fprintf(stderr, "WebGL 2 is unavailable\n");
    return 1;
  }
  if (!setup_renderer())
    return 1;
  double width, height;
  emscripten_get_element_css_size("#canvas", &width, &height);
  school =
      school_create((float)width, (float)height, width <= 720 ? 16 : 40, 2026);
  if (!school)
    return 1;
  resize_canvas((int)width, (int)height,
                (float)emscripten_get_device_pixel_ratio());
  emscripten_request_animation_frame_loop(frame, NULL);
  return 0;
}
