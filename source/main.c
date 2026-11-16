// Mini run-and-gun estilo Contra para PS3 (PSL1GHT)
// Dibuja rectángulos directamente en el framebuffer (CPU).
// Controles: D-pad izq/der = mover | X = saltar | Cuadrado = disparar
//            D-pad arriba = apuntar arriba | START = reiniciar

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <rsx/rsx.h>

#include "rsxutil.h"   // copiar rsxutil.c/.h desde los samples de PSL1GHT

#define MAX_BULLETS  16
#define MAX_ENEMIES  8
#define LEVEL_W      4000
#define GROUND_Y     460
#define GRAVITY      1
#define COL(r,g,b)   (((r)<<16)|((g)<<8)|(b))

typedef struct { int x, y, w, h, vx, vy, alive; } Obj;

static int running = 1;
static Obj player, bullets[MAX_BULLETS], enemies[MAX_ENEMIES];
static int camx = 0, lives = 3, score = 0, facing = 1, shoot_cd = 0;
static int on_ground = 0, spawn_timer = 0, invuln = 0;

static void sys_cb(u64 status, u64 param, void *usrdata) {
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

static void rect(uint32_t *fb, int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= (int)display_height) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= (int)display_width) continue;
            fb[j * display_width + i] = c;
        }
    }
}

static int hit(Obj *a, Obj *b) {
    return a->x < b->x + b->w && a->x + a->w > b->x &&
           a->y < b->y + b->h && a->y + a->h > b->y;
}

static void reset_game(void) {
    memset(&player, 0, sizeof(player));
    memset(bullets, 0, sizeof(bullets));
    memset(enemies, 0, sizeof(enemies));
    player.x = 100; player.y = GROUND_Y - 40; player.w = 24; player.h = 40;
    player.alive = 1;
    camx = 0; lives = 3; score = 0; facing = 1; invuln = 0;
}

static void fire(int dx, int dy) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].alive) {
            bullets[i] = (Obj){ player.x + player.w / 2, player.y + 14, 8, 4,
                                dx * 14, dy * 14, 1 };
            return;
        }
    }
}

static void spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive) {
            int side = (rand() % 2) ? 1 : -1;
            enemies[i] = (Obj){ player.x + side * 700, GROUND_Y - 36, 24, 36,
                                -side * (2 + rand() % 2), 0, 1 };
            return;
        }
    }
}

static void update(padData *pd, int connected) {
    int left = 0, right = 0, up = 0, jump = 0, fire_btn = 0;
    if (connected) {
        left = pd->BTN_LEFT; right = pd->BTN_RIGHT; up = pd->BTN_UP;
        jump = pd->BTN_CROSS; fire_btn = pd->BTN_SQUARE;
        if (pd->BTN_START) reset_game();
    }

    player.vx = 0;
    if (left)  { player.vx = -5; facing = -1; }
    if (right) { player.vx =  5; facing =  1; }
    if (jump && on_ground) { player.vy = -17; on_ground = 0; }

    player.x += player.vx;
    if (player.x < 0) player.x = 0;
    if (player.x > LEVEL_W - player.w) player.x = LEVEL_W - player.w;

    player.vy += GRAVITY;
    player.y += player.vy;
    if (player.y + player.h >= GROUND_Y) {
        player.y = GROUND_Y - player.h; player.vy = 0; on_ground = 1;
    }

    if (shoot_cd > 0) shoot_cd--;
    if (fire_btn && shoot_cd == 0) {
        if (up) fire(0, -1); else fire(facing, 0);
        shoot_cd = 8;
    }

    // cámara
    camx = player.x - (int)display_width / 3;
    if (camx < 0) camx = 0;
    if (camx > LEVEL_W - (int)display_width) camx = LEVEL_W - (int)display_width;

    // balas
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].alive) continue;
        bullets[i].x += bullets[i].vx; bullets[i].y += bullets[i].vy;
        if (bullets[i].x < camx - 50 || bullets[i].x > camx + (int)display_width + 50 ||
            bullets[i].y < -20) bullets[i].alive = 0;
    }

    // enemigos
    if (++spawn_timer > 70) { spawn_timer = 0; spawn_enemy(); }
    if (invuln > 0) invuln--;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive) continue;
        enemies[i].x += enemies[i].vx;
        for (int b = 0; b < MAX_BULLETS; b++) {
            if (bullets[b].alive && hit(&bullets[b], &enemies[i])) {
                bullets[b].alive = 0; enemies[i].alive = 0; score += 100;
            }
        }
        if (enemies[i].alive && !invuln && hit(&player, &enemies[i])) {
            enemies[i].alive = 0;
            if (--lives <= 0) reset_game(); else invuln = 90;
        }
    }
}

static void draw(uint32_t *fb) {
    rect(fb, 0, 0, display_width, display_height, COL(20, 20, 50));       // cielo
    rect(fb, 0, GROUND_Y, display_width, display_height - GROUND_Y, COL(40, 110, 40)); // suelo
    for (int i = 0; i < LEVEL_W; i += 300)                               // decoración
        rect(fb, i - camx, GROUND_Y - 60, 30, 60, COL(30, 80, 30));

    if (!(invuln && (invuln / 4) % 2))
        rect(fb, player.x - camx, player.y, player.w, player.h, COL(60, 120, 255));
    rect(fb, player.x - camx + (facing > 0 ? 14 : 0), player.y + 14, 14, 5, COL(220, 220, 220));

    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].alive)
            rect(fb, bullets[i].x - camx, bullets[i].y, bullets[i].w, bullets[i].h, COL(255, 240, 80));
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive)
            rect(fb, enemies[i].x - camx, enemies[i].y, enemies[i].w, enemies[i].h, COL(220, 50, 50));

    for (int i = 0; i < lives; i++)                                      // vidas
        rect(fb, 20 + i * 30, 20, 20, 20, COL(60, 120, 255));
    rect(fb, 20, 50, (score / 100 % 40) * 5, 8, COL(255, 200, 0));      // barrita de puntos
}

int main(int argc, const char *argv[]) {
    padInfo padinfo;
    padData paddata;

    void *host_addr = memalign(1024 * 1024, HOST_SIZE);
    init_screen(host_addr, HOST_SIZE);
    ioPadInit(7);
    sysUtilRegisterCallback(0, sys_cb, NULL);
    reset_game();

    int cur = 0;
    while (running) {
        sysUtilCheckCallback();
        ioPadGetInfo(&padinfo);
        int connected = 0;
        for (int i = 0; i < MAX_PADS; i++) {
            if (padinfo.status[i]) {
                ioPadGetData(i, &paddata);
                connected = 1;
                break;
            }
        }

        waitFlip();
        update(&paddata, connected);
        draw((uint32_t *)buffers[cur].ptr);
        flip(cur);
        cur = !cur;
    }

    ioPadEnd();
    return 0;
}
