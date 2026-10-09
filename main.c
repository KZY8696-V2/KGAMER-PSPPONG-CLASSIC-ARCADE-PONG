#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("KGAMER Pong MemoryStick", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];

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
float ball_x = 240.0f, ball_y = 136.0f;
float ball_dx = 2.0f, ball_dy = 2.0f;
float ball_speed_multiplier = 1.0f;
int ball_accelerates = 0;

float player_y = 100.0f;
float ai_y = 100.0f;
float paddle_height = 50.0f;
float ai_speed = 2.0f;

// Düzeltildi: SceKernelCallbackFunction prototip uyumu için 3. parametre void *arg
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
    ball_x = 240.0f;
    ball_y = 136.0f;
    player_score = 0;
    ai_score = 0;

    switch (current_diff) {
        case DIFF_ULTRA_EASY:
            ball_dx = 1.5f; ball_dy = 1.5f;
            ai_speed = 0.8f; 
            ball_accelerates = 0; 
            score_limit = 3;
            break;
        case DIFF_EASY:
            ball_dx = 2.0f; ball_dy = 2.0f;
            ai_speed = 1.5f;
            ball_accelerates = 0; 
            score_limit = 5;
            break;
        case DIFF_MEDIUM:
            ball_dx = 2.5f; ball_dy = 2.5f;
            ai_speed = 2.2f;
            ball_accelerates = 1;
            score_limit = 7;
            break;
        case DIFF_HARD:
            ball_dx = 3.5f; ball_dy = 3.5f;
            ai_speed = 3.5f; 
            ball_accelerates = 1; 
            score_limit = 10;
            break;
        case DIFF_CUSTOM:
            ball_speed_multiplier = ms_data.custom_saved_speed;
            score_limit = ms_data.custom_saved_limit;
            ai_speed = ms_data.custom_saved_ai;
            ball_dx = 3.0f * ball_speed_multiplier;
            ball_dy = 3.0f * ball_speed_multiplier;
            ball_accelerates = 1;
            break;
    }
}

