// main.cpp — Point d'entree du portage AKA de crisp-game-lib-portable
// (abagames, MIT) -- 267 mini-jeux a un bouton, bibliotheque et jeux
// inchanges, seule cette couche est specifique a l'AKA.
#include "gb_core.h"
#include "gb_graphics.h"
#include "gb_audio_player.h"
#include "core/input.h"
#include "aka_runtime/aka_runtime.h"

extern "C" {
#include "lib/cglp.h"
}

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

gb_core        g_core;
gb_graphics    gfx;
gb_audio_player g_audio_player;

static void audio_mix_task(void*) {
    for (;;) {
        g_audio_player.pool();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

extern "C" void app_main(void) {
    g_core.init();
    gfx.set_backlight_percent(80);
    gfx.set_refresh_rate(60);

    akaRuntime.begin("crisp");
    static const char* const kControls[] = {
        "CTRL_DPAD", "CTRL_A_B", "CTRL_MENU_SELECT", nullptr
    };
    akaRuntime.setControlsKeys(kControls);
    akaRuntime.setCredits("Crisp Game Lib (266 jeux)", "ABA Games (kenta cho)", "MIT",
                           "github.com/abagames/crisp-game-lib-portable");
    akaRuntime.setGameMenuCallback(goToMenu);   // "Choisir un jeu" -> selecteur interne CGLP
    printf("[BOOT] akaRuntime pret\n");

    g_audio_player.set_master_volume(200);
    printf("[BOOT] audio player configure\n");
    xTaskCreatePinnedToCore(audio_mix_task, "AudioMixTask", 4096, nullptr, 5, nullptr, 1);
    printf("[BOOT] tache audio creee\n");

    printf("[BOOT] appel initGame()...\n");
    initGame();
    printf("[BOOT] initGame() termine, entree boucle principale\n");

    uint32_t frameNum = 0;
    while (true) {
        input_poll(g_keys);   // BUG TROUVE ET CORRIGE : jamais appele -- g_keys restait
                               // fige a sa valeur initiale (tout a zero) pour toujours,
                               // aucune touche n'etait donc jamais detectee.
        if (!akaRuntime.update(g_keys)) { vTaskDelay(pdMS_TO_TICKS(16)); continue; }

        setButtonState(g_keys.left, g_keys.right, g_keys.up, g_keys.down,
                        g_keys.B, g_keys.A);
        updateFrame();
        gfx.update();   // BUG TROUVE ET CORRIGE : jamais appele -- le jeu tournait
                         // (updateFrame() dessinait bien via md_drawRect/md_drawCharacter)
                         // mais rien n'etait jamais pousse vers l'ecran physique, qui
                         // restait bloque sur le logo de demarrage Gamebuino.
        if (frameNum < 5 || frameNum % 300 == 0) printf("[BOOT] frame %lu ok\n", (unsigned long)frameNum);
        ++frameNum;

        vTaskDelay(pdMS_TO_TICKS(16));   // ~60 fps, cadence fixe attendue par cglp
    }
}
