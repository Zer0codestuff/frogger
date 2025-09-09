/*
  File: main2.c
  Scopo: versione COMPLETA del gioco Frogger con tutti i requisiti soddisfatti
  Include: Sistema tane, punteggio, schermate fine partita, versione processi
  Nota: Implementazione completa basata sui requisiti del progetto
*/
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <locale.h>      // setlocale per supporto UTF-8 (sprite/caratteri)
#include <ncurses.h>     // grafica testuale: WINDOW, mvwaddstr, colori, getmaxyx, box, wgetch, napms
#include <stdio.h>       // fprintf, perror
#include <stdlib.h>      // exit, malloc/free (eventuale), general-purpose
#include <stdbool.h>     // tipo bool, true/false
#include <unistd.h>      // fork, pipe, read, write, close, usleep, _exit
#include <sys/types.h>   // pid_t (tipo del PID)
#include <sys/wait.h>    // waitpid (gestione processo figlio)
#include <fcntl.h>       // fcntl, O_NONBLOCK (pipe non bloccante), flag file
#include <errno.h>       // errno, EAGAIN, EWOULDBLOCK (gestione lettura non bloccante)
#include <signal.h>      // kill, SIGKILL (terminazione processi)
#include <time.h>        // usleep, time, clock_gettime
#include <sys/time.h>    // per gettimeofday alternativa

// Helper sleep con risoluzione microsecondi (portabile via nanosleep)
static void sleep_us(unsigned int us) {
    struct timespec ts;
    ts.tv_sec  = us / 1000000u;
    ts.tv_nsec = (long)(us % 1000000u) * 1000L;
    nanosleep(&ts, NULL);
}

// ===== COSTANTI DI GIOCO =====
#define GAME_WIDTH  101             // larghezza interna dell'area di gioco
#define FROG_H      2               // altezza della rana (in caratteri)
#define FROG_W      3               // larghezza della rana (in caratteri)
#define CROC_W      (3 * FROG_W)    // larghezza 3x rana (massimo consentito)
#define CROC_H      (FROG_H)

// ===== LAYOUT VERTICALE =====
#define ZONE_TANE_H           (FROG_H)     // altezza della fascia delle tane
#define ZONE_RIVA_H           (FROG_H)     // altezza riva superiore (erba)
#define ZONE_FIUME_H          (8 * FROG_H) // altezza fiume (8 corsie)
#define ZONE_MARCIAPIEDE_H    (FROG_H)     // altezza marciapiede inferiore
#define ZONE_UI_H             3            // altezza area UI in basso

#define GAME_HEIGHT (ZONE_TANE_H + ZONE_RIVA_H + ZONE_FIUME_H + ZONE_MARCIAPIEDE_H + ZONE_UI_H + 2)

// ===== COORDINATE Y =====
#define Y_TANE          1                           // riga di inizio zona tane
#define Y_RIVA          (Y_TANE + ZONE_TANE_H)      // riga di inizio riva superiore
#define Y_FIUME         (Y_RIVA + ZONE_RIVA_H)      // riga di inizio fiume
#define Y_MARCIAPIEDE   (Y_FIUME + ZONE_FIUME_H)    // riga di inizio marciapiede
#define Y_UI            (Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H) // riga UI

// ===== COSTANTI GIOCO =====
#define LIVES_START 5                        // numero vite iniziali
#define MANCHE_TIME 60                       // secondi per manche
#define NUM_TANE 5                           // numero di tane
#define TANA_CLOSED 0                        // stato tana chiusa
#define TANA_OPEN 1                          // stato tana aperta

// ===== COLORI =====
#define COLORE_RANA            1
#define COLORE_ACQUA           2
#define COLORE_RANA_SU_ACQUA   3
#define COLORE_RANA_SU_ERBA    4
#define COLORE_STRADA          5
#define COLORE_ERBA            6
#define COLORE_TANA            7
#define COLORE_CROC            8
#define COLORE_PROJECTILE      9
#define DARK_GREEN             16

// ===== ID MESSAGGI =====
#define OBJ_RANA 1                          // id messaggio: rana
#define OBJ_CROC         2                  // id messaggio: coccodrillo
#define OBJ_PROJECTILE   4                  // id messaggio: proiettile
#define OBJ_GRENADE      5                  // id messaggio: richiesta sparo granate
#define OBJ_QUIT         3                  // id messaggio: richiesta uscita
#define N_FLUSSI         8                  // numero di corsie del fiume

// ===== COSTANTI TECNICHE =====
#define CROC_SPEED       2
#define CROC_SLEEP_US    120000
#define CREATOR_SLEEP_US 800000
#define MAX_MSGS_PER_FRAME 512
#define GRENADE_COOLDOWN_MS 500
#define MAX_CROCS 16
#define MAX_PROJECTILES 32

// ===== SPRITE =====
static const char *frog_shape[FROG_H * FROG_W] = {
    "@", "o", "@",  // prima riga: testa
    "/", "|", "\\"  // seconda riga: corpo
};

static const char *croc_sprite_right[CROC_H] = {
    " ___^^__>",
    " /__oo__\\"
};
static const char *croc_sprite_left[CROC_H] = {
    "<___^^__ ",
    "\\__oo__/ "
};

