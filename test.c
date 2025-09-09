/*
 * TEST.C - Nuovo progetto Frogger ispirato a frogger_ultimate
 * ========================================================
 *
 * Questo progetto combina:
 * - L'architettura pulita di main.c
 * - Il sistema a processi di frogger_ultimate
 *
 * Ogni entità del gioco (rana, coccodrilli, proiettili)
 * sarà un processo separato che comunica tramite pipe.
 */

// INCLUDES ESSENZIALI
#include <locale.h>      // Per UTF-8 e caratteri speciali
#include <ncurses.h>     // Libreria grafica testuale
#include <stdio.h>       // I/O standard
#include <stdlib.h>      // Funzioni di utilità
#include <stdbool.h>     // Tipo bool
#include <string.h>      // Manipolazione stringhe
#include <unistd.h>      // Processi (fork, pipe)
#include <sys/wait.h>    // waitpid
#include <fcntl.h>       // Controllo file descriptor
#include <errno.h>       // Gestione errori
#include <signal.h>      // Segnali processi
#include <time.h>        // Time e random

// DIMENSIONI FINESTRA E GIOCO
#define GAME_WIDTH  101             // Larghezza area di gioco
#define FROG_H      2               // Altezza rana
#define FROG_W      3               // Larghezza rana

// Sprite della rana (3x2 caratteri)
static const char *frog_shape[FROG_H] = {
    "@-@",  // Testa con occhi
    "l-l",  // Corpo
};

// Layout verticale del gioco
#define ZONE_TANE_H           (FROG_H)     // Altezza zona tane
#define ZONE_RIVA_H           (FROG_H)     // Altezza riva
#define ZONE_FIUME_H          (8 * FROG_H) // Altezza fiume (8 corsie)
#define ZONE_MARCIAPIEDE_H    (FROG_H)     // Altezza marciapiede
#define ZONE_UI_H             3            // Altezza UI

#define GAME_HEIGHT (ZONE_TANE_H + ZONE_RIVA_H + ZONE_FIUME_H + ZONE_MARCIAPIEDE_H + ZONE_UI_H + 2)

// Coordinate Y delle zone
#define Y_TANE          1
#define Y_RIVA          (Y_TANE + ZONE_TANE_H)
#define Y_FIUME         (Y_RIVA + ZONE_RIVA_H)
#define Y_MARCIAPIEDE   (Y_FIUME + ZONE_FIUME_H)
#define Y_UI            (Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H)

// COLORI
#define COLORE_RANA            1
#define COLORE_ACQUA           2
#define COLORE_RANA_SU_ACQUA   3
#define COLORE_RANA_SU_ERBA    4
#define COLORE_STRADA          5
#define COLORE_ERBA            6
#define COLORE_TANA            7
#define COLORE_CROC            8

// ID MESSAGGI (come in frogger_ultimate)
#define OBJ_RANA 1
#define OBJ_CROC 2
#define OBJ_PROJECTILE 4
#define OBJ_GRENADE 5
#define OBJ_QUIT 3

// Dimensioni entità
#define N_FLUSSI 8
#define CROC_W (3 * FROG_W)  // Larghezza coccodrillo
#define CROC_H FROG_H        // Altezza coccodrillo

// Velocità e timing
#define CROC_SPEED 2
#define CROC_SLEEP_US 120000
#define CREATOR_SLEEP_US 800000

// VARIABILI GLOBALI
static int pipe_fds[2];              // Pipe per comunicazione
static WINDOW *game_win = NULL;      // Finestra di gioco
static WINDOW *bg_win = NULL;        // Finestra background

// Struttura messaggi (compatibile con frogger_ultimate)
typedef struct msg {
    int id;                          // Tipo oggetto
    int x, y;                        // Coordinate
    pid_t pid;                       // PID processo
    int x_speed;                     // Velocità orizzontale
} msg;

// Stato posizioni entità
typedef struct {
    int in_use;                      // Slot occupato?
    pid_t pid;                       // PID processo
    int x, y;                        // Posizione
    int x_speed;                     // Velocità
    int has_pos;                     // Ha posizione valida?
    int dx_frame;                    // Delta movimento frame
} CrocState;

#define MAX_CROCS 16
static CrocState crocs[MAX_CROCS];

// Struttura proiettili
typedef struct {
    int in_use;
    pid_t pid;
    int x, y;
    int direction;
} ProjectileState;

#define MAX_PROJECTILES 32
static ProjectileState projectiles[MAX_PROJECTILES];

// Stato rana
static int frog_x = 0, frog_y = 0;
static int lives = 5;
static time_t manche_start;
static int flussi[N_FLUSSI];  // Direzioni flussi

// FORWARD DECLARATIONS
static void init_screen_and_colors(void);
static void center_and_create_game_window(void);
static void draw_background(void);
static void init_frog_state(void);
static void draw_frog(void);
static void draw_crocs(void);
static void draw_projectiles(void);

// INIZIALIZZAZIONE NCURSES
static void init_screen_and_colors(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();

    // Definizione colori
    init_pair(COLORE_RANA, COLOR_RED, COLOR_BLACK);
    init_pair(COLORE_ACQUA, COLOR_CYAN, COLOR_BLUE);
    init_pair(COLORE_RANA_SU_ACQUA, COLOR_RED, COLOR_BLUE);
    init_pair(COLORE_RANA_SU_ERBA, COLOR_RED, COLOR_GREEN);
    init_pair(COLORE_STRADA, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLORE_ERBA, COLOR_BLACK, COLOR_GREEN);
    init_pair(COLORE_TANA, COLOR_BLACK, COLOR_YELLOW);
    init_pair(COLORE_CROC, COLOR_BLACK, COLOR_GREEN);
}

