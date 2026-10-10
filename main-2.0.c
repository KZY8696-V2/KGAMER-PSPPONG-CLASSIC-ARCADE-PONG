#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("KGAMER Pong MemoryStick Fixed", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

typedef enum {
    STATE_MENU,
    STATE_DIFFICULTY,
    STATE_CUSTOM_CONFIG,
    STATE_PLAYING,
    STATE_STATS,
    STATE_CUSTOM_SEARCH,
    STATE_HISTORY,
    STATE_GAMEOVER
} GameState;

typedef enum {
    DIFF_ULTRA_EASY,
    DIFF_EASY,
    DIFF_MEDIUM,
    DIFF_HARD,
    DIFF_CUSTOM
} Difficulty;

typedef struct {
    char mode_name[24];
    int p_score;
    int a_score;
    int won;
    int pinned;
} MatchRecord;

typedef struct {
    int wins_ultra;
    int losses_ultra;
    int wins_easy;
    int losses_easy;
    int wins_medium;
    int losses_medium;
    int wins_hard;
    int losses_hard;
    
    float custom_saved_speed;
    int custom_saved_limit;
    float custom_saved_ai;

    int pinned_custom_limit;
    float pinned_custom_speed;
    int pinned_custom_wins;
    int pinned_custom_losses;

    int current_custom_wins;
    int current_custom_losses;

    MatchRecord history[30];
    int history_count;
} MemoryStickData;

MemoryStickData ms_data = {0, 0, 0, 0, 0, 0, 0, 0, 1.0f, 5, 2.0f, 5, 1.0f, 0, 0, 0, 0, {0}, 0};

GameState current_state = STATE_MENU;
Difficulty current_diff = DIFF_MEDIUM;

int menu_selection = 0; 
int diff_selection = 0; 
int history_selection = 0;

int player_score = 0;
int ai_score = 0;
int score_limit = 5;

int search_limit = 5;
float search_speed = 1.0f;
int search_found_wins = 0;
int search_found_losses = 0;

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

void get_custom_key(char *wins_key, char *losses_key, int limit, float speed) {
    int speed_int = (int)(speed * 10.0f);
    sprintf(wins_key, "custom.limit%d.speed%d.wins", limit, speed_int);
    sprintf(losses_key, "custom.limit%d.speed%d.losses", limit, speed_int);
}

int read_custom_score_from_file(const char *target_key) {
    SceUID fd = sceIoOpen("scores.properties", PSP_O_RDONLY, 0777);
    if (fd < 0) return 0;

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    sceIoRead(fd, buffer, sizeof(buffer) - 1);
    sceIoClose(fd);

    char search_fmt[64];
    sprintf(search_fmt, "%s=%%d", target_key);

    char *line = strtok(buffer, "\r\n");
    while (line != NULL) {
        int val = 0;
        if (sscanf(line, search_fmt, &val) == 1) {
            return val;
        }
        line = strtok(NULL, "\r\n");
    }
    return 0;
}

void update_pinned_main_menu_data(void) {
    char w_key[64], l_key[64];
    get_custom_key(w_key, l_key, ms_data.pinned_custom_limit, ms_data.pinned_custom_speed);
    ms_data.pinned_custom_wins = read_custom_score_from_file(w_key);
    ms_data.pinned_custom_losses = read_custom_score_from_file(l_key);
}

