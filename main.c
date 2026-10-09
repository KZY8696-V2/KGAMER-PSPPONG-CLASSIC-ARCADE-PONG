#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>
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
float ball_x = 30.0f, ball_y = 15.0f;
float ball_dx = 1.2f, ball_dy = 0.8f;
float ball_speed_multiplier = 1.0f;
int ball_accelerates = 0;

float player_y = 10.0f;
float ai_y = 10.0f;
float paddle_height = 6.0f;
float ai_speed = 0.8f;

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

void reset_ball(int scoring_player) {
    ball_x = 30.0f;
    ball_y = 15.0f;
    
    if (scoring_player == 1) {
        ball_dx = -1.2f * ball_speed_multiplier;
    } else {
        ball_dx = 1.2f * ball_speed_multiplier;
    }
    
    ball_dy = (rand() % 2 == 0) ? 0.8f : -0.8f;
}

void apply_difficulty() {
    player_score = 0;
    ai_score = 0;

    switch (current_diff) {
        case DIFF_ULTRA_EASY:
            ball_speed_multiplier = 0.8f;
            ai_speed = 0.4f; 
            ball_accelerates = 0; 
            score_limit = 3;
            break;
        case DIFF_EASY:
            ball_speed_multiplier = 1.0f;
            ai_speed = 0.8f;
            ball_accelerates = 0; 
            score_limit = 5;
            break;
        case DIFF_MEDIUM:
            ball_speed_multiplier = 1.3f;
            ai_speed = 1.2f;
            ball_accelerates = 1;
            score_limit = 7;
            break;
        case DIFF_HARD:
            ball_speed_multiplier = 1.7f;
            ai_speed = 1.6f; 
            ball_accelerates = 1; 
            score_limit = 10;
            break;
        case DIFF_CUSTOM:
            ball_speed_multiplier = ms_data.custom_saved_speed;
            score_limit = ms_data.custom_saved_limit;
            ai_speed = ms_data.custom_saved_ai;
            ball_accelerates = 1;
            break;
    }
    reset_ball(rand() % 2);
}

