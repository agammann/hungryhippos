#include "game.h"
#include <math.h>
#include <string.h>
const float HIPPO_X[4] = {0,-1,0,1};
const float HIPPO_Y[4] = {1,0,-1,0};
static float random01(Game *g) {
    uint32_t x=g->rng; x^=x<<13; x^=x>>17; x^=x<<5; g->rng=x;
    return (float)(x & 0xffffff)/16777216.0f;
}
static void seed_balls(Game *g) {
    for(int i=0;i<MARBLE_COUNT;i++) {
        float a=(float)i*2.39996323f, r=24.0f*sqrtf((float)i+1.0f);
        float v=105.0f+random01(g)*95.0f, d=random01(g)*2*PI;
        g->balls[i]=(Marble){cosf(a)*r,sinf(a)*r,cosf(d)*v,sinf(d)*v,1,-1};
    }
    g->remaining=MARBLE_COUNT;
}
void game_init(Game *g, uint32_t seed) {
    memset(g,0,sizeof(*g)); g->rng=seed?seed:12345; g->players=1;
    g->difficulty=1; g->phase=LOBBY; g->time=ROUND_SECONDS; seed_balls(g);
}
void game_start(Game *g) {
    memset(g->hippos,0,sizeof(g->hippos)); seed_balls(g);
    g->time=ROUND_SECONDS; g->countdown=3.0f; g->phase=COUNTDOWN; g->sound_events=0;
    for(int i=0;i<4;i++) g->hippos[i].think=random01(g)*0.5f;
}
void game_pause(Game *g) {
    if(g->phase==PAUSED) g->phase=g->before_pause;
    else if(g->phase==PLAYING || g->phase==COUNTDOWN) { g->before_pause=g->phase; g->phase=PAUSED; }
}
float game_reach(const Hippo *h) {
    if(h->bite<=0) return 0;
    float t=1.0f-h->bite/0.30f;
    return sinf(t*PI)*104.0f;
}
int game_winners(const Game *g) {
    int best=-1,mask=0;
    for(int i=0;i<4;i++) {
        if(g->hippos[i].score>best) { best=g->hippos[i].score; mask=1<<i; }
        else if(g->hippos[i].score==best) mask|=1<<i;
    }
    return mask;
}
void game_step(Game *g, float dt, const int held[4]) {
    if(dt<=0 || dt>0.05f || !isfinite(dt)) return;
    if(g->phase==PAUSED || g->phase==FINISHED) return;
    g->clock+=dt;
    if(g->phase==COUNTDOWN) {
        g->countdown-=dt;
        if(g->countdown<=0) { g->phase=PLAYING; g->countdown=0; g->sound_events|=2; }
        return;
    }
    int playing=g->phase==PLAYING;
    if(playing) g->time=fmaxf(0,g->time-dt);
    for(int i=0;i<4;i++) {
        Hippo *h=&g->hippos[i];
        h->bite=fmaxf(0,h->bite-dt); h->cooldown=fmaxf(0,h->cooldown-dt); h->flash=fmaxf(0,h->flash-dt);
        int trigger=held[i];
        if(i>=g->players) {
            h->think-=dt; trigger=0;
            if(h->think<=0) {
                h->think=(g->difficulty==0?0.36f:g->difficulty==1?0.20f:0.10f)+random01(g)*0.18f;
                for(int j=0;j<MARBLE_COUNT;j++) if(g->balls[j].alive) {
                    Marble *b=&g->balls[j];
                    float ahead=b->x*HIPPO_X[i]+b->y*HIPPO_Y[i];
                    float side=b->x*HIPPO_Y[i]-b->y*HIPPO_X[i];
                    if(ahead>80 && fabsf(side)<65 && random01(g)>(g->difficulty==0?0.45f:0.12f)) trigger=1;
                }
            }
        }
        if(playing && trigger && h->cooldown<=0) { h->bite=0.30f; h->cooldown=0.42f; }
    }
    for(int j=0;j<MARBLE_COUNT;j++) {
        Marble *b=&g->balls[j]; if(!b->alive) continue;
        // A gently changing force prevents marbles from settling into repeating paths.
        b->vx+=cosf(g->clock*0.9f+(float)j)*8*dt;
        b->vy+=sinf(g->clock*0.7f+(float)j)*8*dt;
        float speed=sqrtf(b->vx*b->vx+b->vy*b->vy);
        if(speed<95) { b->vx+=cosf((float)j*2.4f)*60*dt; b->vy+=sinf((float)j*2.4f)*60*dt; }
        if(speed>260) { b->vx*=260/speed; b->vy*=260/speed; }
        b->x+=b->vx*dt; b->y+=b->vy*dt;
        float r=sqrtf(b->x*b->x+b->y*b->y);
        if(r>211) {
            float nx=b->x/r,ny=b->y/r,dot=b->vx*nx+b->vy*ny;
            b->x=nx*211; b->y=ny*211;
            if(dot>0) { b->vx-=2*dot*nx; b->vy-=2*dot*ny; }
        }
        if(playing) for(int i=0;i<4;i++) {
            Hippo *h=&g->hippos[i]; float reach=game_reach(h);
            float ahead=b->x*HIPPO_X[i]+b->y*HIPPO_Y[i];
            float side=b->x*HIPPO_Y[i]-b->y*HIPPO_X[i];
            // The mouth sweeps inward, then scoops marbles on its return stroke.
            if(h->bite>0.018f && h->bite<0.235f && ahead>207-reach-17 && ahead<237-reach && fabsf(side)<35) {
                b->alive=0; b->owner=i; h->score++; h->flash=0.28f; g->remaining--; g->sound_events|=1; break;
            }
        }
    }
    for(int i=0;i<MARBLE_COUNT;i++) for(int j=i+1;j<MARBLE_COUNT;j++) {
        Marble *a=&g->balls[i],*b=&g->balls[j]; if(!a->alive || !b->alive) continue;
        float dx=b->x-a->x,dy=b->y-a->y,d2=dx*dx+dy*dy;
        if(d2<18*18 && d2>0.0001f) {
            float d=sqrtf(d2), nx=dx/d,ny=dy/d,overlap=(18-d)*0.5f;
            a->x-=nx*overlap; a->y-=ny*overlap; b->x+=nx*overlap; b->y+=ny*overlap;
            float v=(b->vx-a->vx)*nx+(b->vy-a->vy)*ny;
            if(v<0) { a->vx+=v*nx; a->vy+=v*ny; b->vx-=v*nx; b->vy-=v*ny; }
        }
    }
    if(playing && (g->remaining==0 || g->time<=0)) { g->phase=FINISHED; g->sound_events|=4; }
}
