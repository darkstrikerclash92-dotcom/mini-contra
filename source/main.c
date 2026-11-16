
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>

#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <rsx/rsx.h>

#include "rsxutil.h"

#define MAX_BUFFERS 2
#define MAX_BULLETS 16
#define MAX_ENEMIES 8
#define LEVEL_W 4000
#define GROUND_Y 460
#define GRAVITY 1

#define COL(r,g,b) (((r)<<16)|((g)<<8)|(b))

typedef struct {
    int x, y, w, h, vx, vy, alive;
} Obj;

static int running = 1;

static Obj player;
static Obj bullets[MAX_BULLETS];
static Obj enemies[MAX_ENEMIES];

static rsxBuffer buffers[MAX_BUFFERS];
static gcmContextData *context = NULL;

static u16 display_width = 0;
static u16 display_height = 0;

static int camx = 0;
static int lives = 3;
static int score = 0;
static int facing = 1;
static int shoot_cd = 0;
static int on_ground = 0;
static int spawn_timer = 0;
static int invuln = 0;

static void sys_cb(u64 status, u64 param, void *usrdata)
{
    if (status == SYSUTIL_EXIT_GAME)
        running = 0;
}

static void rect(uint32_t *fb, int x, int y,
                 int w, int h, uint32_t color)
{
    int j, i;

    for (j = y; j < y + h; j++) {
        if (j < 0 || j >= display_height)
            continue;

        for (i = x; i < x + w; i++) {
            if (i < 0 || i >= display_width)
                continue;

            fb[j * display_width + i] = color;
        }
    }
}

static int hit(Obj *a, Obj *b)
{
    return a->x < b->x + b->w &&
           a->x + a->w > b->x &&
           a->y < b->y + b->h &&
           a->y + a->h > b->y;
}

static void reset_game(void)
{
    memset(&player, 0, sizeof(player));
    memset(bullets, 0, sizeof(bullets));
    memset(enemies, 0, sizeof(enemies));

    player.x = 100;
    player.y = GROUND_Y - 40;
    player.w = 24;
    player.h = 40;
    player.alive = 1;

    camx = 0;
    lives = 3;
    score = 0;
    facing = 1;
    invuln = 0;
    shoot_cd = 0;
    spawn_timer = 0;
    on_ground = 1;
}

static void fire(int dx, int dy)
{
    int i;

    for (i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].alive) {
            bullets[i] = (Obj) {
                player.x + player.w / 2,
                player.y + 14,
                8, 4,
                dx * 14,
                dy * 14,
                1
            };

            return;
        }
    }
}

static void spawn_enemy(void)
{
    int i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive) {
            int side = (rand() % 2) ? 1 : -1;

            enemies[i] = (Obj) {
                player.x + side * 700,
                GROUND_Y - 36,
                24, 36,
                -side * (2 + rand() % 2),
                0,
                1
            };

            return;
        }
    }
}

static void update(padData *pd, int connected)
{
    int left = 0;
    int right = 0;
    int up = 0;
    int jump = 0;
    int fire_btn = 0;

    if (connected) {
        left = pd->BTN_LEFT;
        right = pd->BTN_RIGHT;
        up = pd->BTN_UP;
        jump = pd->BTN_CROSS;
        fire_btn = pd->BTN_SQUARE;

        if (pd->BTN_START)
            reset_game();
    }

    player.vx = 0;

    if (left) {
        player.vx = -5;
        facing = -1;
    }

    if (right) {
        player.vx = 5;
        facing = 1;
    }

    if (jump && on_ground) {
        player.vy = -17;
        on_ground = 0;
    }

    player.x += player.vx;

    if (player.x < 0)
        player.x = 0;

    if (player.x > LEVEL_W - player.w)
        player.x = LEVEL_W - player.w;

    player.vy += GRAVITY;
    player.y += player.vy;

    if (player.y + player.h >= GROUND_Y) {
        player.y = GROUND_Y - player.h;
        player.vy = 0;
        on_ground = 1;
    }

    if (shoot_cd > 0)
        shoot_cd--;

    if (fire_btn && shoot_cd == 0) {
        if (up)
            fire(0, -1);
        else
            fire(facing, 0);

        shoot_cd = 8;
    }

    camx = player.x - (int)display_width / 3;

    if (camx < 0)
        camx = 0;

    if (camx > LEVEL_W - (int)display_width)
        camx = LEVEL_W - (int)display_width;

    int i, b;

    for (i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].alive)
            continue;

        bullets[i].x += bullets[i].vx;
        bullets[i].y += bullets[i].vy;

        if (bullets[i].x < camx - 50 ||
            bullets[i].x > camx + display_width + 50 ||
            bullets[i].y < -20) {
            bullets[i].alive = 0;
        }
    }

    if (++spawn_timer > 70) {
        spawn_timer = 0;
        spawn_enemy();
    }

    if (invuln > 0)
        invuln--;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive)
            continue;

        enemies[i].x += enemies[i].vx;

        for (b = 0; b < MAX_BULLETS; b++) {
            if (bullets[b].alive &&
                hit(&bullets[b], &enemies[i])) {

                bullets[b].alive = 0;
                enemies[i].alive = 0;
                score += 100;
                break;
            }
        }

        if (enemies[i].alive &&
            !invuln &&
            hit(&player, &enemies[i])) {

            enemies[i].alive = 0;

            if (--lives <= 0)
                reset_game();
            else
                invuln = 90;
        }
    }
}

