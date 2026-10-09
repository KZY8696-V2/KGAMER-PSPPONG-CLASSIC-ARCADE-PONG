#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <string.h>

PSP_MODULE_INFO("KGAMER Pong MemoryStick", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

// Game States
typedef enum {
    STATE_MENU,
    STATE_DIFFICULTY,
    STATE_CUSTOM_CONFIG,
    STATE_PLAYING,
    STATE_STATS,
    STATE_GAMEOVER
} GameState;

// Difficulty Levels
typedef enum {
    DIFF_ULTRA_EASY,
    DIFF_EASY,
    DIFF_MEDIUM,
    DIFF_HARD,
    DIFF_CUSTOM
} Difficulty;

// Memory Stick / scores.properties Data Structure
typedef struct {
    int wins_ultra;
    int losses_ultra;
    int wins_easy;
    int losses_easy;
    int wins_medium;
    int losses_medium;
    int wins_hard;
    int losses_hard;
    int wins_custom;
    int losses_custom;
    float custom_saved_speed;
    int custom_saved_limit;
    float custom_saved_ai;
} MemoryStickData;

MemoryStickData ms_data = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0f, 5, 2.0f};

GameState current_state = STATE_MENU;
Difficulty current_diff = DIFF_MEDIUM;

int menu_selection = 0; 
int diff_selection = 0; 

int player_score = 0;
int ai_score = 0;
int score_limit = 5;

// Ball & Paddles
float ball_x = 30.0f, ball_y = 10.0f;
float ball_dx = 1.0f, ball_dy = 1.0f;
float ball_speed_multiplier = 1.0f;
int ball_accelerates = 0;

float player_y = 10.0f;
float ai_y = 10.0f;
float paddle_height = 5.0f;
float ai_speed = 1.0f;

// Callback
int exit_callback(int arg1, int arg2, void *arg) {
    sceKernelExitGame();
    return 0;
}

int callback_thread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

void setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

void load_from_memory_stick() {
    // Simulated loading from ms0:/PSP/GAME/PONG/scores.properties
}

void save_to_memory_stick(int player_won, Difficulty diff) {
    if (diff == DIFF_ULTRA_EASY) {
        if (!player_won) ms_data.losses_ultra++;
    } 
    else if (diff == DIFF_EASY) {
        if (!player_won) ms_data.losses_easy++; 
    } 
    else if (diff == DIFF_MEDIUM) {
        if (player_won) ms_data.wins_medium++;
        else ms_data.losses_medium++;
    } 
    else if (diff == DIFF_HARD) {
        if (player_won) ms_data.wins_hard++; 
    } 
    else if (diff == DIFF_CUSTOM) {
        if (player_won) ms_data.wins_custom++;
        else ms_data.losses_custom++;
        ms_data.custom_saved_speed = ball_speed_multiplier;
        ms_data.custom_saved_limit = score_limit;
        ms_data.custom_saved_ai = ai_speed;
    }
}

void apply_difficulty() {
    ball_x = 30.0f;
    ball_y = 10.0f;
    player_score = 0;
    ai_score = 0;

    switch (current_diff) {
        case DIFF_ULTRA_EASY:
            ball_dx = 1.0f; ball_dy = 1.0f;
            ai_speed = 0.5f; 
            ball_accelerates = 0; 
            score_limit = 3; // Ultra Easy skor sınırı: 3
            break;
        case DIFF_EASY:
            ball_dx = 1.5f; ball_dy = 1.5f;
            ai_speed = 1.0f;
            ball_accelerates = 0; 
            score_limit = 5; // Easy skor sınırı: 5
            break;
        case DIFF_MEDIUM:
            ball_dx = 2.0f; ball_dy = 2.0f;
            ai_speed = 1.5f;
            ball_accelerates = 1;
            score_limit = 7; // Medium skor sınırı: 7
            break;
        case DIFF_HARD:
            ball_dx = 2.5f; ball_dy = 2.5f;
            ai_speed = 2.2f; 
            ball_accelerates = 1; 
            score_limit = 10; // Hard skor sınırı: 10
            break;
        case DIFF_CUSTOM:
            ball_speed_multiplier = ms_data.custom_saved_speed;
            score_limit = ms_data.custom_saved_limit;
            ai_speed = ms_data.custom_saved_ai;
            ball_dx = 2.0f * ball_speed_multiplier;
            ball_dy = 2.0f * ball_speed_multiplier;
            ball_accelerates = 1;
            break;
    }
}

