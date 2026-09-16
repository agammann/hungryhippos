#include "../src/game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void valid(const Game *g) {
    int alive=0,scores=0,owners[4]={0};
    for(int i=0;i<MARBLE_COUNT;i++) {
        const Marble *b=&g->balls[i]; assert(isfinite(b->x)&&isfinite(b->y)&&isfinite(b->vx)&&isfinite(b->vy));
        if(b->alive) { alive++; assert(b->owner==-1); assert(hypotf(b->x,b->y)<232); }
        else { assert(b->owner>=0 && b->owner<4); owners[b->owner]++; }
    }
    for(int i=0;i<4;i++) { assert(g->hippos[i].score==owners[i]); scores+=g->hippos[i].score; }
    assert(alive==g->remaining); assert(scores+alive==MARBLE_COUNT);
}
int main(void) {
    Game g; int keys[4]={1,1,1,1}; game_init(&g,42); assert(g.phase==LOBBY); valid(&g);
    game_start(&g); assert(g.phase==COUNTDOWN);
    for(int i=0;i<361;i++) game_step(&g,1.0f/120,keys);
    assert(g.phase==PLAYING); game_pause(&g); assert(g.phase==PAUSED);
    Game paused=g; for(int i=0;i<100;i++) game_step(&g,1.0f/120,keys); assert(memcmp(&g,&paused,sizeof(g))==0);
    game_pause(&g); assert(g.phase==PLAYING);
    game_start(&g); game_pause(&g); game_pause(&g); assert(g.phase==COUNTDOWN);
    // A mouth must capture a marble exactly once, with the matching score and owner.
    game_start(&g); g.phase=PLAYING; g.hippos[0].bite=0.16f; g.hippos[0].cooldown=0.2f;
    g.balls[0]=(Marble){0,110,0,0,1,-1};
    game_step(&g,1.0f/120,keys); assert(!g.balls[0].alive && g.balls[0].owner==0 && g.hippos[0].score>=1); valid(&g);
    // A collision at the rim must reflect the marble into the arena.
    game_init(&g,10); g.balls[0]=(Marble){210,0,180,0,1,-1}; int none[4]={0};
    game_step(&g,1.0f/120,none); assert(g.balls[0].vx<0); valid(&g);
    // End on time even if a human never presses a key.
    game_start(&g); g.phase=PLAYING; g.players=4; g.time=0.001f; game_step(&g,1.0f/120,none); assert(g.phase==FINISHED);
    // Ties name every leader. Restart resets all scores and marbles.
    memset(g.hippos,0,sizeof(g.hippos)); assert(game_winners(&g)==15); g.hippos[2].score=5; assert(game_winners(&g)==4);
    game_start(&g); valid(&g); assert(g.time==ROUND_SECONDS && g.remaining==MARBLE_COUNT);
    int total=0,fully_cleared=0;
    for(int difficulty=0;difficulty<3;difficulty++) for(int players=1;players<=4;players++) for(int seed=1;seed<=20;seed++) {
        game_init(&g,(uint32_t)seed); g.players=players; g.difficulty=difficulty; game_start(&g);
        for(int t=0;t<8000 && g.phase!=FINISHED;t++) { game_step(&g,1.0f/120,keys); valid(&g); }
        assert(g.phase==FINISHED); assert(game_winners(&g)>0); if(!g.remaining) fully_cleared++; total++;
        Game finished=g; game_step(&g,1.0f/120,keys); assert(memcmp(&g,&finished,sizeof(g))==0);
    }
    printf("PASS: %d complete simulated rounds; %d cleared every marble.\n",total,fully_cleared);
    puts("PASS: capture ownership, score conservation, finite physics, arena bounds, wall bounce, timer, pause/resume, countdown, ties, restart, finished state.");
    return 0;
}