// ===== STRUTTURE DATI =====
typedef struct msg{
    int id;                                 // tipo oggetto/azione
    int x;                                  // coordinata x (o delta per la rana)
    int y;                                  // coordinata y (o delta per la rana)
    int pid;                                // pid del processo mittente
    int x_speed;                            // velocità orizzontale
} msg;

typedef struct {
    int in_use;   // 0 = slot libero, 1 = occupato
    pid_t pid;    // pid del processo coccodrillo
    int x;        // posizione corrente
    int y;        // posizione corrente
    int x_speed;  // velocità orizzontale del coccodrillo
    int has_pos;  // 0 se non ha ancora una posizione valida
    int dx_frame; // delta x accumulato in questo frame
} CrocState;

typedef struct {
    int in_use;     // Slot attivo?
    pid_t pid;      // PID del processo proiettile
    int x, y;       // Posizione
    int direction;  // Direzione (-1 = sinistra, +1 = destra)
    int id;         // OBJ_PROJECTILE o OBJ_GRENADE
} ProjectileState;

// ===== VARIABILI GLOBALI =====
static int pipe_fds[2];
static WINDOW *game_win = NULL;
static WINDOW *bg_win = NULL;

// Stato rana
static int frog_x = 0;
static int frog_y = 0;
static long long last_grenade_ms = -1000000000LL;

// Stato tane
static int tane_stato[5];
static int tane_chiuse_count = 0;

// Stato punteggio
static int punteggio_corrente = 0;
static int punteggio_manche = 0;
static int bonus_tempo = 0;

// Stato gioco
static int lives = LIVES_START;
static time_t manche_start;
static int flussi[N_FLUSSI];

// Array di coccodrilli e proiettili
static CrocState crocs[MAX_CROCS];
static ProjectileState projectiles[MAX_PROJECTILES];

// ===== PROTOTIPI FUNZIONI =====
static void init_screen_and_colors(void);
static void center_and_create_game_window(void);
static void draw_background(void);
static void init_frog_state(void);
static void clamp_frog_position(void);
static int get_frog_color_pair(void);
static void draw_frog(void);
static void croc_process(int write_fd, int flow_index);
static void croc_creator(int write_fd);
static void projectile_process(int write_fd, int start_x, int start_y, int direction, int msg_id);
static void frog_process(int write_fd, int start_x, int start_y);
static CrocState* get_croc_slot(pid_t pid);
static ProjectileState* get_projectile_slot(pid_t pid);
static void draw_crocs(void);
static void draw_projectiles(void);
static void sweep_crocs_offscreen(void);
static void sweep_projectiles_offscreen(void);
static bool is_frog_on_croc(int* out_dx);
static void snap_frog_onto_croc_edge_if_partial(void);
static void handle_frog_collisions(int* running);
static void draw_game_frame(void);
static void start_new_manche(void);
static void draw_ui(void);
static void init_game_data(void);
static void init_game_system(void);
static void terminate_process(pid_t pid);
static void cleanup_crocs(void);
static void cleanup_projectiles(void);
static void cleanup_pipes(void);
static void full_cleanup(pid_t frog_pid, pid_t creator_pid);

// Nuove funzioni per tane e punteggio
static int check_tana_collision(int frog_x, int frog_y);
static void close_tana(int tana_index);
static void draw_tane(WINDOW *win);
static void draw_punteggio_ui(int y);
static int show_victory_screen(void);
static int show_game_over_screen(void);
static int ask_play_again(void);

// ===== IMPLEMENTAZIONE =====

// Inizializza direzioni dei flussi
static void init_flussi(void) {
    int start = rand() % 2;
    for (int i = 0; i < N_FLUSSI; i++) {
        flussi[i] = (i % 2 == 0) ? start : (1 - start);
    }
}

// Inizializza velocità per flusso
static int flow_speeds[N_FLUSSI];
static void init_flow_speeds_random(void) {
    int choices[6] = {1,1,1,2,2,3};
    for (int i = 0; i < N_FLUSSI; i++) {
        flow_speeds[i] = choices[rand() % 6];
    }
}

// Tempo corrente in millisecondi
static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
}

// Inizializza ncurses
static void init_screen_and_colors(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();

    init_color(DARK_GREEN, 0, 300, 0);

    init_pair(COLORE_RANA, DARK_GREEN, COLOR_BLACK);
    init_pair(COLORE_ACQUA, COLOR_CYAN, COLOR_BLUE);
    init_pair(COLORE_RANA_SU_ACQUA, DARK_GREEN, COLOR_BLUE);
    init_pair(COLORE_RANA_SU_ERBA, DARK_GREEN, COLOR_GREEN);
    init_pair(COLORE_STRADA, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLORE_ERBA, COLOR_BLACK, COLOR_GREEN);
    init_pair(COLORE_TANA, COLOR_BLACK, COLOR_YELLOW);
    init_pair(COLORE_CROC, COLOR_BLACK, COLOR_GREEN);
    init_pair(COLORE_PROJECTILE, COLOR_BLACK, COLOR_BLUE);
}

// Crea finestra centrata
static void center_and_create_game_window(void) {
    int term_y, term_x;
    getmaxyx(stdscr, term_y, term_x);

    int win_h = GAME_HEIGHT;
    int win_w = GAME_WIDTH;

    int start_y = (term_y - win_h) / 2;
    int start_x = (term_x - win_w) / 2;
    if (start_y < 0) start_y = 0;
    if (start_x < 0) start_x = 0;

    game_win = newwin(win_h, win_w, start_y, start_x);
    if (!game_win) {
        endwin();
        fprintf(stderr, "Terminale troppo piccolo. Richiesti: %dx%d\n", win_h, win_w);
        exit(1);
    }
    keypad(game_win, TRUE);
    box(game_win, 0, 0);
}

