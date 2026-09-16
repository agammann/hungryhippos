#ifndef HIPPO_GAME_H
#define HIPPO_GAME_H
#include <stdint.h>
#define MARBLE_COUNT 28
#define HIPPO_COUNT 4
#define PI 3.14159265358979323846f
#define ROUND_SECONDS 60.0f
typedef enum { LOBBY, COUNTDOWN, PLAYING, PAUSED, FINISHED } Phase;
typedef struct { float x,y,vx,vy; int alive, owner; } Marble;
typedef struct { float bite, cooldown, think, flash; int score; } Hippo;
typedef struct {
    Phase phase, before_pause;
    Marble balls[MARBLE_COUNT];
    Hippo hippos[HIPPO_COUNT];
    float time, countdown, clock;
    int players, difficulty, remaining, sound_events;
    uint32_t rng;
} Game;
extern const float HIPPO_X[4], HIPPO_Y[4];
void game_init(Game *g, uint32_t seed);
void game_start(Game *g);
void game_step(Game *g, float dt, const int held[4]);
void game_pause(Game *g);
float game_reach(const Hippo *h);
int game_winners(const Game *g);
#endif
