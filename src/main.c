#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

#define WIDTH 1200
#define HEIGHT 820
#define SCALE 2
#define C(r,g,b) RGB(r,g,b)
static const COLORREF BG=C(18,35,43), PANEL=C(29,48,56), INK=C(242,242,218), MUTED=C(157,181,182);
static const COLORREF COLORS[4]={C(119,215,153),C(248,132,115),C(244,203,94),C(111,185,226)};
static const COLORREF DARKS[4]={C(49,135,94),C(182,74,70),C(180,133,42),C(50,116,161)};
static const char *NAMES[4]={"MINT","PEACH","SUNNY","BUBBLES"};
static const char *KEYS[4]={"SPACE","A","I","L"};
static Game game;
static HDC canvas;
static HBITMAP bitmap;
static HGDIOBJ old_bitmap;
static void *pixels;
static HFONT fonts[7];
static int down[4], muted=0, hover=-1, client_w=WIDTH, client_h=HEIGHT;
static HWND window;
static double accumulator=0;
static LARGE_INTEGER last_tick,frequency;
static unsigned char sounds[3][9000];
static int smoke=0, smoke_ticks=0;
static const float BX=420, BY=448;

static void fill(float x,float y,float w,float h,COLORREF color) {
    HBRUSH b=CreateSolidBrush(color); RECT r={(LONG)x,(LONG)y,(LONG)(x+w),(LONG)(y+h)}; FillRect(canvas,&r,b); DeleteObject(b);
}
static void oval(float x,float y,float rx,float ry,COLORREF color) {
    HBRUSH b=CreateSolidBrush(color); HGDIOBJ old=SelectObject(canvas,b),pen=SelectObject(canvas,GetStockObject(NULL_PEN));
    Ellipse(canvas,(int)(x-rx),(int)(y-ry),(int)(x+rx),(int)(y+ry)); SelectObject(canvas,old); SelectObject(canvas,pen); DeleteObject(b);
}
static void roundbox(float x,float y,float w,float h,float r,COLORREF color) {
    HBRUSH b=CreateSolidBrush(color); HGDIOBJ old=SelectObject(canvas,b),pen=SelectObject(canvas,GetStockObject(NULL_PEN));
    RoundRect(canvas,(int)x,(int)y,(int)(x+w),(int)(y+h),(int)r,(int)r); SelectObject(canvas,old); SelectObject(canvas,pen); DeleteObject(b);
}
static void label(float x,float y,float w,float h,const char *s,int font,COLORREF color,UINT align) {
    RECT r={(LONG)x,(LONG)y,(LONG)(x+w),(LONG)(y+h)}; SelectObject(canvas,fonts[font]); SetTextColor(canvas,color); SetBkMode(canvas,TRANSPARENT);
    DrawTextA(canvas,s,-1,&r,align|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
}
static void line(float x,float y,float xx,float yy,int width,COLORREF color) {
    HPEN p=CreatePen(PS_SOLID,width,color); HGDIOBJ old=SelectObject(canvas,p); MoveToEx(canvas,(int)x,(int)y,NULL); LineTo(canvas,(int)xx,(int)yy); SelectObject(canvas,old); DeleteObject(p);
}
static void local_oval(int id,float u,float v,float rx,float ry,COLORREF color) {
    float ox=BX+HIPPO_X[id]*257,oy=BY+HIPPO_Y[id]*257;
    float tx=HIPPO_Y[id],ty=-HIPPO_X[id],fx=-HIPPO_X[id],fy=-HIPPO_Y[id];
    POINT pts[48];
    for(int n=0;n<48;n++) { float a=2*PI*(float)n/48; float px=u+cosf(a)*rx,py=v+sinf(a)*ry;
        pts[n].x=(LONG)(ox+tx*px+fx*py); pts[n].y=(LONG)(oy+ty*px+fy*py); }
    HBRUSH b=CreateSolidBrush(color); HGDIOBJ old=SelectObject(canvas,b),p=SelectObject(canvas,GetStockObject(NULL_PEN));
    Polygon(canvas,pts,48); SelectObject(canvas,old); SelectObject(canvas,p); DeleteObject(b);
}
static void hippo(int id) {
    Hippo *h=&game.hippos[id]; float reach=game_reach(h);
    COLORREF body=COLORS[id],dark=DARKS[id];
    local_oval(id,0,-2,54,55,C(27,70,70));
    local_oval(id,-37,-19,20,25,dark); local_oval(id,37,-19,20,25,dark);
    local_oval(id,0,4,49,51,dark); local_oval(id,0,9,46,48,body);
    local_oval(id,0,37+reach*0.45f,31,36+reach*0.5f,dark);
    local_oval(id,0,41+reach*0.45f,28,34+reach*0.5f,body);
    local_oval(id,-29,40+reach,15,16,dark); local_oval(id,29,40+reach,15,16,dark);
    local_oval(id,-29,43+reach,10,10,body); local_oval(id,29,43+reach,10,10,body);
    local_oval(id,0,66+reach,42,37,dark);
    local_oval(id,0,68+reach,36,29,C(45,59,60));
    if(reach>15) {
        local_oval(id,0,66+reach,24,17,C(193,99,100));
        local_oval(id,-20,85+reach,7,9,INK); local_oval(id,20,85+reach,7,9,INK);
    }
    local_oval(id,0,51+reach,40,29,body);
    local_oval(id,-15,64+reach,5,4,dark); local_oval(id,15,64+reach,5,4,dark);
    local_oval(id,-20,35+reach,12,14,INK); local_oval(id,20,35+reach,12,14,INK);
    local_oval(id,-20,40+reach,5,7,C(29,49,49)); local_oval(id,20,40+reach,5,7,C(29,49,49));
    local_oval(id,-22,42+reach,2,2,C(255,255,255)); local_oval(id,18,42+reach,2,2,C(255,255,255));
    if(h->flash>0) {
        float px=BX+HIPPO_X[id]*155,py=BY+HIPPO_Y[id]*155-18*(1-h->flash/0.28f);
        label(px-24,py-22,48,35,"+1",3,INK,DT_CENTER);
    }
}
static void button(int id,int x,int y,int w,int h,const char *text,int active) {
    COLORREF color=active?COLORS[0]:hover==id?C(56,82,87):C(43,65,72);
    roundbox((float)x,(float)y,(float)w,(float)h,16,color);
    label((float)x,(float)y,(float)w,(float)h,text,2,active?BG:INK,DT_CENTER);
}
static int hit(int x,int y) {
    if(x>=858 && x<1122 && y>=682 && y<738) return 10;
    if(x>=858 && x<1122 && y>=598 && y<638) return 11;
    if(x>=858 && x<1122 && y>=214 && y<250) return (x-858)/68;
    if(x>=1075 && x<1150 && y>=60 && y<96) return 12;
    if(x>=858 && x<1122 && y>=644 && y<673) return 13;
    return -1;
}
static void board(void) {
    oval(BX,BY+18,265,265,C(10,25,31));
    oval(BX,BY,264,264,C(54,88,86));
    oval(BX,BY-3,253,253,C(239,231,192));
    oval(BX,BY-3,238,238,C(201,209,175));
    oval(BX,BY,227,227,C(100,170,162));
    oval(BX,BY+4,218,218,C(161,208,185));
    oval(BX,BY+4,177,177,C(168,212,190));
    oval(BX,BY+4,115,115,C(177,217,195));
    for(int i=0;i<36;i++) {
        float a=(float)i*2*PI/36; oval(BX+cosf(a)*244,BY-3+sinf(a)*244,2,2,C(125,145,128));
    }
    label(BX-130,BY-35,260,28,"THE MARBLE CLUB",2,C(103,158,143),DT_CENTER);
    label(BX-100,BY-6,200,22,"CHOMP. SCOOP. REPEAT.",0,C(113,166,150),DT_CENTER);
    for(int j=0;j<MARBLE_COUNT;j++) if(game.balls[j].alive) {
        Marble *b=&game.balls[j]; float x=BX+b->x,y=BY+b->y;
        oval(x+2,y+4,10,9,C(107,159,145));
        oval(x,y,9,9,C(209,214,206)); oval(x-1,y-2,8,7,C(250,249,231)); oval(x-3,y-4,3,2,C(255,255,255));
    }
    for(int i=2;i>=0;i--) hippo(i); hippo(3);
    const int lx[4]={359,67,359,656},ly[4]={751,437,118,437};
    for(int i=0;i<4;i++) {
        roundbox((float)lx[i],(float)ly[i],122,29,14,PANEL);
        char text[40]; snprintf(text,sizeof(text),"%s  /  %s",NAMES[i],i<game.players?KEYS[i]:"CPU");
        label((float)lx[i],(float)ly[i],122,29,text,0,COLORS[i],DT_CENTER);
    }
}
static void overlay(const char *big,const char *small) {
    roundbox(BX-158,BY-65,316,130,28,BG);
    label(BX-152,BY-58,304,64,big,5,INK,DT_CENTER);
    label(BX-149,BY+9,298,42,small,1,MUTED,DT_CENTER);
}
static void render(void) {
    fill(0,0,WIDTH,HEIGHT,BG);
    for(int x=32;x<820;x+=32) for(int y=126;y<795;y+=32) oval((float)x,(float)y,1,1,C(34,53,59));
    roundbox(48,44,42,42,15,COLORS[0]);
    oval(62,62,6,8,INK); oval(76,62,6,8,INK); oval(63,64,2,3,BG); oval(75,64,2,3,BG);
    label(106,37,560,54,"HUNGRY HIPPOS",6,INK,DT_LEFT);
    label(109,87,600,24,"A LITTLE TABLETOP CHAOS. A LOT OF MARBLES.",0,MUTED,DT_LEFT);
    label(842,45,220,27,"THE MARBLE CLUB",2,INK,DT_LEFT);
    label(842,74,220,23,"LOCAL PLAY  /  1 TO 4 PLAYERS",0,MUTED,DT_LEFT);
    button(12,1075,60,75,36,muted?"SFX OFF":"SFX ON",0);
    board();
    roundbox(835,142,310,620,28,PANEL);
    label(858,158,264,30,"MAKE ROOM AT THE TABLE",0,MUTED,DT_LEFT);
    for(int i=0;i<4;i++) { char n[2]={(char)('1'+i),0}; button(i,858+68*i,214,60,36,n,game.players==i+1); }
    label(858,185,264,25,"Human players",2,INK,DT_LEFT);
    int config=game.phase==LOBBY || game.phase==FINISHED;
    label(858,256,270,25,config?"Everyone else is a computer hippo.":"Finish this round to change players.",0,MUTED,DT_LEFT);
    line(858,294,1122,294,1,C(64,83,87));
    char buf[80];
    const char *status=game.phase==LOBBY?"READY TO FEAST":game.phase==FINISHED?"FINAL SCORES":game.phase==PAUSED?"TAKE A BREATHER":"THE MARBLE RACE";
    label(858,308,264,24,status,0,MUTED,DT_LEFT);
    for(int i=0;i<4;i++) {
        float y=344+(float)i*48;
        oval(870,y+16,6,6,COLORS[i]);
        label(888,y,140,21,NAMES[i],2,COLORS[i],DT_LEFT);
        snprintf(buf,sizeof(buf),"%s",i<game.players?KEYS[i]:"COMPUTER");
        label(888,y+20,140,17,buf,0,MUTED,DT_LEFT);
        snprintf(buf,sizeof(buf),"%02d",game.hippos[i].score);
        label(1058,y-1,64,39,buf,3,INK,DT_RIGHT);
    }
    line(858,545,1122,545,1,C(64,83,87));
    snprintf(buf,sizeof(buf),"%02d MARBLES LEFT",game.remaining); label(858,555,170,30,buf,0,MUTED,DT_LEFT);
    snprintf(buf,sizeof(buf),"0:%02d",(int)ceilf(game.time));
    if(game.time>=60) strcpy(buf,"1:00");
    label(1035,548,87,39,buf,3,INK,DT_RIGHT);
    const char *levels[]={"CPU PACE: GENTLE","CPU PACE: LIVELY","CPU PACE: QUICK"};
    button(11,858,598,264,40,levels[game.difficulty],0);
    label(858,644,264,29,(game.phase==PLAYING || game.phase==PAUSED || game.phase==COUNTDOWN)?"R: restart    M: sound":"Hold your key to keep chomping",0,MUTED,DT_CENTER);
    button(10,858,682,264,56,game.phase==LOBBY?"LET'S PLAY":game.phase==FINISHED?"PLAY AGAIN":game.phase==PAUSED?"KEEP CHOMPING":"PAUSE ROUND",1);
    label(836,777,310,24,"ENTER: PLAY   P: PAUSE   ESC: LOBBY",0,MUTED,DT_CENTER);
    label(48,786,720,20,"28 marbles. Four hungry friends. Most marbles wins.",0,MUTED,DT_LEFT);
    if(game.phase==COUNTDOWN) { snprintf(buf,sizeof(buf),"%d",(int)ceilf(game.countdown)); overlay(buf,"Fingers ready. Hippos hungry."); }
    if(game.phase==PAUSED) overlay("SNACK BREAK","Press P or click to keep chomping.");
    if(game.phase==FINISHED) {
        int mask=game_winners(&game); int tied=(mask&(mask-1))!=0;
        if(tied) strcpy(buf,"A TASTY TIE!");
        else { int i=0; while(!(mask&(1<<i))) i++; snprintf(buf,sizeof(buf),"%s WINS!",NAMES[i]); }
        // A smaller font keeps the longest winner name inside its panel.
        roundbox(BX-172,BY-65,344,130,28,BG);
        label(BX-166,BY-52,332,57,buf,4,INK,DT_CENTER);
        label(BX-164,BY+12,328,33,"Another helping? Press Enter.",1,MUTED,DT_CENTER);
    }
}
static void init_canvas(void) {
    canvas=CreateCompatibleDC(NULL);
    BITMAPINFO bi; memset(&bi,0,sizeof(bi)); bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=WIDTH*SCALE; bi.bmiHeader.biHeight=-HEIGHT*SCALE; bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(canvas,&bi,DIB_RGB_COLORS,&pixels,NULL,0); old_bitmap=SelectObject(canvas,bitmap);
    SetGraphicsMode(canvas,GM_ADVANCED); XFORM xf={SCALE,0,0,SCALE,0,0}; SetWorldTransform(canvas,&xf);
    const int sizes[]={12,15,16,29,32,52,42};
    for(int i=0;i<7;i++) fonts[i]=CreateFontA(-sizes[i],0,0,0,i==1?FW_NORMAL:FW_BOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
}
static void destroy_canvas(void) {
    SelectObject(canvas,GetStockObject(SYSTEM_FONT));
    for(int i=0;i<7;i++) DeleteObject(fonts[i]);
    SelectObject(canvas,old_bitmap); DeleteObject(bitmap); DeleteDC(canvas);
}
static int snapshot(const char *path) {
    render(); FILE *f=fopen(path,"wb"); if(!f) return 0;
    BITMAPFILEHEADER fh; BITMAPINFOHEADER ih; memset(&fh,0,sizeof(fh)); memset(&ih,0,sizeof(ih));
    fh.bfType=0x4d42; fh.bfOffBits=sizeof(fh)+sizeof(ih); fh.bfSize=fh.bfOffBits+WIDTH*SCALE*HEIGHT*SCALE*4;
    ih.biSize=sizeof(ih); ih.biWidth=WIDTH*SCALE; ih.biHeight=-HEIGHT*SCALE; ih.biPlanes=1; ih.biBitCount=32; ih.biSizeImage=WIDTH*SCALE*HEIGHT*SCALE*4;
    int ok=fwrite(&fh,sizeof(fh),1,f)==1 && fwrite(&ih,sizeof(ih),1,f)==1 && fwrite(pixels,ih.biSizeImage,1,f)==1;
    if(fclose(f)!=0) ok=0; return ok;
}
static void put16(unsigned char *p,unsigned int n) { p[0]=(unsigned char)n; p[1]=(unsigned char)(n>>8); }
static void put32(unsigned char *p,unsigned int n) { for(int i=0;i<4;i++) p[i]=(unsigned char)(n>>(8*i)); }
static void init_sounds(void) {
    for(int k=0;k<3;k++) {
        unsigned char *s=sounds[k]; int count=4000;
        memcpy(s,"RIFF",4); put32(s+4,36+count*2); memcpy(s+8,"WAVEfmt ",8); put32(s+16,16); put16(s+20,1); put16(s+22,1);
        put32(s+24,22050); put32(s+28,44100); put16(s+32,2); put16(s+34,16); memcpy(s+36,"data",4); put32(s+40,count*2);
        for(int i=0;i<count;i++) { float t=(float)i/22050,env=1-(float)i/(float)count; float f=k==0?600-1800*t:k==1?700:523+(float)(i/1300)*130;
            short sample=(short)(sinf(2*PI*f*t)*env*env*5500); put16(s+44+i*2,(unsigned short)sample); }
    }
}
static void action(int id) {
    if(id>=0 && id<4 && (game.phase==LOBBY || game.phase==FINISHED)) game.players=id+1;
    if(id==10) { if(game.phase==LOBBY || game.phase==FINISHED) game_start(&game); else game_pause(&game); accumulator=0; }
    if(id==11 && (game.phase==LOBBY || game.phase==FINISHED)) game.difficulty=(game.difficulty+1)%3;
    if(id==12) muted=!muted;
}
static void mouse_point(LPARAM lp,int *x,int *y) {
    float s=fminf((float)client_w/WIDTH,(float)client_h/HEIGHT);
    if(s<=0) { *x=*y=-1; return; }
    *x=(int)(((float)GET_X_LPARAM(lp)-((float)client_w-WIDTH*s)*0.5f)/s);
    *y=(int)(((float)GET_Y_LPARAM(lp)-((float)client_h-HEIGHT*s)*0.5f)/s);
}
static int key_index(WPARAM key) { return key==VK_SPACE?0:key=='A'?1:key=='I'?2:key=='L'?3:-1; }
static LRESULT CALLBACK wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: client_w=LOWORD(lp); client_h=HIWORD(lp); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)lp)->ptMinTrackSize.x=850; ((MINMAXINFO*)lp)->ptMinTrackSize.y=610; return 0;
    case WM_KEYDOWN: {
        int k=key_index(wp); if(k>=0) down[k]=1;
        if(lp&(1L<<30)) return 0;
        if(wp==VK_RETURN) { if(game.phase==LOBBY || game.phase==FINISHED || game.phase==PAUSED) action(10); }
        if(wp=='P') game_pause(&game);
        if(wp=='R' && game.phase!=LOBBY) game_start(&game);
        if(wp=='M') action(12);
        if(wp>='1' && wp<='4') action((int)(wp-'1'));
        if(wp=='D') action(11);
        if(wp==VK_ESCAPE) {
            int players=game.players,difficulty=game.difficulty; uint32_t seed=game.rng;
            game_init(&game,seed); game.players=players; game.difficulty=difficulty;
            memset(down,0,sizeof(down)); accumulator=0;
        }
        return 0;
    }
    case WM_KEYUP: { int k=key_index(wp); if(k>=0) down[k]=0; return 0; }
    case WM_KILLFOCUS: memset(down,0,sizeof(down)); if(game.phase==PLAYING || game.phase==COUNTDOWN) game_pause(&game); return 0;
    case WM_MOUSEMOVE: { int x,y; mouse_point(lp,&x,&y); hover=hit(x,y); SetCursor(LoadCursor(NULL,hover>=0?IDC_HAND:IDC_ARROW)); return 0; }
    case WM_LBUTTONDOWN: { int x,y; mouse_point(lp,&x,&y); action(hit(x,y)); SetFocus(hwnd); return 0; }
    case WM_TIMER: {
        LARGE_INTEGER now; QueryPerformanceCounter(&now); double dt=(double)(now.QuadPart-last_tick.QuadPart)/(double)frequency.QuadPart; last_tick=now;
        if(dt>0.1) dt=0.1; accumulator+=dt;
        while(accumulator>=1.0/120.0) { game_step(&game,1.0f/120.0f,down); accumulator-=1.0/120.0; }
        if(game.sound_events && !muted) { int k=(game.sound_events&4)?2:(game.sound_events&2)?1:0; PlaySoundA((LPCSTR)sounds[k],NULL,SND_MEMORY|SND_ASYNC|SND_NODEFAULT); }
        game.sound_events=0;
        InvalidateRect(hwnd,NULL,FALSE);
        if(smoke && ++smoke_ticks==12) {
            int ok=game.phase==COUNTDOWN;
            SendMessage(hwnd,WM_KEYDOWN,'P',0); ok&=game.phase==PAUSED;
            SendMessage(hwnd,WM_KEYDOWN,VK_RETURN,0); ok&=game.phase==COUNTDOWN;
            SendMessage(hwnd,WM_KEYDOWN,VK_SPACE,0); ok&=down[0]==1;
            SendMessage(hwnd,WM_KEYUP,VK_SPACE,0); ok&=down[0]==0;
            SendMessage(hwnd,WM_KILLFOCUS,0,0); ok&=game.phase==PAUSED;
            SendMessage(hwnd,WM_KEYDOWN,VK_ESCAPE,0); ok&=game.phase==LOBBY;
            SendMessage(hwnd,WM_KEYDOWN,'4',0); ok&=game.players==4;
            SendMessage(hwnd,WM_KEYDOWN,'D',0); ok&=game.difficulty==2;
            SendMessage(hwnd,WM_KEYDOWN,'M',0); ok&=muted;
            SendMessage(hwnd,WM_LBUTTONDOWN,0,MAKELPARAM(880,230)); ok&=game.players==1;
            SendMessage(hwnd,WM_LBUTTONDOWN,0,MAKELPARAM(900,710)); ok&=game.phase==COUNTDOWN;
            SendMessage(hwnd,WM_KEYDOWN,'R',0); ok&=game.countdown==3.0f && game.remaining==MARBLE_COUNT;
            DWORD handles_before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
            for(int frame=0;frame<60;frame++) render();
            DWORD handles_after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS); ok&=handles_before==handles_after;
            FILE *f=fopen("smoke-result.txt","w"); if(f) { fprintf(f,"%s: window, timer, paint, start, pause, resume, key down/up, focus loss, lobby, player selection, difficulty, sound toggle, mouse buttons, restart, stable GDI resource count across 60 renders\n",ok?"PASS":"FAIL"); fclose(f); }
            DestroyWindow(hwnd); if(!ok) PostQuitMessage(1);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC target=BeginPaint(hwnd,&ps); render();
        RECT r; GetClientRect(hwnd,&r); HBRUSH b=CreateSolidBrush(BG); FillRect(target,&r,b); DeleteObject(b);
        float s=fminf((float)client_w/WIDTH,(float)client_h/HEIGHT); int w=(int)(WIDTH*s),h=(int)(HEIGHT*s);
        SetStretchBltMode(target,HALFTONE); SetBrushOrgEx(target,0,0,NULL);
        XFORM identity={1,0,0,1,0,0}; SetWorldTransform(canvas,&identity);
        StretchBlt(target,(client_w-w)/2,(client_h-h)/2,w,h,canvas,0,0,WIDTH*SCALE,HEIGHT*SCALE,SRCCOPY);
        XFORM xf={SCALE,0,0,SCALE,0,0}; SetWorldTransform(canvas,&xf); EndPaint(hwnd,&ps); return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: KillTimer(hwnd,1); PostQuitMessage(0); return 0;
    default: return DefWindowProc(hwnd,msg,wp,lp);
    }
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR cmd,int show) {
    (void)previous; (void)cmd; SetProcessDPIAware(); game_init(&game,(uint32_t)GetTickCount()); init_canvas(); init_sounds();
    for(int i=1;i<__argc;i++) {
        if(strcmp(__argv[i],"--snapshot")==0 && i+1<__argc) {
            const char *path=__argv[++i];
            if(i+1<__argc && strcmp(__argv[i+1],"playing")==0) {
                game_start(&game); game.countdown=0; game.phase=PLAYING;
                int held[4]={1,0,0,0}; for(int t=0;t<180;t++) game_step(&game,1.0f/120.0f,held);
            }
            int ok=snapshot(path); destroy_canvas(); return ok?0:1;
        }
        if(strcmp(__argv[i],"--smoke-test")==0) smoke=1;
    }
    WNDCLASSA wc; memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wndproc; wc.hInstance=instance; wc.lpszClassName="HungryHipposTable";
    wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hIcon=LoadIcon(instance,MAKEINTRESOURCE(1)); if(!wc.hIcon) wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);
    if(!RegisterClassA(&wc)) { destroy_canvas(); return 1; }
    RECT rect={0,0,WIDTH,HEIGHT}; AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    int ww=rect.right-rect.left,wh=rect.bottom-rect.top;
    if(wh>GetSystemMetrics(SM_CYSCREEN)-80) { wh=GetSystemMetrics(SM_CYSCREEN)-80; ww=(int)((float)wh*WIDTH/HEIGHT); }
    window=CreateWindowA(wc.lpszClassName,"Hungry Hippos | The Marble Club",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,ww,wh,NULL,NULL,instance,NULL);
    if(!window) { destroy_canvas(); return 1; }
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&last_tick);
    SetTimer(window,1,16,NULL); ShowWindow(window,smoke?SW_HIDE:show); UpdateWindow(window);
    if(smoke) { SendMessage(window,WM_KEYDOWN,VK_RETURN,0); InvalidateRect(window,NULL,FALSE); SendMessage(window,WM_PAINT,0,0); }
    MSG msg; int result;
    while((result=GetMessage(&msg,NULL,0,0))>0) { TranslateMessage(&msg); DispatchMessage(&msg); }
    PlaySoundA(NULL,NULL,0); destroy_canvas(); return result<0?1:(int)msg.wParam;
}