// Disegna area rettangolare
static void draw_rect_area(WINDOW *win, int start_y, int end_y, int start_x, int end_x, char ch, int color_pair) {
    wattron(win, COLOR_PAIR(color_pair));
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            mvwaddch(win, y, x, ch);
        }
    }
    wattroff(win, COLOR_PAIR(color_pair));
}

// Disegna sfondo
static void draw_background(void) {
    int max_x = getmaxx(game_win);

    box(game_win, 0, 0);
    draw_rect_area(game_win, Y_MARCIAPIEDE, Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H, 1, max_x - 1, ' ', COLORE_STRADA);
    draw_rect_area(game_win, Y_FIUME, Y_MARCIAPIEDE, 1, max_x - 1, '~', COLORE_ACQUA);
    draw_rect_area(game_win, Y_RIVA, Y_FIUME, 1, max_x - 1, ' ', COLORE_ERBA);
    draw_rect_area(game_win, Y_TANE, Y_RIVA, 1, max_x - 1, ' ', COLORE_ERBA);
    draw_tane(game_win);
}

// Crea background prerenderizzato
static void init_background_layer(void) {
    if (!game_win) return;
    int win_h, win_w; getmaxyx(game_win, win_h, win_w);
    int start_y, start_x; getbegyx(game_win, start_y, start_x);
    bg_win = newwin(win_h, win_w, start_y, start_x);
    if (bg_win) {
        int max_x = getmaxx(bg_win);
        box(bg_win, 0, 0);
        draw_rect_area(bg_win, Y_MARCIAPIEDE, Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H, 1, max_x - 1, ' ', COLORE_STRADA);
        draw_rect_area(bg_win, Y_FIUME, Y_MARCIAPIEDE, 1, max_x - 1, '~', COLORE_ACQUA);
        draw_rect_area(bg_win, Y_RIVA, Y_FIUME, 1, max_x - 1, ' ', COLORE_ERBA);
        draw_rect_area(bg_win, Y_TANE, Y_RIVA, 1, max_x - 1, ' ', COLORE_ERBA);
        draw_tane(bg_win);
    }
}

// Gestisce bounds della rana
static void clamp_frog_position(void) {
    int max_x = getmaxx(game_win);

    if (frog_x < 1) frog_x = 1;
    if (frog_x + FROG_W > max_x - 1) frog_x = (max_x - 1) - FROG_W;
    if (frog_y < Y_TANE) frog_y = Y_TANE;

    int floor_y = Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H - FROG_H;
    if (frog_y > floor_y) frog_y = floor_y;
}

// Inizializza stato rana
static void init_frog_state(void) {
    int max_x = getmaxx(game_win);
    frog_y = Y_MARCIAPIEDE;
    frog_x = (max_x - FROG_W) / 2;
    clamp_frog_position();
}

// Determina colore rana
static int get_frog_color_pair(void) {
    int pair = COLORE_RANA;
    int temp_dx = 0;
    bool on_croc = is_frog_on_croc(&temp_dx);
    if (on_croc) {
        pair = COLORE_RANA_SU_ERBA;
    } else if (frog_y >= Y_FIUME && frog_y < Y_MARCIAPIEDE) {
        pair = COLORE_RANA_SU_ACQUA;
    } else if (frog_y >= Y_RIVA && frog_y < Y_FIUME) {
        pair = COLORE_RANA_SU_ERBA;
    } else if (frog_y >= Y_TANE && frog_y < Y_RIVA) {
        pair = COLORE_RANA_SU_ERBA;
    }
    return pair;
}

// Disegna rana
static void draw_frog(void) {
    int pair = get_frog_color_pair();
    wattron(game_win, COLOR_PAIR(pair));
    for (int i = 0; i < FROG_H; ++i) {
        for (int j = 0; j < FROG_W; ++j) {
            int index = i * FROG_W + j;
            const char *ch = frog_shape[index];
            if (ch[0] != ' ') {
                mvwprintw(game_win, frog_y + i, frog_x + j, "%s", ch);
            }
        }
    }
    wattroff(game_win, COLOR_PAIR(pair));
}

// Controlla collisioni con tane
static int check_tana_collision(int frog_x, int frog_y) {
    if (frog_y >= Y_TANE && frog_y < Y_TANE + ZONE_TANE_H) {
        int max_x = getmaxx(game_win);

        int tana_width = (max_x - 2) / NUM_TANE;
        int tana_index = frog_x / tana_width;

        if (tana_index >= 0 && tana_index < NUM_TANE) {
            if (tane_stato[tana_index] == TANA_CLOSED) {
                return -1;
            } else {
                return tana_index;
            }
        }
    }
    return -2;
}

// Chiude una tana
static void close_tana(int tana_index) {
    tane_stato[tana_index] = TANA_CLOSED;
    tane_chiuse_count++;
}