int main(void) {
    setup_callbacks();
    load_from_memory_stick();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(480, 272, (void*)0x00088000, 512);
    sceGuDepthBuffer((void*)0x00110000, 512);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_LEQUAL);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

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
        // --- CUSTOM CONFIG & FIXING VALUES ---
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
        // --- MEMORY STICK STATS SCREEN ---
        else if (current_state == STATE_STATS) {
            if (just_triangle || just_cross) {
                current_state = STATE_MENU;
            }
        }
        // --- PLAYING ---
        else if (current_state == STATE_PLAYING) {
            if (pad.Buttons & PSP_CTRL_UP || pad.Ly < 100) player_y -= 3.5f;
            if (pad.Buttons & PSP_CTRL_DOWN || pad.Ly > 150) player_y += 3.5f;

            if (player_y < 10.0f) player_y = 10.0f;
            if (player_y > 272 - 10 - paddle_height) player_y = 272 - 10 - paddle_height;

            // AI Movement
            if (ai_y + (paddle_height / 2) < ball_y) ai_y += ai_speed;
            if (ai_y + (paddle_height / 2) > ball_y) ai_y -= ai_speed;

            if (ai_y < 10.0f) ai_y = 10.0f;
            if (ai_y > 272 - 10 - paddle_height) ai_y = 272 - 10 - paddle_height;

            // Ball Physics
            ball_x += ball_dx;
            ball_y += ball_dy;

            if (ball_y <= 10.0f || ball_y >= 262.0f) {
                ball_dy = -ball_dy;
            }

            // Collisions
            if (ball_x <= 30.0f && ball_x >= 20.0f && ball_y >= player_y && ball_y <= player_y + paddle_height) {
                ball_dx = -ball_dx;
                if (ball_accelerates) ball_dx *= 1.05f;
            }

            if (ball_x >= 450.0f && ball_x <= 460.0f && ball_y >= ai_y && ball_y <= ai_y + paddle_height) {
                ball_dx = -ball_dx;
                if (ball_accelerates) ball_dx *= 1.05f;
            }

            if (ball_x < 0) {
                ai_score++;
                ball_x = 240.0f; ball_y = 136.0f;
                apply_difficulty();
            }
            if (ball_x > 480) {
                player_score++;
                ball_x = 240.0f; ball_y = 136.0f;
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

        // --- RENDERING ---
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF000000);
        sceGuClearDepth(0xFFFF);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        // Düzeltildi: GU_M_PROJECTION yerine GU_PROJECTION vb. kullanıldı
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumOrtho(0.0f, 480.0f, 272.0f, 0.0f, -1.0f, 1.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();

        if (current_state == STATE_MENU) {
            pspDebugScreenSetXY(14, 3);
            pspDebugScreenPrintf("====================================");
            pspDebugScreenSetXY(14, 4);
            pspDebugScreenPrintf("      CLASSIC ARCADE: PONG          ");
            pspDebugScreenSetXY(14, 5);
            pspDebugScreenPrintf("               [ PSP ]              ");
            pspDebugScreenSetXY(14, 6);
            pspDebugScreenPrintf("====================================");

            pspDebugScreenSetXY(10, 8);
            pspDebugScreenPrintf("[MS0:/scores.properties STATS]");
            pspDebugScreenSetXY(10, 10);
            pspDebugScreenPrintf("ULTRA: %dW / %dL  |  EASY: %dW / %dL", ms_data.wins_ultra, ms_data.losses_ultra, ms_data.wins_easy, ms_data.losses_easy);
            pspDebugScreenSetXY(10, 12);
            pspDebugScreenPrintf("MED:   %dW / %dL  |  HARD: %dW / %dL", ms_data.wins_medium, ms_data.losses_medium, ms_data.wins_hard, ms_data.losses_hard);
            pspDebugScreenSetXY(10, 14);
            pspDebugScreenPrintf("CUSTOM (Fixed): %dW / %dL", ms_data.wins_custom, ms_data.losses_custom);

            pspDebugScreenSetXY(16, 18);
            pspDebugScreenPrintf("%s [1] START GAME", menu_selection == 0 ? ">" : " ");
            pspDebugScreenSetXY(16, 20);
            pspDebugScreenPrintf("%s [2] FILE DETAILS (SCORES)", menu_selection == 1 ? ">" : " ");
            pspDebugScreenSetXY(16, 22);
            pspDebugScreenPrintf("%s [3] EXIT", menu_selection == 2 ? ">" : " ");

            pspDebugScreenSetXY(15, 25);
            pspDebugScreenPrintf("     2026 KGAMER - ALL RIGHTS RESERVED     ");
        } 
        else if (current_state == STATE_DIFFICULTY) {
            pspDebugScreenSetXY(15, 4);
            pspDebugScreenPrintf("=== SELECT DIFFICULTY ===");
            pspDebugScreenSetXY(15, 8);
            pspDebugScreenPrintf("%s ULTRA EASY (Limit: 3)", diff_selection == 0 ? ">" : " ");
            pspDebugScreenSetXY(15, 10);
            pspDebugScreenPrintf("%s EASY (Limit: 5 / No Speedup)", diff_selection == 1 ? ">" : " ");
            pspDebugScreenSetXY(15, 12);
            pspDebugScreenPrintf("%s MEDIUM (Limit: 7)", diff_selection == 2 ? ">" : " ");
            pspDebugScreenSetXY(15, 14);
            pspDebugScreenPrintf("%s HARD (Limit: 10 / Fast)", diff_selection == 3 ? ">" : " ");
            pspDebugScreenSetXY(15, 16);
            pspDebugScreenPrintf("%s CUSTOM MODE (Fixed Settings)", diff_selection == 4 ? ">" : " ");
            pspDebugScreenSetXY(15, 22);
            pspDebugScreenPrintf("Press TRIANGLE (Y) to Return");
        }
        else if (current_state == STATE_CUSTOM_CONFIG) {
            pspDebugScreenSetXY(12, 5);
            pspDebugScreenPrintf("=== CUSTOM MODE & FIX SETTINGS ===");
            pspDebugScreenSetXY(10, 9);
            pspDebugScreenPrintf("Fixed Score Limit: < %d >", ms_data.custom_saved_limit);
            pspDebugScreenSetXY(10, 11);
            pspDebugScreenPrintf("Fixed Speed Mult:  < %.1f >", ms_data.custom_saved_speed);
            pspDebugScreenSetXY(10, 13);
            pspDebugScreenPrintf("Fixed AI Power:    < %.1f >", ms_data.custom_saved_ai);
            pspDebugScreenSetXY(10, 17);
            pspDebugScreenPrintf("Left/Right to adjust, X to Start!");
            pspDebugScreenSetXY(10, 21);
            pspDebugScreenPrintf("Press Triangle to Return");
        }
        else if (current_state == STATE_STATS) {
            pspDebugScreenSetXY(12, 4);
            pspDebugScreenPrintf("=== MEMORY STICK FILE VIEW ===");
            pspDebugScreenSetXY(10, 7);
            pspDebugScreenPrintf("Path: ms0:/PSP/GAME/PONG/scores.properties");
            pspDebugScreenSetXY(10, 10);
            pspDebugScreenPrintf("ultra.easy.wins=%d", ms_data.wins_ultra);
            pspDebugScreenSetXY(10, 12);
            pspDebugScreenPrintf("ultra.easy.losses=%d", ms_data.losses_ultra);
            pspDebugScreenSetXY(10, 14);
            pspDebugScreenPrintf("easy.losses=%d (Wins not saved)", ms_data.losses_easy);
            pspDebugScreenSetXY(10, 16);
            pspDebugScreenPrintf("medium.wins=%d | medium.losses=%d", ms_data.wins_medium, ms_data.losses_medium);
            pspDebugScreenSetXY(10, 18);
            pspDebugScreenPrintf("hard.wins=%d (Losses not saved)", ms_data.wins_hard);
            pspDebugScreenSetXY(10, 20);
            pspDebugScreenPrintf("custom.wins=%d | custom.losses=%d", ms_data.wins_custom, ms_data.losses_custom);
            pspDebugScreenSetXY(12, 24);
            pspDebugScreenPrintf("Press [X] or [Y] to Go Back");
        }
        else if (current_state == STATE_PLAYING) {
            pspDebugScreenSetXY(30, 2);
            pspDebugScreenPrintf("PLAYER: %d  |  AI: %d", player_score, ai_score);
            
            // Big Center Score Numbers
            pspDebugScreenSetXY(27, 8);
            pspDebugScreenPrintf("  %d   :   %d  ", player_score, ai_score);

            pspDebugScreenSetXY(38, 32);
            pspDebugScreenPrintf("SELECT FOR MENU");
        }
        else if (current_state == STATE_GAMEOVER) {
            pspDebugScreenSetXY(15, 10);
            pspDebugScreenPrintf("==================================");
            pspDebugScreenSetXY(15, 12);
            pspDebugScreenPrintf("          GAME OVER !             ");
            pspDebugScreenSetXY(15, 14);
            pspDebugScreenPrintf("    WINNER: %s                    ", player_score > ai_score ? "PLAYER" : "AI BOT");
            pspDebugScreenSetXY(15, 16);
            pspDebugScreenPrintf("  scores.properties Updated!      ");
            pspDebugScreenSetXY(15, 18);
            pspDebugScreenPrintf("==================================");
            pspDebugScreenSetXY(15, 22);
            pspDebugScreenPrintf("Press X to return to Main Menu");
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
