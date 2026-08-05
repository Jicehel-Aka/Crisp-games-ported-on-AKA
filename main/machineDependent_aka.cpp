// machineDependent_aka.cpp — Implementation AKA (ESP32-S3, gb_graphics) de
// l'interface machineDependent.h attendue par crisp-game-lib-portable.
// Bibliotheque et jeux (abagames/crisp-game-lib-portable, MIT) inchanges --
// seule cette couche est specifique au portage AKA.
//
// Principe : crisp-game-lib dessine dans un canevas LOGIQUE dont la taille
// varie par jeu (md_initView(w,h), ex: 100x50, 150x100...). On calcule un
// facteur d'echelle pour remplir au mieux l'ecran AKA (320x240, avec une
// zone de reserve en haut pour un bandeau de statut) et on l'applique a
// chaque appel de dessin -- meme principe que le canevas 110x88 de Karateka,
// mais generique/reutilisable pour n'importe quelle taille de vue.
extern "C" {
#include "lib/machineDependent.h"
}
#include "gb_core.h"
#include "gb_graphics.h"
#include "gb_audio_track_tone.h"
#include "gb_audio_player.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstring>

extern gb_core g_core;
extern gb_graphics gfx;

// Zone physique dediee au jeu (le reste de l'ecran 320x240 sert de marge/
// bandeau, coherent avec le viewport centre utilise par les autres portages).
static constexpr int PHYS_X = 10, PHYS_Y = 10, PHYS_W = 300, PHYS_H = 220;

static int   s_viewW = 100, s_viewH = 100;
static float s_scale = 1.0f;
static int   s_offX = 0, s_offY = 0;

static inline int mapX(float x) { return PHYS_X + s_offX + (int)(x * s_scale); }
static inline int mapY(float y) { return PHYS_Y + s_offY + (int)(y * s_scale); }

extern "C" void md_initView(int w, int h) {
    s_viewW = w > 0 ? w : 100;
    s_viewH = h > 0 ? h : 100;
    float scaleX = (float)PHYS_W / (float)s_viewW;
    float scaleY = (float)PHYS_H / (float)s_viewH;
    s_scale = scaleX < scaleY ? scaleX : scaleY;   // conserve le ratio, pas de deformation
    s_offX = (int)((PHYS_W - s_viewW * s_scale) / 2);
    s_offY = (int)((PHYS_H - s_viewH * s_scale) / 2);
}

extern "C" void md_clearScreen(unsigned char r, unsigned char g, unsigned char b) {
    gfx.setColor(gfx.makeColor(r, g, b));
    gfx.fillRect(0, 0, 320, 240);
}

extern "C" void md_clearView(unsigned char r, unsigned char g, unsigned char b) {
    gfx.setColor(gfx.makeColor(r, g, b));
    gfx.fillRect(PHYS_X, PHYS_Y, PHYS_W, PHYS_H);
}

// Limite un rectangle a la zone de jeu (PHYS_X..PHYS_X+PHYS_W, idem Y) --
// BUG TROUVE ET CORRIGE : aucune limite n'etait appliquee auparavant, un
// objet de jeu dont le bord depasse legerement sa vue logique (frequent en
// bordure d'ecran) debordait directement dans la marge/le bord physique
// (petit triangle/pixels parasites constates en capture d'ecran).
static bool clip_to_zone(int& px, int& py, int& pw, int& ph) {
    int x2 = px + pw, y2 = py + ph;
    if (px < PHYS_X) px = PHYS_X;
    if (py < PHYS_Y) py = PHYS_Y;
    if (x2 > PHYS_X + PHYS_W) x2 = PHYS_X + PHYS_W;
    if (y2 > PHYS_Y + PHYS_H) y2 = PHYS_Y + PHYS_H;
    pw = x2 - px; ph = y2 - py;
    return pw > 0 && ph > 0;
}

extern "C" void md_drawRect(float x, float y, float w, float h,
                             unsigned char r, unsigned char g, unsigned char b) {
    int px = mapX(x), py = mapY(y);
    int pw = (int)(w * s_scale) + 1, ph = (int)(h * s_scale) + 1;
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;
    if (!clip_to_zone(px, py, pw, ph)) return;
    gfx.setColor(gfx.makeColor(r, g, b));
    gfx.fillRect(px, py, pw, ph);
}

// Sprite "personnage" 6x6 (grille RGB) -- dessine pixel par pixel a
// l'echelle courante ; le parametre 'hash' (mise en cache cote appelant
// dans la lib d'origine) n'est pas exploite ici, le cout est negligeable
// pour des sprites 6x6.
extern "C" void md_drawCharacter(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3],
                                  float x, float y, int /*hash*/) {
    int cell = (int)s_scale; if (cell < 1) cell = 1;
    for (int gy = 0; gy < CHARACTER_HEIGHT; ++gy) {
        for (int gx = 0; gx < CHARACTER_WIDTH; ++gx) {
            unsigned char r = grid[gy][gx][0], g = grid[gy][gx][1], b = grid[gy][gx][2];
            if (r == 0 && g == 0 && b == 0) continue;   // noir = transparent (convention cglp)
            int px = mapX(x + gx), py = mapY(y + gy);
            int pw = cell, ph = cell;
            if (!clip_to_zone(px, py, pw, ph)) continue;
            gfx.setColor(gfx.makeColor(r, g, b));
            gfx.fillRect(px, py, pw, ph);
        }
    }
}

extern "C" void md_consoleLog(char* msg) {
    printf("[cglp] %s\n", msg);
}

// --- Audio : generation de tonalites par frequence (gb_audio_track_tone) ---
static gb_audio_track_tone s_toneTrack;
static bool s_toneRegistered = false;
static uint32_t s_audioStartMs = 0;

static void ensure_tone_registered() {
    if (s_toneRegistered) return;
    s_toneRegistered = true;
    s_audioStartMs = (uint32_t)(esp_timer_get_time() / 1000);
    // Rejoint le meme lecteur partage que le reste du projet (cf.
    // Pokitto::Sound::registerTrack sur les autres portages) -- ici acces
    // direct puisqu'il n'y a pas de couche Pokitto::Sound pour ce projet.
    extern gb_audio_player g_audio_player;   // instancie dans main.cpp
    g_audio_player.add_track(&s_toneTrack, 1.0f);
}

extern "C" void md_playTone(float freq, float duration, float when) {
    ensure_tone_registered();
    (void)when;   // planification fine non geree -- joue immediatement (suffisant, cf. jeux 1-bouton courts)
    s_toneTrack.play_tone(freq, 0.6f, (uint16_t)(duration * 1000.0f), gb_audio_track_tone::SQUARE);
}

extern "C" void md_stopTone() {
    s_toneTrack.stop_playing();
}

extern "C" float md_getAudioTime() {
    ensure_tone_registered();
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    return (float)(now - s_audioStartMs) / 1000.0f;
}