// Disegna tane
static void draw_tane(WINDOW *win) {
    int max_x = getmaxx(win);

    int tana_width = (max_x - 2) / NUM_TANE;

    for (int i = 0; i < NUM_TANE; ++i) {
        int tana_left = 1 + (i * tana_width);

        if (tane_stato[i] == TANA_OPEN) {
            wattron(win, COLOR_PAIR(COLORE_TANA));
        } else {
            wattron(win, COLOR_PAIR(COLORE_STRADA));
        }

        for (int yy = 0; yy < ZONE_TANE_H; ++yy) {
            for (int xx = 0; xx < tana_width; ++xx) {
                int px = tana_left + xx;
                if (px > 0 && px < max_x - 1) {
                    mvwaddch(win, Y_TANE + yy, px, ' ');
                }
            }
        }

        if (tane_stato[i] == TANA_OPEN) {
            wattroff(win, COLOR_PAIR(COLORE_TANA));
        } else {
            wattroff(win, COLOR_PAIR(COLORE_STRADA));
        }
    }
}

// Processo proiettile
static void projectile_process(int write_fd, int start_x, int start_y, int direction, int msg_id) {
    close(pipe_fds[0]);

    msg m;
    m.id = msg_id;
    m.pid = getpid();
    m.x_speed = direction;

    int x = start_x;
    int y = start_y;
    int left_edge = 1;
    int right_edge = GAME_WIDTH - 2;

    int wfl = fcntl(write_fd, F_GETFL, 0);
    if (wfl != -1) fcntl(write_fd, F_SETFL, wfl | O_NONBLOCK);

    while (1) {
        if (x < left_edge || x > right_edge) {
            break;
        }

        m.x = x;
        m.y = y;
        ssize_t wr = write(write_fd, &m, sizeof(m));
        if (wr < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                sleep_us(2000);
                continue;
            } else {
                break;
            }
        }

        x += direction;
        sleep_us(50000);
    }

    close(write_fd);
    _exit(0);
}

// Processo coccodrillo
static void croc_process(int write_fd, int flow_index) {
    close(pipe_fds[0]);
    int dir = (flussi[flow_index] == 0) ? +1 : -1;
    int y = Y_MARCIAPIEDE - ((flow_index + 1) * FROG_H);
    int left_edge = 1;
    int right_edge = GAME_WIDTH - 2;

    int x = (dir > 0) ? (left_edge - CROC_W) : right_edge;
    int base = flow_speeds[flow_index] > 0 ? flow_speeds[flow_index] : 1;
    int speed = base;

    msg m; m.id = OBJ_CROC; m.pid = getpid(); m.x_speed = dir * speed; m.y = y;
    int shoot_cooldown = 0;

    while (1) {
        m.x = x;
        write(write_fd, &m, sizeof(m));

        if (shoot_cooldown <= 0) {
            int shoot_chance = rand() % 100;
            if (shoot_chance < 5) {
                pid_t projectile_pid = fork();
                if (projectile_pid == 0) {
                    int projectile_x = (dir > 0) ? (x + CROC_W) : (x - 1);
                    projectile_process(write_fd, projectile_x, y, dir, OBJ_PROJECTILE);
                } else if (projectile_pid > 0) {
                    shoot_cooldown = 30;
                }
            }
        } else {
            shoot_cooldown--;
        }

        x += dir * speed;
        if ((dir > 0 && x > right_edge) || (dir < 0 && x + CROC_W < left_edge)) break;
        sleep_us(CROC_SLEEP_US * CROC_SPEED);

        pid_t zr;
        while ((zr = waitpid(-1, NULL, WNOHANG)) > 0);
    }
    close(write_fd);
    _exit(0);
}

// Processo creatore coccodrilli
static void croc_creator(int write_fd) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    init_flussi();
    init_flow_speeds_random();
    close(pipe_fds[0]);
    int last_flow = -1;
    int last_last_flow = -1;
    int last_last_last_flow = -1;
    int active_crocs = 0;
    const int MAX_ACTIVE_CROCS = 16;
    while (1) {
        if (active_crocs >= MAX_ACTIVE_CROCS) {
            pid_t rpid;
            while ((rpid = waitpid(-1, NULL, WNOHANG)) > 0) {
                if (active_crocs > 0) active_crocs--;
            }
            sleep_us(250000);
            continue;
        }
        int flow = rand() % N_FLUSSI;

        if (flow == last_flow || flow == last_last_flow || flow == last_last_last_flow) {
            sleep_us(CREATOR_SLEEP_US);
            continue;
        }

        last_last_last_flow = last_last_flow;
        last_last_flow = last_flow;
        last_flow = flow;

        pid_t pid = fork();
        if (pid == 0) {
            croc_process(write_fd, flow);
        } else if (pid > 0) {
            active_crocs++;
        }

        pid_t rpid;
        while ((rpid = waitpid(-1, NULL, WNOHANG)) > 0) {
            if (active_crocs > 0) active_crocs--;
        }
        int extra = (rand() % 800) * 1000;
        sleep_us(800000 + extra);
    }
}