void load_scores(void) {
    SceUID fd = sceIoOpen("scores.properties", PSP_O_RDONLY, 0777);
    if (fd < 0) return;

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    sceIoRead(fd, buffer, sizeof(buffer) - 1);
    sceIoClose(fd);

    ms_data.history_count = 0;
    char *line = strtok(buffer, "\r\n");
    while (line != NULL) {
        int val = 0;
        if (sscanf(line, "ultra.easy.wins=%d", &val) == 1) ms_data.wins_ultra = val;
        else if (sscanf(line, "ultra.easy.losses=%d", &val) == 1) ms_data.losses_ultra = val;
        else if (sscanf(line, "easy.wins=%d", &val) == 1) ms_data.wins_easy = val;
        else if (sscanf(line, "easy.losses=%d", &val) == 1) ms_data.losses_easy = val;
        else if (sscanf(line, "medium.wins=%d", &val) == 1) ms_data.wins_medium = val;
        else if (sscanf(line, "medium.losses=%d", &val) == 1) ms_data.losses_medium = val;
        else if (sscanf(line, "hard.wins=%d", &val) == 1) ms_data.wins_hard = val;
        else if (sscanf(line, "hard.losses=%d", &val) == 1) ms_data.losses_hard = val;
        else if (sscanf(line, "custom.last.limit=%d", &val) == 1) ms_data.custom_saved_limit = val;
        else if (sscanf(line, "custom.last.speed=%f", &ms_data.custom_saved_speed) == 1);
        else if (sscanf(line, "custom.last.ai=%f", &ms_data.custom_saved_ai) == 1);
        else if (sscanf(line, "custom.pinned.limit=%d", &val) == 1) ms_data.pinned_custom_limit = val;
        else if (sscanf(line, "custom.pinned.speed=%f", &ms_data.pinned_custom_speed) == 1);
        else {
            char m_name[24];
            int p_s, a_s, w, p;
            if (sscanf(line, "history.%23[^,],p=%d,a=%d,win=%d,pin=%d", m_name, &p_s, &a_s, &w, &p) == 5) {
                if (ms_data.history_count < 30) {
                    strcpy(ms_data.history[ms_data.history_count].mode_name, m_name);
                    ms_data.history[ms_data.history_count].p_score = p_s;
                    ms_data.history[ms_data.history_count].a_score = a_s;
                    ms_data.history[ms_data.history_count].won = w;
                    ms_data.history[ms_data.history_count].pinned = p;
                    ms_data.history_count++;
                }
            }
        }
        line = strtok(NULL, "\r\n");
    }
    update_pinned_main_menu_data();
}