static void draw(uint32_t *fb)
{
    int i;

    rect(fb, 0, 0,
         display_width, display_height,
         COL(20, 20, 50));

    rect(fb, 0, GROUND_Y,
         display_width,
         display_height - GROUND_Y,
         COL(40, 110, 40));

    for (i = 0; i < LEVEL_W; i += 300) {
        rect(fb, i - camx,
             GROUND_Y - 60,
             30, 60,
             COL(30, 80, 30));
    }

    if (!(invuln && (invuln / 4) % 2)) {
        rect(fb,
             player.x - camx,
             player.y,
             player.w,
             player.h,
             COL(60, 120, 255));
    }

    rect(fb,
         player.x - camx + (facing > 0 ? 14 : 0),
         player.y + 14,
         14, 5,
         COL(220, 220, 220));

    for (i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].alive) {
            rect(fb,
                 bullets[i].x - camx,
                 bullets[i].y,
                 bullets[i].w,
                 bullets[i].h,
                 COL(255, 240, 80));
        }
    }

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) {
            rect(fb,
                 enemies[i].x - camx,
                 enemies[i].y,
                 enemies[i].w,
                 enemies[i].h,
                 COL(220, 50, 50));
        }
    }

    for (i = 0; i < lives; i++) {
        rect(fb,
             20 + i * 30,
             20,
             20, 20,
             COL(60, 120, 255));
    }

    rect(fb, 20, 50,
         (score / 100 % 40) * 5,
         8,
         COL(255, 200, 0));
}

int main(int argc, const char *argv[])
{
    padInfo padinfo;
    padData paddata;

    void *host_addr = NULL;
    int currentBuffer = 0;
    int i;
    int connected;

    memset(&padinfo, 0, sizeof(padinfo));
    memset(&paddata, 0, sizeof(paddata));
    memset(buffers, 0, sizeof(buffers));

    host_addr = memalign(1024 * 1024, HOST_SIZE);

    if (!host_addr)
        return 1;

    context = initScreen(host_addr, HOST_SIZE);

    /*
       IMPORTANTE:
       initScreen ya libera host_addr si falla.
       Por eso NO debemos volver a liberarlo aquí.
    */
    if (!context)
        return 1;

    if (!getResolution(&display_width, &display_height)) {
        rsxFinish(context, 1);
        free(host_addr);
        return 1;
    }

    for (i = 0; i < MAX_BUFFERS; i++) {
        if (!makeBuffer(&buffers[i],
                        display_width,
                        display_height,
                        i)) {

            int j;

            for (j = 0; j < i; j++)
                rsxFree(buffers[j].ptr);

            rsxFinish(context, 1);
            free(host_addr);
            return 1;
        }
    }

    ioPadInit(7);

    sysUtilRegisterCallback(0, sys_cb, NULL);

    reset_game();

    flip(context, MAX_BUFFERS - 1);

    while (running) {

        sysUtilCheckCallback();

        ioPadGetInfo(&padinfo);

        connected = 0;

        for (i = 0; i < MAX_PADS; i++) {
            if (padinfo.status[i]) {
                ioPadGetData(i, &paddata);
                connected = 1;
                break;
            }
        }

        setRenderTarget(context, &buffers[currentBuffer]);

        update(&paddata, connected);

        waitFlip();

        draw(buffers[currentBuffer].ptr);

        flip(context, buffers[currentBuffer].id);

        currentBuffer++;

        if (currentBuffer >= MAX_BUFFERS)
            currentBuffer = 0;
    }

    gcmSetWaitFlip(context);

    for (i = 0; i < MAX_BUFFERS; i++)
        rsxFree(buffers[i].ptr);

    ioPadEnd();

    rsxFinish(context, 1);

    free(host_addr);

    return 0;
}