// Processo rana
static void frog_process(int write_fd, int start_x __attribute__((unused)), int start_y __attribute__((unused))) {
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    msg m;
    m.id = OBJ_RANA;
    m.pid = getpid();
    m.x = 0; m.y = 0;

    int space_latch = 0;

    while (1) {
        int input = getch();
        int dx = 0, dy = 0;

        if (input == 'q' || input == 'Q') {
            m.id = OBJ_QUIT;
            m.x = 0; m.y = 0; m.x_speed = 0;
            write(write_fd, &m, sizeof(m));
            break;
        }
        else if (input == KEY_UP)    dy = -FROG_H;
        else if (input == KEY_DOWN)  dy = +FROG_H;
        else if (input == KEY_LEFT)  dx = -FROG_W;
        else if (input == KEY_RIGHT) dx = +FROG_W;
        else if (input == ' ' && space_latch == 0) {
            m.id = OBJ_GRENADE;
            m.x = frog_x;
            m.y = frog_y;
            m.x_speed = 0;
            write(write_fd, &m, sizeof(m));
            space_latch = 1;
            dx = 0; dy = 0;
        }

        if (dx != 0 || dy != 0) {
            m.id = OBJ_RANA;
            m.x = dx; m.y = dy; m.x_speed = 0;
            write(write_fd, &m, sizeof(m));
        }

        if (input != ' ') space_latch = 0;
        sleep_us(30000);
    }

    close(write_fd);
    _exit(0);
}

// Gestione slot coccodrilli
static CrocState* get_croc_slot(pid_t pid) {
    for (int i = 0; i < MAX_CROCS; i++) {
        if (crocs[i].in_use && crocs[i].pid == pid) {
            return &crocs[i];
        }
    }
    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) {
            crocs[i].in_use = 1;
            crocs[i].pid = pid;
            crocs[i].x = 0;
            crocs[i].y = 0;
            crocs[i].has_pos = 0;
            crocs[i].dx_frame = 0;
            return &crocs[i];
        }
    }
    return NULL;
}

// Gestione slot proiettili
static ProjectileState* get_projectile_slot(pid_t pid) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].in_use && projectiles[i].pid == pid) {
            return &projectiles[i];
        }
    }
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].in_use) {
            projectiles[i].in_use = 1;
            projectiles[i].pid = pid;
            projectiles[i].x = 0;
            projectiles[i].y = 0;
            projectiles[i].direction = 0;
            return &projectiles[i];
        }
    }
    return NULL;
}

// Disegna coccodrilli
static void draw_crocs(void) {
    int max_x = getmaxx(game_win);

    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) continue;
        const char **sprite = (crocs[i].x_speed > 0) ? croc_sprite_right : croc_sprite_left;
        wattron(game_win, COLOR_PAIR(COLORE_CROC));
        for (int yy = 0; yy < CROC_H; yy++) {
            for (int xx = 0; xx < CROC_W && sprite[yy][xx] != '\0'; xx++) {
                char ch = sprite[yy][xx];
                if (ch == ' ') continue;
                int px = crocs[i].x + xx;
                int py = crocs[i].y + yy;
                if (px >= 1 && px < max_x - 1 && py >= 1 && py < GAME_HEIGHT - 1) {
                    char temp[2] = {ch, '\0'};
                    mvwaddstr(game_win, py, px, temp);
                }
            }
        }
        wattroff(game_win, COLOR_PAIR(COLORE_CROC));
    }
}

// Disegna proiettili
static void draw_projectiles(void) {
    wattron(game_win, COLOR_PAIR(COLORE_PROJECTILE));
    int max_x = getmaxx(game_win);
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].in_use) continue;
        int px = projectiles[i].x;
        int py = projectiles[i].y;
        if (px >= 1 && px < max_x - 1 && py >= 1 && py < GAME_HEIGHT - 1) {
            const char *ch = (projectiles[i].id == OBJ_GRENADE) ? "💥" : "►";
            mvwaddstr(game_win, py, px, ch);
        }
    }
    wattroff(game_win, COLOR_PAIR(COLORE_PROJECTILE));
}

// Sweep coccodrilli fuori schermo
static void sweep_crocs_offscreen(void) {
    int max_x = getmaxx(game_win);
    int left_edge = 1;
    int right_edge = max_x - 2;
    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) continue;
        if (crocs[i].pid > 0) {
            if (kill(crocs[i].pid, 0) == -1 && errno == ESRCH) {
                crocs[i].in_use = 0;
                crocs[i].pid = -1;
                continue;
            }
        }

        if (crocs[i].x > right_edge || (crocs[i].x + CROC_W - 1) < left_edge) {
            crocs[i].in_use = 0;
            crocs[i].pid = -1;
            continue;
        }

        if (crocs[i].x_speed > 0 && crocs[i].x >= right_edge) {
            crocs[i].in_use = 0;
            crocs[i].pid = -1;
            continue;
        }
        if (crocs[i].x_speed < 0 && (crocs[i].x + CROC_W - 1) <= left_edge) {
            crocs[i].in_use = 0;
            crocs[i].pid = -1;
            continue;
        }
    }
}

// Sweep proiettili fuori schermo
static void sweep_projectiles_offscreen(void) {
    int max_x = getmaxx(game_win);
    int left_edge = 1;
    int right_edge = max_x - 2;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].in_use) continue;

        if (projectiles[i].pid > 0) {
            if (kill(projectiles[i].pid, 0) == -1 && errno == ESRCH) {
                projectiles[i].in_use = 0;
                projectiles[i].pid = -1;
                continue;
            }
        }

        if (projectiles[i].x < left_edge || projectiles[i].x > right_edge) {
            projectiles[i].in_use = 0;
            projectiles[i].pid = -1;
            continue;
        }
    }
}