int main(void) {
    setup_callbacks();
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
            if (ai_y + (paddle_height / 2.0f) < ball_y) ai_y += ai_speed;
            if (ai_y + (paddle_height / 2.0f) > ball_y) ai_y -= ai_speed;

            if (ai_y < 2.0f) ai_y = 2.0f;
            if (ai_y > 30.0f - paddle_height) ai_y = 30.0f - paddle_height;

            // Ball Physics
            ball_x += ball_dx;
            ball_y += ball_dy;

            if (ball_y <= 2.0f) {
                ball_y = 2.0f;
                ball_dy = -ball_dy;
            }
            if (ball_y >= 31.0f) {
                ball_y = 31.0f;
                ball_dy = -ball_dy;
            }

            if (ball_x <= 4.0f && ball_x >= 2.0f && ball_y >= player_y && ball_y <= player_y + paddle_height) {
                ball_dx = -ball_dx;
                if (ball_accelerates) ball_dx *= 1.05f;
            }

            if (ball_x >= 56.0f && ball_x <= 58.0f && ball_y >= ai_y && ball_y <= ai_y + paddle_height) {
                ball_dx = -ball_dx;
                if (ball_accelerates) ball_dx *= 1.05f;
            }

            if (ball_x < 1.0f) {
                ai_score++;
                if (ai_score >= score_limit) {
                    current_state = STATE_GAMEOVER;
                } else {
                    reset_ball(1);
                }
            }
            if (ball_x > 59.0f) {
                player_score++;
                if (player_score >= score_limit) {
                    current_state = STATE_GAMEOVER;
                } else {
                    reset_ball(0);
                }
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

        // --- RENDERING ---
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

            pspDebugScreenPrintf("      %s [X] START GAME\n", menu_selection == 0 ? ">" : " ");
            pspDebugScreenPrintf("      %s [X] FILE DETAILS (SCORES)\n", menu_selection == 1 ? ">" : " ");
            pspDebugScreenPrintf("      %s [X] EXIT\n\n", menu_selection == 2 ? ">" : " ");
            pspDebugScreenPrintf("      Controls: (D-Pad / Analog) + [X] Select\n");
        } 
        else if (current_state == STATE_DIFFICULTY) {
            pspDebugScreenPrintf("\n      === SELECT DIFFICULTY ===\n\n");
            pspDebugScreenPrintf("      %s ULTRA EASY (Limit: 3)\n", diff_selection == 0 ? ">" : " ");
            pspDebugScreenPrintf("      %s EASY (Limit: 5 / No Speedup)\n", diff_selection == 1 ? ">" : " ");
            pspDebugScreenPrintf("      %s MEDIUM (Limit: 7)\n", diff_selection == 2 ? ">" : " ");
            pspDebugScreenPrintf("      %s HARD (Limit: 10 / Fast)\n", diff_selection == 3 ? ">" : " ");
            pspDebugScreenPrintf("      %s CUSTOM MODE (Fixed Settings)\n\n", diff_selection == 4 ? ">" : " ");
            pspDebugScreenPrintf("      [/\\] Back  |  [X] Confirm\n");
        }
        else if (current_state == STATE_CUSTOM_CONFIG) {
            pspDebugScreenPrintf("\n      === CUSTOM MODE & FIX SETTINGS ===\n\n");
            pspDebugScreenPrintf("      Fixed Score Limit: < %d >\n", ms_data.custom_saved_limit);
            pspDebugScreenPrintf("      Fixed Speed Mult:  < %.1f >\n", ms_data.custom_saved_speed);
            pspDebugScreenPrintf("      Fixed AI Power:    < %.1f >\n\n", ms_data.custom_saved_ai);
            pspDebugScreenPrintf("      [<-][->] Adjust | [X] Start | [/\\] Back\n");
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
            pspDebugScreenPrintf("      Press [X] or [/\\] to Go Back\n");
        }
        else if (current_state == STATE_PLAYING) {
            pspDebugScreenPrintf(" PLAYER: %d  |  AI: %d  (Target: %d)\n", player_score, ai_score, score_limit);
            pspDebugScreenPrintf(" ----------------------------------------------------\n");
            
            int bx = (int)ball_x;
            int by = (int)ball_y;

            for (int r = 2; r < 32; r++) {
                int p_active = (r >= (int)player_y && r <= (int)(player_y + paddle_height));
                int a_active = (r >= (int)ai_y && r <= (int)(ai_y + paddle_height));
                int b_on_row = (by == r);

                if (p_active) pspDebugScreenPrintf("#");
                else pspDebugScreenPrintf(" ");

                for (int c = 1; c < 59; c++) {
                    if (b_on_row && bx == c) {
                        pspDebugScreenPrintf("O");
                    } else {
                        pspDebugScreenPrintf(" ");
                    }
                }

                if (a_active) pspDebugScreenPrintf("#\n");
                else pspDebugScreenPrintf(" \n");
            }
            pspDebugScreenPrintf(" [SELECT] Menu  |  (Analog/D-Pad Up/Down) Move\n");
        }
        else if (current_state == STATE_GAMEOVER) {
            pspDebugScreenPrintf("\n\n      ==================================\n");
            pspDebugScreenPrintf("          GAME OVER !             \n");
            pspDebugScreenPrintf("      WINNER: %s                    \n", player_score > ai_score ? "PLAYER" : "AI BOT");
            pspDebugScreenPrintf("      scores.properties Updated!      \n");
            pspDebugScreenPrintf("      ==================================\n\n");
            pspDebugScreenPrintf("      Press [X] to return to Main Menu\n");
        }

        sceDisplayWaitVblankStart();
        sceKernelDelayThread(25000); 
    }

    return 0;
}