int main(void) {
    setup_callbacks();
    load_from_memory_stick();
    pspDebugScreenInit();

    SceCtrlData pad, old_pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlReadBufferPositive(&old_pad, 1);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);
        int just_cross = (pad.Buttons & PSP_CTRL_CROSS) && !(old_pad.Buttons & PSP_CTRL_CROSS);
        int just_triangle = (pad.Buttons & PSP_CTRL_TRIANGLE) && !(old_pad.Buttons & PSP_CTRL_TRIANGLE);

        // --- MAIN MENU ---
        if (current_state == STATE_MENU) {
            if ((pad.Buttons & PSP_CTRL_UP) && !(old_pad.Buttons & PSP_CTRL_UP)) {
                menu_selection--;
                if (menu_selection < 0) menu_selection = 2;
            }
            if ((pad.Buttons & PSP_CTRL_DOWN) && !(old_pad.Buttons & PSP_CTRL_DOWN)) {
                menu_selection++;
                if (menu_selection > 2) menu_selection = 0;
            }

            if (just_cross) {
                if (menu_selection == 0) current_state = STATE_DIFFICULTY;
                else if (menu_selection == 1) current_state = STATE_STATS;
                else sceKernelExitGame();
            }
        }
        // --- DIFFICULTY SELECT ---
        else if (current_state == STATE_DIFFICULTY) {
            if ((pad.Buttons & PSP_CTRL_UP) && !(old_pad.Buttons & PSP_CTRL_UP)) {
                diff_selection--;
                if (diff_selection < 0) diff_selection = 4;
            }
            if ((pad.Buttons & PSP_CTRL_DOWN) && !(old_pad.Buttons & PSP_CTRL_DOWN)) {
                diff_selection++;
                if (diff_selection > 4) diff_selection = 0;
            }

            if (just_cross) {
                current_diff = (Difficulty)diff_selection;
                if (current_diff == DIFF_CUSTOM) {
                    current_state = STATE_CUSTOM_CONFIG;
                } else {
                    apply_difficulty();
                    current_state = STATE_PLAYING;
                }
            }
            if (just_triangle) current_state = STATE_MENU;
        }
        // --- CUSTOM CONFIG ---
        else if (current_state == STATE_CUSTOM_CONFIG) {
            if ((pad.Buttons & PSP_CTRL_RIGHT) && !(old_pad.Buttons & PSP_CTRL_RIGHT)) {
                ms_data.custom_saved_limit += 1;
                ms_data.custom_saved_speed += 0.2f;
                ms_data.custom_saved_ai += 0.5f;
            }
            if ((pad.Buttons & PSP_CTRL_LEFT) && !(old_pad.Buttons & PSP_CTRL_LEFT)) {
                if (ms_data.custom_saved_limit > 1) ms_data.custom_saved_limit -= 1;
                if (ms_data.custom_saved_speed > 0.4f) ms_data.custom_saved_speed -= 0.2f;
                if (ms_data.custom_saved_ai > 0.5f) ms_data.custom_saved_ai -= 0.5f;
            }
            if (just_cross) {
                apply_difficulty();
                current_state = STATE_PLAYING;
            }
            if (just_triangle) current_state = STATE_DIFFICULTY;
        }
        // --- STATS SCREEN ---
        else if (current_state == STATE_STATS) {
            if (just_triangle || just_cross) {
                current_state = STATE_MENU;
            }
        }
        // --- PLAYING ---
        else if (current_state == STATE_PLAYING) {
            if (pad.Buttons & PSP_CTRL_UP || pad.Ly < 100) player_y -= 1.0f;
            if (pad.Buttons & PSP_CTRL_DOWN || pad.Ly > 150) player_y += 1.0f;

            if (player_y < 2.0f) player_y = 2.0f;
            if (player_y > 30.0f - paddle_height) player_y = 30.0f - paddle_height;

            // AI Movement
            if (ai_y + (paddle_height / 2) < ball_y) ai_y += ai_speed;
            if (ai_y + (paddle_height / 2) > ball_y) ai_y -= ai_speed;

            if (ai_y < 2.0f) ai_y = 2.0f;
            if (ai_y > 30.0f - paddle_height) ai_y = 30.0f - paddle_height;

            // Ball Physics
            ball_x += ball_dx;
            ball_y += ball_dy;

            if (ball_y <= 2.0f || ball_y >= 32.0f) {
                ball_dy = -ball_dy;
            }

            // Collisions
            if (ball_x <= 4.0f && ball_y >= player_y && ball_y <= player_y + paddle_height) {
                ball_dx = -ball_dx;
                if (ball_accelerates) ball_dx *= 1.05f;
            }

            if (ball_x >= 56.0f && ball_y >= ai_y && ball_y <= ai_y + paddle_height) {
                ball_dx = -ball_dx;
                if (ball_accelerates) ball_dx *= 1.05f;
            }

            if (ball_x < 0) {
                ai_score++;
                ball_x = 30.0f; ball_y = 15.0f;
                apply_difficulty();
            }
            if (ball_x > 60) {
                player_score++;
                ball_x = 30.0f; ball_y = 15.0f;
                apply_difficulty();
            }

            if (player_score >= score_limit || ai_score >= score_limit) {
                int won = (player_score > ai_score) ? 1 : 0;
                save_to_memory_stick(won, current_diff);
                current_state = STATE_GAMEOVER;
            }

            if (pad.Buttons & PSP_CTRL_SELECT) {
                current_state = STATE_MENU;
            }
        }
        // --- GAME OVER ---
        else if (current_state == STATE_GAMEOVER) {
            if (just_cross || just_triangle) {
                current_state = STATE_MENU;
            }
        }

        old_pad = pad;

        // --- RENDERING (Saf pspDebugScreen ile Siyah Ekran Sorunu Olmaksızın) ---
        pspDebugScreenClear();
        pspDebugScreenSetXY(0, 0);

        if (current_state == STATE_MENU) {
            pspDebugScreenPrintf("\n");
            pspDebugScreenPrintf("      ====================================\n");
            pspDebugScreenPrintf("            CLASSIC ARCADE: PONG          \n");
            pspDebugScreenPrintf("                   [ PSP ]                \n");
            pspDebugScreenPrintf("      ====================================\n\n");
            pspDebugScreenPrintf("      [MS0:/scores.properties STATS]\n");
            pspDebugScreenPrintf("      ULTRA: %dW / %dL  |  EASY: %dW / %dL\n", ms_data.wins_ultra, ms_data.losses_ultra, ms_data.wins_easy, ms_data.losses_easy);
            pspDebugScreenPrintf("      MED:   %dW / %dL  |  HARD: %dW / %dL\n", ms_data.wins_medium, ms_data.losses_medium, ms_data.wins_hard, ms_data.losses_hard);
            pspDebugScreenPrintf("      CUSTOM (Fixed): %dW / %dL\n\n", ms_data.wins_custom, ms_data.losses_custom);

            pspDebugScreenPrintf("      %s [1] START GAME\n", menu_selection == 0 ? ">" : " ");
            pspDebugScreenPrintf("      %s [2] FILE DETAILS (SCORES)\n", menu_selection == 1 ? ">" : " ");
            pspDebugScreenPrintf("      %s [3] EXIT\n\n", menu_selection == 2 ? ">" : " ");
            pspDebugScreenPrintf("           2026 KGAMER - ALL RIGHTS RESERVED     \n");
        } 
        else if (current_state == STATE_DIFFICULTY) {
            pspDebugScreenPrintf("\n      === SELECT DIFFICULTY ===\n\n");
            pspDebugScreenPrintf("      %s ULTRA EASY (Limit: 3)\n", diff_selection == 0 ? ">" : " ");
            pspDebugScreenPrintf("      %s EASY (Limit: 5 / No Speedup)\n", diff_selection == 1 ? ">" : " ");
            pspDebugScreenPrintf("      %s MEDIUM (Limit: 7)\n", diff_selection == 2 ? ">" : " ");
            pspDebugScreenPrintf("      %s HARD (Limit: 10 / Fast)\n", diff_selection == 3 ? ">" : " ");
            pspDebugScreenPrintf("      %s CUSTOM MODE (Fixed Settings)\n\n", diff_selection == 4 ? ">" : " ");
            pspDebugScreenPrintf("      Press TRIANGLE (Y) to Return\n");
        }
        else if (current_state == STATE_CUSTOM_CONFIG) {
            pspDebugScreenPrintf("\n      === CUSTOM MODE & FIX SETTINGS ===\n\n");
            pspDebugScreenPrintf("      Fixed Score Limit: < %d >\n", ms_data.custom_saved_limit);
            pspDebugScreenPrintf("      Fixed Speed Mult:  < %.1f >\n", ms_data.custom_saved_speed);
            pspDebugScreenPrintf("      Fixed AI Power:    < %.1f >\n\n", ms_data.custom_saved_ai);
            pspDebugScreenPrintf("      Left/Right to adjust, X to Start!\n");
            pspDebugScreenPrintf("      Press Triangle to Return\n");
        }
        else if (current_state == STATE_STATS) {
            pspDebugScreenPrintf("\n      === MEMORY STICK FILE VIEW ===\n\n");
            pspDebugScreenPrintf("      Path: ms0:/PSP/GAME/PONG/scores.properties\n\n");
            pspDebugScreenPrintf("      ultra.easy.wins=%d\n", ms_data.wins_ultra);
            pspDebugScreenPrintf("      ultra.easy.losses=%d\n", ms_data.losses_ultra);
            pspDebugScreenPrintf("      easy.losses=%d (Wins not saved)\n", ms_data.losses_easy);
            pspDebugScreenPrintf("      medium.wins=%d | medium.losses=%d\n", ms_data.wins_medium, ms_data.losses_medium);
            pspDebugScreenPrintf("      hard.wins=%d (Losses not saved)\n", ms_data.wins_hard);
            pspDebugScreenPrintf("      custom.wins=%d | custom.losses=%d\n\n", ms_data.wins_custom, ms_data.losses_custom);
            pspDebugScreenPrintf("      Press [X] or [Y] to Go Back\n");
        }
        else if (current_state == STATE_PLAYING) {
            pspDebugScreenPrintf(" PLAYER: %d  |  AI: %d  (Target: %d)\n", player_score, ai_score, score_limit);
            pspDebugScreenPrintf(" ----------------------------------------------------\n");
            for (int r = 2; r < 32; r++) {
                int p_active = (r >= (int)player_y && r <= (int)(player_y + paddle_height));
                int a_active = (r >= (int)ai_y && r <= (int)(ai_y + paddle_height));
                int b_active = ((int)ball_y == r);

                if (p_active && b_active) pspDebugScreenPrintf(" #                     O                             #\n");
                else if (p_active && a_active) pspDebugScreenPrintf(" #                                                   #\n");
                else if (p_active) pspDebugScreenPrintf(" #                                                   \n");
                else if (a_active) pspDebugScreenPrintf("                                                   #\n");
                else if (b_active) pspDebugScreenPrintf("                       O                             \n");
                else pspDebugScreenPrintf("\n");
            }
            pspDebugScreenPrintf(" SELECT FOR MENU\n");
        }
        else if (current_state == STATE_GAMEOVER) {
            pspDebugScreenPrintf("\n\n      ==================================\n");
            pspDebugScreenPrintf("          GAME OVER !             \n");
            pspDebugScreenPrintf("      WINNER: %s                    \n", player_score > ai_score ? "PLAYER" : "AI BOT");
            pspDebugScreenPrintf("      scores.properties Updated!      \n");
            pspDebugScreenPrintf("      ==================================\n\n");
            pspDebugScreenPrintf("      Press X to return to Main Menu\n");
        }

        sceDisplayWaitVblankStart();
        sceKernelDelayThread(30000); // Stabil kare hızı
    }

    return 0;
}