// Verifica se rana su coccodrillo
static bool is_frog_on_croc(int* out_dx) {
    if (out_dx) *out_dx = 0;

    int frog_left  = frog_x;
    int frog_right = frog_x + FROG_W - 1;

    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) continue;

        if (frog_y != crocs[i].y) continue;

        int croc_left  = crocs[i].x;
        int croc_right = crocs[i].x + CROC_W - 1;

        if (frog_left <= croc_right && frog_right >= croc_left) {
            if (out_dx) *out_dx = crocs[i].dx_frame;
            return true;
        }
    }
    return false;
}

// Snap rana su bordo coccodrillo
static void snap_frog_onto_croc_edge_if_partial(void) {
    if (!(frog_y >= Y_FIUME && frog_y < Y_MARCIAPIEDE)) return;

    int frog_left  = frog_x;
    int frog_right = frog_x + FROG_W - 1;

    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) continue;
        if (crocs[i].y != frog_y) continue;

        int croc_left  = crocs[i].x;
        int croc_right = crocs[i].x + CROC_W - 1;

        bool overlap = (frog_left <= croc_right && frog_right >= croc_left);
        if (!overlap) continue;

        int overlap_left  = (frog_left   > croc_left)  ? frog_left  : croc_left;
        int overlap_right = (frog_right  < croc_right) ? frog_right : croc_right;
        int overlap_w = overlap_right - overlap_left + 1;

        if (overlap_w <= 0 || overlap_w >= FROG_W) continue;

        if (frog_right > croc_right && frog_left <= croc_right) {
            frog_x = croc_right - (FROG_W - 1);
            clamp_frog_position();
            return;
        }
        if (frog_left < croc_left && frog_right >= croc_left) {
            frog_x = croc_left;
            clamp_frog_position();
            return;
        }
    }
}

// Gestisce collisioni rana
static void handle_frog_collisions(int* running) {
    int final_ride_dx = 0;
    bool final_on_croc = is_frog_on_croc(&final_ride_dx);

    bool frog_in_water = (frog_y >= Y_FIUME && frog_y < Y_MARCIAPIEDE) && !final_on_croc;
    if (frog_in_water) {
        lives--;
        if (lives <= 0) {
            *running = 0;
        } else {
            start_new_manche();
        }
        return;
    }

    // Controlla collisioni con tane
    int tana_result = check_tana_collision(frog_x, frog_y);
    if (tana_result >= 0 && tana_result < NUM_TANE) {
        close_tana(tana_result);
        start_new_manche();
        return;
    } else if (tana_result == -1) {
        lives--;
        if (lives <= 0) {
            *running = 0;
        } else {
            start_new_manche();
        }
        return;
    }
}

// Disegna UI punteggio
static void draw_punteggio_ui(int y) {
    mvwprintw(game_win, y + 3, 2, "SCORE: %d", punteggio_corrente);
    if (punteggio_manche > 0) {
        mvwprintw(game_win, y + 4, 2, "LAST: +%d", punteggio_manche);
    }
}

// Disegna frame completo
static void draw_game_frame(void) {
    int max_x = getmaxx(game_win);

    if (bg_win) {
        copywin(bg_win, game_win, 0, 0, 0, 0, GAME_HEIGHT - 1, GAME_WIDTH - 1, FALSE);
    } else {
        werase(game_win);
        draw_background();
    }

    sweep_crocs_offscreen();
    sweep_projectiles_offscreen();

    draw_crocs();
    draw_projectiles();
    draw_frog();
    draw_ui();

    mvwprintw(game_win, 0, 2, "frog x=%d y=%d   ", frog_x, frog_y);
    wrefresh(game_win);
}

// Inizia nuova manche
static void start_new_manche(void) {
    manche_start = time(NULL);
    int max_x = getmaxx(game_win);
    frog_y = Y_MARCIAPIEDE;
    frog_x = (max_x - FROG_W) / 2;
    clamp_frog_position();
    punteggio_manche = 0;
    bonus_tempo = 0;
}

// Disegna UI
static void draw_ui(void) {
    int max_x = getmaxx(game_win);

    int y = Y_UI;

    int elapsed = (int)(time(NULL) - manche_start);
    if (elapsed < 0) elapsed = 0;
    if (elapsed > MANCHE_TIME) elapsed = MANCHE_TIME;
    int remaining = MANCHE_TIME - elapsed;

    mvwprintw(game_win, y, 2, "LIVES: ");
    int cx = 10;
    for (int i = 0; i < lives; i++) {
        mvwprintw(game_win, y, cx, "🐸 ");
        cx += 3;
    }

    int bar_w = 30;
    int filled = (remaining * bar_w) / MANCHE_TIME;
    if (filled < 0) filled = 0;
    if (filled > bar_w) filled = bar_w;

    mvwprintw(game_win, y + 1, 2, "TIME: [");
    for (int i = 0; i < bar_w; i++) {
        char c = (i < filled) ? '=' : ' ';
        mvwaddch(game_win, y + 1, 9 + i, c);
    }
    mvwprintw(game_win, y + 1, 9 + bar_w, "] %ds", remaining);

    // Mostra tane chiuse
    mvwprintw(game_win, y + 2, 2, "TANE: ");
    cx = 10;
    for (int i = 0; i < NUM_TANE; i++) {
        if (tane_stato[i] == TANA_OPEN) {
            mvwprintw(game_win, y + 2, cx, "[ ] ");
        } else {
            mvwprintw(game_win, y + 2, cx, "[X] ");
        }
        cx += 4;
    }

    draw_punteggio_ui(y);
}