void save_scores(void) {
    char c_win_key[64], c_loss_key[64];
    get_custom_key(c_win_key, c_loss_key, ms_data.custom_saved_limit, ms_data.custom_saved_speed);

    int cur_c_wins = read_custom_score_from_file(c_win_key) + ms_data.current_custom_wins;
    int cur_c_losses = read_custom_score_from_file(c_loss_key) + ms_data.current_custom_losses;
    ms_data.current_custom_wins = 0;
    ms_data.current_custom_losses = 0;

    SceUID fd = sceIoOpen("scores.properties", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;

    char out[8192];
    int len = sprintf(out,
        "# KGAMER PONG PROPERTIES\n"
        "ultra.easy.wins=%d\nultra.easy.losses=%d\n"
        "easy.wins=%d\neasy.losses=%d\n"
        "medium.wins=%d\nmedium.losses=%d\n"
        "hard.wins=%d\nhard.losses=%d\n"
        "custom.last.limit=%d\ncustom.last.speed=%.1f\ncustom.last.ai=%.1f\n"
        "custom.pinned.limit=%d\ncustom.pinned.speed=%.1f\n"
        "%s=%d\n%s=%d\n",
        ms_data.wins_ultra, ms_data.losses_ultra,
        ms_data.wins_easy, ms_data.losses_easy,
        ms_data.wins_medium, ms_data.losses_medium,
        ms_data.wins_hard, ms_data.losses_hard,
        ms_data.custom_saved_limit, ms_data.custom_saved_speed, ms_data.custom_saved_ai,
        ms_data.pinned_custom_limit, ms_data.pinned_custom_speed,
        c_win_key, cur_c_wins,
        c_loss_key, cur_c_losses
    );

    for (int i = 0; i < ms_data.history_count; i++) {
        char h_line[128];
        sprintf(h_line, "history.%s,p=%d,a=%d,win=%d,pin=%d\n",
            ms_data.history[i].mode_name,
            ms_data.history[i].p_score,
            ms_data.history[i].a_score,
            ms_data.history[i].won,
            ms_data.history[i].pinned
        );
        strcat(out, h_line);
    }

    len = strlen(out);
    sceIoWrite(fd, out, len);
    sceIoClose(fd);

    update_pinned_main_menu_data();
}

void add_history_record(const char *mode, int p_s, int a_s, int won) {
    MatchRecord new_rec;
    strcpy(new_rec.mode_name, mode);
    new_rec.p_score = p_s;
    new_rec.a_score = a_s;
    new_rec.won = won;
    new_rec.pinned = 0;

    if (ms_data.history_count < 30) {
        for (int i = ms_data.history_count; i > 0; i--) ms_data.history[i] = ms_data.history[i - 1];
        ms_data.history[0] = new_rec;
        ms_data.history_count++;
    } else {
        for (int i = 29; i > 0; i--) {
            if (!ms_data.history[i].pinned) ms_data.history[i] = ms_data.history[i - 1];
        }
        ms_data.history[0] = new_rec;
    }
    save_scores();
}

void search_custom_scores(void) {
    char w_key[64], l_key[64];
    get_custom_key(w_key, l_key, search_limit, search_speed);
    search_found_wins = read_custom_score_from_file(w_key);
    search_found_losses = read_custom_score_from_file(l_key);
}

void reset_ball(int scoring_player) {
    ball_x = 30.0f;
    ball_y = 15.0f;
    
    float base_speed = 1.0f * ball_speed_multiplier;
    ball_dx = (scoring_player == 1 ? -base_speed : base_speed);
    ball_dy = (rand() % 2 == 0 ? 0.6f : -0.6f) * ball_speed_multiplier;
}

void apply_difficulty(void) {
    player_score = 0;
    ai_score = 0;

    switch (current_diff) {
        case DIFF_ULTRA_EASY:
            ball_speed_multiplier = 0.8f; ai_speed = 0.4f; ball_accelerates = 0; score_limit = 3; break;
        case DIFF_EASY:
            ball_speed_multiplier = 1.0f; ai_speed = 0.8f; ball_accelerates = 0; score_limit = 5; break;
        case DIFF_MEDIUM:
            ball_speed_multiplier = 1.3f; ai_speed = 1.2f; ball_accelerates = 1; score_limit = 7; break;
        case DIFF_HARD:
            ball_speed_multiplier = 1.7f; ai_speed = 1.6f; ball_accelerates = 1; score_limit = 10; break;
        case DIFF_CUSTOM:
            ball_speed_multiplier = ms_data.custom_saved_speed;
            score_limit = ms_data.custom_saved_limit;
            ai_speed = ms_data.custom_saved_ai;
            ball_accelerates = 1;
            break;
    }
    reset_ball(rand() % 2);
}

void record_game_result(int player_won) {
    char m_str[24];
    switch (current_diff) {
        case DIFF_ULTRA_EASY: 
            if (player_won) ms_data.wins_ultra++; else ms_data.losses_ultra++; 
            strcpy(m_str, "ULTRA EASY"); break;
        case DIFF_EASY:       
            if (player_won) ms_data.wins_easy++; else ms_data.losses_easy++; 
            strcpy(m_str, "EASY"); break;
        case DIFF_MEDIUM:     
            if (player_won) ms_data.wins_medium++; else ms_data.losses_medium++; 
            strcpy(m_str, "MEDIUM"); break;
        case DIFF_HARD:       
            if (player_won) ms_data.wins_hard++; else ms_data.losses_hard++; 
            strcpy(m_str, "HARD"); break;
        case DIFF_CUSTOM:
            if (player_won) ms_data.current_custom_wins++; else ms_data.current_custom_losses++;
            sprintf(m_str, "CUSTOM L%d", score_limit);
            break;
    }
    add_history_record(m_str, player_score, ai_score, player_won);
}

int main(void) {
    setup_callbacks();
    pspDebugScreenInit();
    load_scores();

    SceCtrlData pad, old_pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlReadBufferPositive(&old_pad, 1);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);
        int just_cross = (pad.Buttons & PSP_CTRL_CROSS) && !(old_pad.Buttons & PSP_CTRL_CROSS);
        int just_triangle = (pad.Buttons & PSP_CTRL_TRIANGLE) && !(old_pad.Buttons & PSP_CTRL_TRIANGLE);
        int just_square = (pad.Buttons & PSP_CTRL_SQUARE) && !(old_pad.Buttons & PSP_CTRL_SQUARE);

        if (current_state == STATE_MENU) {
            if ((pad.Buttons & PSP_CTRL_UP) && !(old_pad.Buttons & PSP_CTRL_UP)) {
                menu_selection--; if (menu_selection < 0) menu_selection = 4;
            }
            if ((pad.Buttons & PSP_CTRL_DOWN) && !(old_pad.Buttons & PSP_CTRL_DOWN)) {
                menu_selection++; if (menu_selection > 4) menu_selection = 0;
            }

            if (just_cross) {
                if (menu_selection == 0) current_state = STATE_DIFFICULTY;
                else if (menu_selection == 1) current_state = STATE_STATS;
                else if (menu_selection == 2) { search_custom_scores(); current_state = STATE_CUSTOM_SEARCH; }
                else if (menu_selection == 3) { history_selection = 0; current_state = STATE_HISTORY; }
                else sceKernelExitGame();
            }
        }
        else if (current_state == STATE_DIFFICULTY) {
            if ((pad.Buttons & PSP_CTRL_UP) && !(old_pad.Buttons & PSP_CTRL_UP)) {
                diff_selection--; if (diff_selection < 0) diff_selection = 4;
            }
            if ((pad.Buttons & PSP_CTRL_DOWN) && !(old_pad.Buttons & PSP_CTRL_DOWN)) {
                diff_selection++; if (diff_selection > 4) diff_selection = 0;
            }

            if (just_cross) {
                current_diff = (Difficulty)diff_selection;
                if (current_diff == DIFF_CUSTOM) current_state = STATE_CUSTOM_CONFIG;
                else { apply_difficulty(); current_state = STATE_PLAYING; }
            }
            if (just_triangle) current_state = STATE_MENU;
        }
        else if (current_state == STATE_CUSTOM_CONFIG) {
            if ((pad.Buttons & PSP_CTRL_RIGHT) && !(old_pad.Buttons & PSP_CTRL_RIGHT)) {
                ms_data.custom_saved_limit += 1; ms_data.custom_saved_speed += 0.2f; ms_data.custom_saved_ai += 0.5f;
            }
            if ((pad.Buttons & PSP_CTRL_LEFT) && !(old_pad.Buttons & PSP_CTRL_LEFT)) {
                if (ms_data.custom_saved_limit > 1) ms_data.custom_saved_limit -= 1;
                if (ms_data.custom_saved_speed > 0.4f) ms_data.custom_saved_speed -= 0.2f;
                if (ms_data.custom_saved_ai > 0.5f) ms_data.custom_saved_ai -= 0.5f;
            }
            if (just_cross) { save_scores(); apply_difficulty(); current_state = STATE_PLAYING; }
            if (just_square) {
                search_limit = ms_data.custom_saved_limit; search_speed = ms_data.custom_saved_speed;
                search_custom_scores(); current_state = STATE_CUSTOM_SEARCH;
            }
            if (just_triangle) current_state = STATE_DIFFICULTY;
        }
        else if (current_state == STATE_CUSTOM_SEARCH) {
            if ((pad.Buttons & PSP_CTRL_RIGHT) && !(old_pad.Buttons & PSP_CTRL_RIGHT)) { search_limit++; search_custom_scores(); }
            if ((pad.Buttons & PSP_CTRL_LEFT) && !(old_pad.Buttons & PSP_CTRL_LEFT)) { if (search_limit > 1) search_limit--; search_custom_scores(); }
            if ((pad.Buttons & PSP_CTRL_UP) && !(old_pad.Buttons & PSP_CTRL_UP)) { search_speed += 0.2f; search_custom_scores(); }
            if ((pad.Buttons & PSP_CTRL_DOWN) && !(old_pad.Buttons & PSP_CTRL_DOWN)) { if (search_speed > 0.4f) search_speed -= 0.2f; search_custom_scores(); }
            
            if (just_square) {
                ms_data.pinned_custom_limit = search_limit;
                ms_data.pinned_custom_speed = search_speed;
                save_scores();
            }
            if (just_triangle || just_cross) current_state = STATE_MENU;
        }
        else if (current_state == STATE_HISTORY) {
            if (ms_data.history_count > 0) {
                if ((pad.Buttons & PSP_CTRL_UP) && !(old_pad.Buttons & PSP_CTRL_UP)) {
                    history_selection--; if (history_selection < 0) history_selection = ms_data.history_count - 1;
                }
                if ((pad.Buttons & PSP_CTRL_DOWN) && !(old_pad.Buttons & PSP_CTRL_DOWN)) {
                    history_selection++; if (history_selection >= ms_data.history_count) history_selection = 0;
                }
                if (just_square) {
                    ms_data.history[history_selection].pinned = !ms_data.history[history_selection].pinned;
                    save_scores();
                }
            }
            if (just_triangle || just_cross) current_state = STATE_MENU;
        }
        else if (current_state == STATE_STATS) {
            if (just_triangle || just_cross) current_state = STATE_MENU;
        }
        else if (current_state == STATE_PLAYING) {
            if (pad.Buttons & PSP_CTRL_UP || pad.Ly < 100) player_y -= 1.2f;
            if (pad.Buttons & PSP_CTRL_DOWN || pad.Ly > 150) player_y += 1.2f;

            if (player_y < 2.0f) player_y = 2.0f;
            if (player_y > 30.0f - paddle_height) player_y = 30.0f - paddle_height;

            float ai_center = ai_y + (paddle_height / 2.0f);
            if (ai_center < ball_y - 0.5f) ai_y += ai_speed;
            else if (ai_center > ball_y + 0.5f) ai_y -= ai_speed;

            if (ai_y < 2.0f) ai_y = 2.0f;
            if (ai_y > 30.0f - paddle_height) ai_y = 30.0f - paddle_height;

            ball_x += ball_dx; 
            ball_y += ball_dy;

            if (ball_y <= 2.0f) { ball_y = 2.0f; ball_dy = -ball_dy; }
            if (ball_y >= 31.0f) { ball_y = 31.0f; ball_dy = -ball_dy; }

            if (ball_dx < 0 && ball_x <= 4.0f && ball_x >= 2.0f) {
                if (ball_y >= player_y - 0.5f && ball_y <= player_y + paddle_height + 0.5f) {
                    ball_x = 4.1f;
                    ball_dx = -ball_dx;
                    float hit_pos = (ball_y - (player_y + paddle_height / 2.0f)) / (paddle_height / 2.0f);
                    ball_dy = hit_pos * 1.2f * ball_speed_multiplier;
                    if (ball_accelerates && fabs(ball_dx) < 3.0f) ball_dx *= 1.05f;
                }
            }

            if (ball_dx > 0 && ball_x >= 56.0f && ball_x <= 58.0f) {
                if (ball_y >= ai_y - 0.5f && ball_y <= ai_y + paddle_height + 0.5f) {
                    ball_x = 55.9f;
                    ball_dx = -ball_dx;
                    float hit_pos = (ball_y - (ai_y + paddle_height / 2.0f)) / (paddle_height / 2.0f);
                    ball_dy = hit_pos * 1.2f * ball_speed_multiplier;
                    if (ball_accelerates && fabs(ball_dx) < 3.0f) ball_dx *= 1.05f;
                }
            }

            if (ball_x < 1.0f) {
                ai_score++;
                if (ai_score >= score_limit) { record_game_result(0); current_state = STATE_GAMEOVER; }
                else reset_ball(1);
            }
            if (ball_x > 59.0f) {
                player_score++;
                if (player_score >= score_limit) { record_game_result(1); current_state = STATE_GAMEOVER; }
                else reset_ball(0);
            }

            if (pad.Buttons & PSP_CTRL_SELECT) current_state = STATE_MENU;
        }
        else if (current_state == STATE_GAMEOVER) {
            if (just_cross || just_triangle) current_state = STATE_MENU;
        }

        old_pad = pad;

        pspDebugScreenClear();
        pspDebugScreenSetXY(0, 0);

        if (current_state == STATE_MENU) {
            pspDebugScreenPrintf("\n      ====================================\n");
            pspDebugScreenPrintf("            CLASSIC ARCADE: PONG          \n");
            pspDebugScreenPrintf("                   [ PSP ]                \n");
            pspDebugScreenPrintf("      ====================================\n\n");
            pspDebugScreenPrintf("      [SCORES PROPERTIES]\n");
            pspDebugScreenPrintf("      ULTRA: %dW/%dL | EASY: %dW/%dL\n", ms_data.wins_ultra, ms_data.losses_ultra, ms_data.wins_easy, ms_data.losses_easy);
            pspDebugScreenPrintf("      MED:   %dW/%dL | HARD: %dW/%dL\n", ms_data.wins_medium, ms_data.losses_medium, ms_data.wins_hard, ms_data.losses_hard);
            pspDebugScreenPrintf("      CUSTOM (L%d/Spd%.1f): %dW / %dL\n\n", 
                ms_data.pinned_custom_limit, ms_data.pinned_custom_speed, 
                ms_data.pinned_custom_wins, ms_data.pinned_custom_losses);

            pspDebugScreenPrintf("      %s [X] START GAME\n", menu_selection == 0 ? ">" : " ");
            pspDebugScreenPrintf("      %s [X] GENERAL SCORES TABLE\n", menu_selection == 1 ? ">" : " ");
            pspDebugScreenPrintf("      %s [X] CUSTOM SCORES SEARCH\n", menu_selection == 2 ? ">" : " ");
            pspDebugScreenPrintf("      %s [X] MATCH HISTORY (LAST 30)\n", menu_selection == 3 ? ">" : " ");
            pspDebugScreenPrintf("      %s [X] EXIT\n\n", menu_selection == 4 ? ">" : " ");
        } 
        else if (current_state == STATE_DIFFICULTY) {
            pspDebugScreenPrintf("\n      === SELECT DIFFICULTY ===\n\n");
            pspDebugScreenPrintf("      %s ULTRA EASY (Limit: 3)\n", diff_selection == 0 ? ">" : " ");
            pspDebugScreenPrintf("      %s EASY (Limit: 5)\n", diff_selection == 1 ? ">" : " ");
            pspDebugScreenPrintf("      %s MEDIUM (Limit: 7)\n", diff_selection == 2 ? ">" : " ");
            pspDebugScreenPrintf("      %s HARD (Limit: 10)\n", diff_selection == 3 ? ">" : " ");
            pspDebugScreenPrintf("      %s CUSTOM MODE (Custom Settings)\n\n", diff_selection == 4 ? ">" : " ");
            pspDebugScreenPrintf("      [/\\] Back  |  [X] Select\n");
        }
        else if (current_state == STATE_CUSTOM_CONFIG) {
            pspDebugScreenPrintf("\n      === CUSTOM MODE SETTINGS ===\n\n");
            pspDebugScreenPrintf("      Score Limit: < %d >\n", ms_data.custom_saved_limit);
            pspDebugScreenPrintf("      Ball Speed:  < %.1f >\n", ms_data.custom_saved_speed);
            pspDebugScreenPrintf("      AI Power:    < %.1f >\n\n", ms_data.custom_saved_ai);
            pspDebugScreenPrintf("      [<-][->] Change | [X] Start Game\n");
            pspDebugScreenPrintf("      [Square] Search Scores | [/\\] Back\n");
        }
        else if (current_state == STATE_CUSTOM_SEARCH) {
            pspDebugScreenPrintf("\n      === CUSTOM MODE SCORES SEARCH ===\n\n");
            pspDebugScreenPrintf("      Limit: < %d > | Speed: < %.1f >\n", search_limit, search_speed);
            pspDebugScreenPrintf("      -------------------------------------\n");
            pspDebugScreenPrintf("      FOUND WINS   : %d\n", search_found_wins);
            pspDebugScreenPrintf("      FOUND LOSSES : %d\n", search_found_losses);
            pspDebugScreenPrintf("      -------------------------------------\n\n");
            pspDebugScreenPrintf("      [Square] Pin/Show on Main Menu\n");
            pspDebugScreenPrintf("      [X] / [/\\] Return to Main Menu\n");
        }
        else if (current_state == STATE_HISTORY) {
            pspDebugScreenPrintf("\n      === MATCH HISTORY (LAST 30) ===\n");
            pspDebugScreenPrintf("      Pinned matches (*) appear on top!\n\n");
            if (ms_data.history_count == 0) {
                pspDebugScreenPrintf("      No match history recorded yet.\n");
            } else {
                int start_idx = history_selection - 4;
                if (start_idx < 0) start_idx = 0;
                int end_idx = start_idx + 8;
                if (end_idx > ms_data.history_count) end_idx = ms_data.history_count;

                for (int i = start_idx; i < end_idx; i++) {
                    MatchRecord *mr = &ms_data.history[i];
                    char cursor = (i == history_selection) ? '>' : ' ';
                    char pin_icon = mr->pinned ? '*' : ' ';
                    pspDebugScreenPrintf("      %c [%c] %-12s | P:%d A:%d | %s\n",
                        cursor, pin_icon, mr->mode_name, mr->p_score, mr->a_score, mr->won ? "WIN" : "LOSS");
                }
            }
            pspDebugScreenPrintf("\n      [Square] Pin/Unpin Match | [X] Back\n");
        }
        else if (current_state == STATE_STATS) {
            pspDebugScreenPrintf("\n      === scores.properties VIEWER ===\n");
            pspDebugScreenPrintf("      Path: ./scores.properties\n\n");
            pspDebugScreenPrintf("      ultra.easy.wins=%d | losses=%d\n", ms_data.wins_ultra, ms_data.losses_ultra);
            pspDebugScreenPrintf("      easy.wins=%d | losses=%d\n", ms_data.wins_easy, ms_data.losses_easy);
            pspDebugScreenPrintf("      medium.wins=%d | losses=%d\n", ms_data.wins_medium, ms_data.losses_medium);
            pspDebugScreenPrintf("      hard.wins=%d | losses=%d\n\n", ms_data.wins_hard, ms_data.losses_hard);
            pspDebugScreenPrintf("      [X] or [/\\] Back\n");
        }
        else if (current_state == STATE_PLAYING) {
            pspDebugScreenPrintf(" PLAYER: %d  |  AI: %d  (Target: %d)\n", player_score, ai_score, score_limit);
            pspDebugScreenPrintf(" ----------------------------------------------------\n");
            
            int bx = (int)ball_x, by = (int)ball_y;
            for (int r = 2; r < 32; r++) {
                int p_active = (r >= (int)player_y && r <= (int)(player_y + paddle_height));
                int a_active = (r >= (int)ai_y && r <= (int)(ai_y + paddle_height));
                int b_on_row = (by == r);

                pspDebugScreenPrintf(p_active ? "#" : " ");
                for (int c = 1; c < 59; c++) {
                    pspDebugScreenPrintf((b_on_row && bx == c) ? "O" : " ");
                }
                pspDebugScreenPrintf(a_active ? "#\n" : " \n");
            }
            pspDebugScreenPrintf(" [SELECT] Menu  |  (Analog/D-Pad) Move Paddle\n");
        }
        else if (current_state == STATE_GAMEOVER) {
            pspDebugScreenPrintf("\n\n      ==================================\n");
            pspDebugScreenPrintf("          GAME OVER !             \n");
            pspDebugScreenPrintf("      WINNER: %s                 \n", player_score > ai_score ? "PLAYER" : "AI BOT");
            pspDebugScreenPrintf("      scores.properties Updated!       \n");
            pspDebugScreenPrintf("      ==================================\n\n");
            pspDebugScreenPrintf("      [X] Return to Main Menu\n");
        }

        sceDisplayWaitVblankStart();
        sceKernelDelayThread(25000); 
    }

    return 0;
}