// CREAZIONE FINESTRA CENTRATA
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

// DISEGNO SFONDO (PRESO DA main.c)
static void draw_background(void) {
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);

    // Bordo
    box(game_win, 0, 0);

    // Disegna rettangoli colorati per ogni zona
    // Marciapiede (basso)
    for (int y = Y_MARCIAPIEDE; y < Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H; y++) {
        for (int x = 1; x < max_x - 1; x++) {
            wattron(game_win, COLOR_PAIR(COLORE_STRADA));
            mvwaddch(game_win, y, x, ' ');
            wattroff(game_win, COLOR_PAIR(COLORE_STRADA));
        }
    }

    // Fiume (centrale)
    for (int y = Y_FIUME; y < Y_MARCIAPIEDE; y++) {
        for (int x = 1; x < max_x - 1; x++) {
            wattron(game_win, COLOR_PAIR(COLORE_ACQUA));
            mvwaddch(game_win, y, x, '~');
            wattroff(game_win, COLOR_PAIR(COLORE_ACQUA));
        }
    }

    // Riva (erba)
    for (int y = Y_RIVA; y < Y_FIUME; y++) {
        for (int x = 1; x < max_x - 1; x++) {
            wattron(game_win, COLOR_PAIR(COLORE_ERBA));
            mvwaddch(game_win, y, x, ' ');
            wattroff(game_win, COLOR_PAIR(COLORE_ERBA));
        }
    }

    // Zona tane (alto)
    for (int y = Y_TANE; y < Y_RIVA; y++) {
        for (int x = 1; x < max_x - 1; x++) {
            wattron(game_win, COLOR_PAIR(COLORE_ERBA));
            mvwaddch(game_win, y, x, ' ');
            wattroff(game_win, COLOR_PAIR(COLORE_ERBA));
        }
    }
}

// POSIZIONAMENTO INIZIALE RANA
static void init_frog_state(void) {
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);
    frog_y = Y_MARCIAPIEDE;
    frog_x = (max_x - FROG_W) / 2;
    if (frog_x < 1) frog_x = 1;
}

// DISEGNO RANA
static void draw_frog(void) {
    wattron(game_win, COLOR_PAIR(COLORE_RANA));
    for (int i = 0; i < FROG_H; i++) {
        mvwaddstr(game_win, frog_y + i, frog_x, frog_shape[i]);
    }
    wattroff(game_win, COLOR_PAIR(COLORE_RANA));
}

// DISEGNO COCCODRILLI (per ora placeholder)
static void draw_crocs(void) {
    wattron(game_win, COLOR_PAIR(COLORE_CROC));
    // Per ora non disegniamo nulla, sarà implementato con processi
    wattroff(game_win, COLOR_PAIR(COLORE_CROC));
}

// DISEGNO PROIETTILI (per ora placeholder)
static void draw_projectiles(void) {
    wattron(game_win, COLOR_PAIR(COLORE_RANA));
    // Per ora non disegniamo nulla, sarà implementato con processi
    wattroff(game_win, COLOR_PAIR(COLORE_RANA));
}

// MAIN FUNCTION - PUNTO DI PARTENZA
int main(void) {
    // INIZIALIZZAZIONE
    init_screen_and_colors();
    center_and_create_game_window();
    init_frog_state();

    // DISEGNO INIZIALE
    draw_background();
    draw_frog();
    wrefresh(game_win);
    napms(1000);  // Pausa per vedere il disegno iniziale

    // QUI INIZIEREMO AD IMPLEMENTARE:
    // 1. Sistema di processi (pipe, fork)
    // 2. Processo rana
    // 3. Processo creatore coccodrilli
    // 4. Processi coccodrilli individuali
    // 5. Sistema proiettili
    // 6. Collisioni
    // 7. UI e punteggio

    // PER ORA: CICLO SEMPLICE CHE RIDISEGNA
    int running = 1;
    while (running) {
        // Input semplice (per ora solo per uscire)
        int ch = wgetch(game_win);
        if (ch == 'q' || ch == 'Q') {
            running = 0;
        }

        // Ridiamo il frame (per ora statico)
        draw_background();
        draw_frog();
        draw_crocs();
        draw_projectiles();
        wrefresh(game_win);

        napms(50);  // ~20 FPS
    }

    // PULIZIA
    if (game_win) delwin(game_win);
    endwin();

    return 0;
}

/*
 * PROSSIMI PASSI DA IMPLEMENTARE:
 * ===============================
 *
 * 1. ✅ Scheletro base con disegno background
 * 2. 🔄 Sistema pipe per comunicazione processi
 * 3. 🔄 Processo rana (movimento con frecce)
 * 4. 🔄 Processo creatore coccodrilli
 * 5. 🔄 Processi coccodrilli individuali
 * 6. 🔄 Sistema proiettili/granate
 * 7. 🔄 Collisioni e morte rana
 * 8. 🔄 UI con vite e tempo
 * 9. 🔄 Sistema punteggio
 * 10. 🔄 Gestione fine partita
 *
 * Il progetto è pronto per essere sviluppato passo dopo passo!
 */