// Inizializza dati gioco
static void init_game_data(void) {
    srand(time(NULL));
    init_frog_state();

    for (int i = 0; i < NUM_TANE; i++) {
        tane_stato[i] = TANA_OPEN;
    }
    tane_chiuse_count = 0;

    punteggio_corrente = 0;
    punteggio_manche = 0;
    bonus_tempo = 0;
}

// Inizializza sistema
static void init_game_system(void) {
    init_screen_and_colors();
    center_and_create_game_window();
    init_background_layer();
}

// Schermata vittoria
static int show_victory_screen(void) {
    werase(game_win);
    box(game_win, 0, 0);

    int center_x = GAME_WIDTH / 2;
    int start_y = GAME_HEIGHT / 2 - 4;

    wattron(game_win, COLOR_PAIR(COLORE_TANA));
    mvwprintw(game_win, start_y, center_x - 8, "VITTORIA! 🐸");
    wattroff(game_win, COLOR_PAIR(COLORE_TANA));

    mvwprintw(game_win, start_y + 2, center_x - 12, "Statistiche Partita:");
    mvwprintw(game_win, start_y + 3, center_x - 12, "Punteggio Finale: %d", punteggio_corrente);
    mvwprintw(game_win, start_y + 4, center_x - 12, "Tane Chiuse: %d/5", tane_chiuse_count);
    mvwprintw(game_win, start_y + 5, center_x - 12, "Vite Rimanenti: %d", lives);

    int tempo_totale = (int)(time(NULL) - manche_start);
    mvwprintw(game_win, start_y + 6, center_x - 12, "Tempo Totale: %d secondi", tempo_totale);

    mvwprintw(game_win, start_y + 8, center_x - 15, "Complimenti! Hai completato Frogger!");

    wrefresh(game_win);
    napms(2000);

    return ask_play_again();
}

// Schermata sconfitta
static int show_game_over_screen(void) {
    werase(game_win);
    box(game_win, 0, 0);

    int center_x = GAME_WIDTH / 2;
    int start_y = GAME_HEIGHT / 2 - 4;

    wattron(game_win, COLOR_PAIR(COLORE_ACQUA));
    mvwprintw(game_win, start_y, center_x - 8, "GAME OVER 💀");
    wattroff(game_win, COLOR_PAIR(COLORE_ACQUA));

    mvwprintw(game_win, start_y + 2, center_x - 12, "Statistiche Finali:");
    mvwprintw(game_win, start_y + 3, center_x - 12, "Punteggio: %d", punteggio_corrente);
    mvwprintw(game_win, start_y + 4, center_x - 12, "Tane Chiuse: %d/5", tane_chiuse_count);
    mvwprintw(game_win, start_y + 5, center_x - 12, "Vite Esaurite: 0/%d", LIVES_START);

    mvwprintw(game_win, start_y + 7, center_x - 18, "Non scoraggiarti! Riprova la prossima volta!");

    wrefresh(game_win);
    napms(2000);

    return ask_play_again();
}

// Chiedi se giocare ancora
static int ask_play_again(void) {
    int center_x = GAME_WIDTH / 2;
    int question_y = GAME_HEIGHT / 2 + 2;

    mvwprintw(game_win, question_y, center_x - 12, "Vuoi giocare ancora? (Y/N)");
    wrefresh(game_win);

    while (1) {
        int ch = wgetch(game_win);
        if (ch == 'y' || ch == 'Y' || ch == 'n' || ch == 'N') {
            return (ch == 'y' || ch == 'Y') ? 1 : 0;
        }
    }
}

// Funzioni di cleanup
static void terminate_process(pid_t pid) {
    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, WNOHANG);
    }
}

static void cleanup_crocs(void) {
    for (int i = 0; i < MAX_CROCS; i++) {
        if (crocs[i].in_use && crocs[i].pid > 0) {
            terminate_process(crocs[i].pid);
            crocs[i].in_use = 0;
            crocs[i].pid = -1;
        }
    }
}

static void cleanup_projectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].in_use && projectiles[i].pid > 0) {
            terminate_process(projectiles[i].pid);
            projectiles[i].in_use = 0;
            projectiles[i].pid = -1;
        }
    }
}

static void cleanup_pipes(void) {
    if (pipe_fds[0] >= 0) {
        close(pipe_fds[0]);
        pipe_fds[0] = -1;
    }
    if (pipe_fds[1] >= 0) {
        close(pipe_fds[1]);
        pipe_fds[1] = -1;
    }
}

static void full_cleanup(pid_t frog_pid, pid_t creator_pid) {
    terminate_process(frog_pid);
    terminate_process(creator_pid);
    cleanup_crocs();
    cleanup_projectiles();
    cleanup_pipes();

    if (game_win) {
        delwin(game_win);
        game_win = NULL;
    }
    if (bg_win) {
        delwin(bg_win);
        bg_win = NULL;
    }
    endwin();

    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// MAIN FUNCTION
int main(void) {
    init_game_system();
    init_game_data();
    start_new_manche();

    if (pipe(pipe_fds) == -1) {
        endwin();
        perror("pipe");
        return 1;
    }

    int max_x = getmaxx(game_win);
    int start_rx = (max_x - FROG_W) / 2;
    int start_ry = Y_MARCIAPIEDE;

    pid_t frog_pid = fork();
    if (frog_pid < 0) {
        endwin();
        perror("fork");
        return 1;
    }

    if (frog_pid == 0) {
        close(pipe_fds[0]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
        }
        frog_process(pipe_fds[1], start_rx, start_ry);
        _exit(0);
    }

    pid_t creator_pid = fork();
    if (creator_pid < 0) {
        endwin();
        perror("fork");
        return 1;
    }
    if (creator_pid == 0) {
        croc_creator(pipe_fds[1]);
        _exit(0);
    }

    int fl = fcntl(pipe_fds[0], F_GETFL, 0);
    fcntl(pipe_fds[0], F_SETFL, fl | O_NONBLOCK);

    if (bg_win) {
        copywin(bg_win, game_win, 0, 0, 0, 0, GAME_HEIGHT - 1, GAME_WIDTH - 1, FALSE);
    } else {
        draw_background();
    }
    draw_frog();
    draw_ui();
    wrefresh(game_win);
    napms(800);

    int running = 1;
    while (running) {
        int elapsed = (int)(time(NULL) - manche_start);
        if (elapsed >= MANCHE_TIME) {
            lives--;
            if (lives <= 0) {
                running = 0;
            } else {
                start_new_manche();
            }
        }

        int acc_dx = 0;
        int acc_dy = 0;

        for (int i = 0; i < MAX_CROCS; i++) {
            if (crocs[i].in_use) crocs[i].dx_frame = 0;
        }

        for (int drained = 0; drained < MAX_MSGS_PER_FRAME; drained++) {
            msg m;
            ssize_t n = read(pipe_fds[0], &m, sizeof(m));
            if (n > 0) {
                if (m.id == OBJ_RANA) {
                    acc_dx += m.x;
                    acc_dy += m.y;
                } else if (m.id == OBJ_GRENADE && m.pid == frog_pid) {
                    long long t = now_ms();
                    if (t - last_grenade_ms < GRENADE_COOLDOWN_MS) {
                        // cooldown
                    } else {
                        last_grenade_ms = t;
                        int gx = frog_x;
                        int gy = frog_y;
                        pid_t lg = fork();
                        if (lg == 0) {
                            projectile_process(pipe_fds[1], gx - 1, gy, -1, OBJ_GRENADE);
                            _exit(0);
                        }
                        pid_t rg = fork();
                        if (rg == 0) {
                            projectile_process(pipe_fds[1], gx + FROG_W, gy, +1, OBJ_GRENADE);
                            _exit(0);
                        }
                    }
                } else if (m.id == OBJ_CROC) {
                    CrocState* cs = get_croc_slot(m.pid);
                    if (cs) {
                        int prev_x = cs->x;
                        if (cs->has_pos) {
                            cs->dx_frame += (m.x - prev_x);
                        } else {
                            cs->has_pos = 1;
                        }
                        cs->x = m.x;
                        cs->y = m.y;
                        cs->x_speed = m.x_speed;
                    }
                } else if (m.id == OBJ_PROJECTILE) {
                    ProjectileState* ps = get_projectile_slot(m.pid);
                    if (ps) {
                        ps->id = OBJ_PROJECTILE;
                        if (ps->direction == 0) {
                            for (int i = 0; i < MAX_CROCS; i++) {
                                if (crocs[i].in_use && crocs[i].y == m.y) {
                                    ps->direction = (crocs[i].x_speed > 0) ? 1 : -1;
                                    break;
                                }
                            }
                            if (ps->direction == 0) ps->direction = 1;
                        }
                        ps->x = m.x;
                        ps->y = m.y;
                    }
                } else if (m.id == OBJ_GRENADE) {
                    ProjectileState* ps = get_projectile_slot(m.pid);
                    if (ps) {
                        ps->id = OBJ_GRENADE;
                        if (ps->direction == 0) {
                            ps->direction = (m.x_speed >= 0) ? 1 : -1;
                        }
                        ps->x = m.x;
                        ps->y = m.y;
                    }
                } else if (m.id == OBJ_QUIT) {
                    running = 0;
                }
                continue;
            }
            if (n == 0) {
                running = 0;
                break;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            if (n < 0) {
                perror("read");
                running = 0;
                break;
            }
        }

        pid_t zr;
        while ((zr = waitpid(-1, NULL, WNOHANG)) > 0);

        int ride_dx = 0;
        bool on_croc = is_frog_on_croc(&ride_dx);
        if (on_croc) {
            frog_x += ride_dx;
        }
        frog_x += acc_dx;
        frog_y += acc_dy;

        if (frog_x < 1) frog_x = 1;
        if (frog_x + FROG_W > max_x - 1) frog_x = (max_x - 1) - FROG_W;
        if (frog_y < Y_TANE) frog_y = Y_TANE;
        {
            int floor_y = Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H - FROG_H;
            if (frog_y > floor_y) frog_y = floor_y;
        }

        snap_frog_onto_croc_edge_if_partial();
        handle_frog_collisions(&running);

        if (tane_chiuse_count >= NUM_TANE) {
            running = 0;
        }

        draw_game_frame();
        napms(16);
    }

    // Gestione fine partita
    if (tane_chiuse_count >= NUM_TANE) {
        show_victory_screen();
    } else {
        show_game_over_screen();
    }

    full_cleanup(frog_pid, creator_pid);
    return 0;
}