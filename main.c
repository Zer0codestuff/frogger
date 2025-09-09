/*
  File: main.c
  Scopo: versione a processi del gioco Frogger (padre = disegno/logica, figli = produttori eventi).
  Nota: commenti didattici aggiunti per chiarire le sezioni principali e le righe chiave.
*/
#include <locale.h>      // setlocale per supporto UTF-8 (sprite/caratteri)
#include <ncurses.h>     // grafica testuale: WINDOW, mvwaddstr, colori, getmaxyx, box, wgetch, napms
#include <stdio.h>       // fprintf, perror
#include <stdlib.h>      // exit, malloc/free (eventuale), general-purpose
#include <stdbool.h>     // tipo bool, true/false
#include <string.h>      // strlen per centrare testo schermate finali


#include <unistd.h>      // fork, pipe, read, write, close, usleep, _exit
#include <sys/types.h>   // pid_t (tipo del PID)
#include <sys/wait.h>    // waitpid (gestione processo figlio)
#include <fcntl.h>       // fcntl, O_NONBLOCK (pipe non bloccante), flag file
#include <errno.h>       // errno, EAGAIN, EWOULDBLOCK (gestione lettura non bloccante)
#include <signal.h>      // kill, SIGKILL (terminazione processi)
#include <time.h>        // usleep



// Dimensioni base del campo e rana
#define GAME_WIDTH  101             // larghezza interna dell'area di gioco
#define FROG_H      2 // altezza della rana (in caratteri)
             
#define FROG_W      3               // larghezza della rana (in caratteri)
#define CROC_W           (3 * FROG_W)   // larghezza 3x rana (massimo consentito dai requisiti)
#define CROC_H           (FROG_H)  
// Sprite 3x2, rivolto verso l'alto (caratteri con stessa larghezza)
static const char *frog_shape[FROG_H * FROG_W] = {
    "▙", "▄", "▟",  // prima riga: occhi
    "▛", "▀", "▜"   // seconda riga: corpo (cerchi pieni)
};

// Sprite 2xCROC_W del coccodrillo (destra/sinistra), solo caratteri non-spazio
static const char *croc_sprite_right[CROC_H] = {
    " ___^^__>",
    " /__oo__\\"
};
static const char *croc_sprite_left[CROC_H] = {
    "<___^^__ ",
    "\\__oo__/ "
};

// Layout verticale (rispetta i requisiti)
#define ZONE_TANE_H           (FROG_H)     // altezza della fascia delle tane
#define ZONE_RIVA_H           (FROG_H)     // altezza riva superiore (erba)
#define ZONE_FIUME_H          (8 * FROG_H) // altezza fiume (8 corsie)
#define ZONE_MARCIAPIEDE_H    (FROG_H)     // altezza marciapiede inferiore
#define ZONE_UI_H             3            // altezza area UI in basso

#define GAME_HEIGHT (ZONE_TANE_H + ZONE_RIVA_H + ZONE_FIUME_H + ZONE_MARCIAPIEDE_H + ZONE_UI_H + 2) // +2 per bordo

// Coordinate Y di inizio zone
#define Y_TANE          1                           // riga di inizio zona tane
#define Y_RIVA          (Y_TANE + ZONE_TANE_H)      // riga di inizio riva superiore
#define Y_FIUME         (Y_RIVA + ZONE_RIVA_H)      // riga di inizio fiume
#define Y_MARCIAPIEDE   (Y_FIUME + ZONE_FIUME_H)    // riga di inizio marciapiede
#define Y_UI            (Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H) // riga UI

#define LIVES_START 5                        // numero vite iniziali
#define MANCHE_TIME 60  // secondi per manche

// Colori (allineati a frogger.c)
#define COLORE_RANA            1
#define COLORE_ACQUA           2
#define COLORE_RANA_SU_ACQUA   3
#define COLORE_RANA_SU_ERBA    4
#define COLORE_STRADA          5
#define COLORE_ERBA            6
#define COLORE_TANA            7
#define COLORE_CROC            8
#define COLORE_PROJECTILE      9  // Proiettili: nero su blu

// Colori personalizzati (init_color)
#define DARK_GREEN             16

#define OBJ_RANA 1                          // id messaggio: rana
#define OBJ_CROC         2                  // id messaggio: coccodrillo
#define OBJ_PROJECTILE   4                  // id messaggio: proiettile
#define OBJ_GRENADE      5                  // id messaggio: richiesta sparo granate
#define OBJ_QUIT         3                  // id messaggio: richiesta uscita
#define OBJ_TELEPORT     6                  // id messaggio: richiesta teletrasporto rana
#define N_FLUSSI         8                  // numero di corsie del fiume
         // altezza coccodrillo (uguale alla rana)
// Fattore di velocità globale: maggiore => più lento (moltiplica la sleep)
#define CROC_SPEED       2
#define CROC_SLEEP_US    120000   // base frame delay per coccodrilli
#define CREATOR_SLEEP_US 800000   // spawna molto meno spesso
#define MAX_MSGS_PER_FRAME 512     // limite di messaggi da drenare per frame per evitare starvation
#define GRENADE_COOLDOWN_MS 500    // tempo minimo tra due spari di granata


static int pipe_fds[2];                     // pipe: [0]=read lato padre, [1]=write lato figli

typedef struct msg{                         // messaggio inviato dai figli al padre
    int id;                                 // tipo oggetto/azione
    int x;                                  // coordinata x (o delta per la rana)
    int y;                                  // coordinata y (o delta per la rana)
    int pid;                                // pid del processo mittente
    int x_speed; // velocità orizzontale (0 per rana, direzione per proiettili)
} msg;


static WINDOW *game_win = NULL;             // puntatore alla finestra ncurses di gioco
static WINDOW *bg_win = NULL;               // finestra di background prerenderizzata

// Forward declarations
static bool is_frog_on_croc(int* out_dx);
// Helpers (prototipi) aggiunti per evitare implicit declaration
static void terminate_process(pid_t pid);
static void cleanup_crocs(void);
static void cleanup_projectiles(void);
static void cleanup_pipes(void);
static void handle_frog_collisions(int* running, pid_t* frog_pid, pid_t* creator_pid);
static void draw_game_frame(void);
static void full_cleanup(pid_t frog_pid, pid_t creator_pid);
static void snap_frog_onto_croc_edge_if_partial(void);
static void compute_tane_layout(void);
static bool is_frog_inside_tana_index(int idx);
static bool all_tane_closed(void);
static bool show_end_screen(int result, long long final_score);
static void restart_game(pid_t* frog_pid, pid_t* creator_pid);

// Enum-like costanti per risultato finale
#define END_VICTORY 1
#define END_DEFEAT  2
static int get_remaining_time_sec(void);
static void add_score_for_den(void);
static void add_score_for_timeout(void);
static void add_score_for_death(void);


// Stato logico rana mantenuto dal processo padre (consumatore)
static int frog_x = 0;                      // posizione x della rana (padre)
static int frog_y = 0;                      // posizione y della rana (padre)
static long long last_grenade_ms = -1000000000LL; // ultimo tempo di sparo (ms)

static int lives = LIVES_START;
static time_t manche_start;

static int flussi[N_FLUSSI]; // 0: sx→dx, 1: dx→sx

// Stato e layout delle 5 tane
static int tane_closed[5] = {0, 0, 0, 0, 0};
static int tane_left[5];
static int tane_right[5];

// Sistema punteggio
static long long score = 0;                 // punteggio complessivo
static const int SCORE_DEN_BASE = 100;      // punti base per chiudere una tana
static const int SCORE_TIME_BONUS = 2;      // moltiplicatore sul tempo rimanente
static const int SCORE_TIMEOUT_PENALTY = 10;// penalità per timeout
static const int SCORE_DEATH_PENALTY = 20;  // penalità per morte

// Inizializza direzioni dei flussi (alternanza partendo da un valore casuale)
static void init_flussi(void) {
    int start = rand() % 2;
    for (int i = 0; i < N_FLUSSI; i++) {
        flussi[i] = (i % 2 == 0) ? start : (1 - start);
    }
}



// Inizializza ncurses (schermo, input, cursore) e definisce le coppie di colori
static void init_screen_and_colors(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();

    // Inizializza colori personalizzati
    init_color(DARK_GREEN, 0, 300, 0);  // Dark green (RGB: 0, 300, 0)

    init_pair(COLORE_RANA,          DARK_GREEN,   COLOR_BLACK);  // Rana su marciapiede
    init_pair(COLORE_ACQUA,         COLOR_CYAN,  COLOR_BLUE);
    init_pair(COLORE_RANA_SU_ACQUA, DARK_GREEN,   COLOR_BLUE);   // Rana su acqua
    init_pair(COLORE_RANA_SU_ERBA,  DARK_GREEN,   COLOR_GREEN);  // Rana su erba
    init_pair(COLORE_STRADA,        COLOR_WHITE, COLOR_BLACK);
    init_pair(COLORE_ERBA,          COLOR_BLACK, COLOR_GREEN);
    init_pair(COLORE_TANA,          COLOR_BLACK, COLOR_YELLOW);
    // Coccodrilli verdi (riempiono in verde sopra il fiume)
    init_pair(COLORE_CROC,          COLOR_BLACK, COLOR_GREEN);
    init_pair(COLORE_PROJECTILE,    COLOR_BLACK, COLOR_BLUE);  // Proiettili: nero su blu
}

// Crea la finestra di gioco centrata, abilita tasti speciali e disegna il bordo
static void center_and_create_game_window(void) { // crea e centra la finestra di gioco
    int term_y, term_x;                           // variabili per dimensioni del terminale
    getmaxyx(stdscr, term_y, term_x);             // ottiene righe/colonne del terminale corrente

    int win_h = GAME_HEIGHT;                      // altezza finestra di gioco richiesta
    int win_w = GAME_WIDTH;                       // larghezza finestra di gioco richiesta

    int start_y = (term_y - win_h) / 2;           // calcola y di partenza per centrare verticalmente
    int start_x = (term_x - win_w) / 2;           // calcola x di partenza per centrare orizzontalmente
    if (start_y < 0) start_y = 0;                 // se il terminale è troppo piccolo, non andare sotto 0
    if (start_x < 0) start_x = 0;                 // idem per x

    game_win = newwin(win_h, win_w, start_y, start_x); // crea la nuova finestra ncurses
    if (!game_win) {                              // se la creazione fallisce
        endwin();                                 // ripristina il terminale
        fprintf(stderr, "Terminale troppo piccolo. Richiesti: %dx%d\n", win_h, win_w); // avvisa l'utente
        exit(1);                                  // termina il programma
    }
    keypad(game_win, TRUE);                       // abilita i tasti speciali (frecce, ecc.) per la finestra
    box(game_win, 0, 0);                          // disegna un bordo attorno alla finestra
    // Calcola la posizione orizzontale delle 5 tane in base alla larghezza corrente
    compute_tane_layout();
}

// Disegna un'area rettangolare con un carattere specifico
static void draw_rect_area(WINDOW *win, int start_y, int end_y, int start_x, int end_x, char ch, int color_pair) {
    wattron(win, COLOR_PAIR(color_pair));
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            mvwaddch(win, y, x, ch);
        }
    }
    wattroff(win, COLOR_PAIR(color_pair));
}

// Disegna le tane nella zona superiore
static void draw_tane(WINDOW *win) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    // Base gialla delle tane (spazio riempito nella fascia Y_TANE)
    wattron(win, COLOR_PAIR(COLORE_TANA));
    for (int i = 0; i < 5; ++i) {
        for (int yy = 0; yy < FROG_H; ++yy) {
            for (int xx = tane_left[i]; xx <= tane_right[i]; ++xx) {
                if (xx > 0 && xx < max_x - 1) {
                    mvwaddch(win, Y_TANE + yy, xx, ' ');
                }
            }
        }
    }
    wattroff(win, COLOR_PAIR(COLORE_TANA));

    // Overlay per le tane chiuse (visivo) — riempi con un carattere distintivo
    wattron(win, COLOR_PAIR(COLORE_ERBA));
    for (int i = 0; i < 5; ++i) {
        if (!tane_closed[i]) continue;
        for (int yy = 0; yy < FROG_H; ++yy) {
            for (int xx = tane_left[i]; xx <= tane_right[i]; ++xx) {
                if (xx > 0 && xx < max_x - 1) {
                    mvwaddch(win, Y_TANE + yy, xx, '#');
                }
            }
        }
    }
    wattroff(win, COLOR_PAIR(COLORE_ERBA));
}

// Disegna l'intero sfondo (marciapiede, fiume, riva superiore, zona tane)
static void draw_background(void) {
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);

    // Bordo
    box(game_win, 0, 0);

    // Marciapiede (in basso)
    draw_rect_area(game_win, Y_MARCIAPIEDE, Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H, 1, max_x - 1, ' ', COLORE_STRADA);

    // Fiume (centrale)
    draw_rect_area(game_win, Y_FIUME, Y_MARCIAPIEDE, 1, max_x - 1, '~', COLORE_ACQUA);

    // Riva superiore (erba)
    draw_rect_area(game_win, Y_RIVA, Y_FIUME, 1, max_x - 1, ' ', COLORE_ERBA);

    // Zona Tane (in alto) – base erba
    draw_rect_area(game_win, Y_TANE, Y_RIVA, 1, max_x - 1, ' ', COLORE_ERBA);

    // 5 tane
    draw_tane(game_win);
}

// Disegna lo sfondo dentro una finestra passata (pre-render del layer di background)
static void draw_background_into(WINDOW *w) {
    int max_y, max_x;
    getmaxyx(w, max_y, max_x);

    // Bordo
    box(w, 0, 0);

    // Usa le funzioni riutilizzabili
    draw_rect_area(w, Y_MARCIAPIEDE, Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H, 1, max_x - 1, ' ', COLORE_STRADA);
    draw_rect_area(w, Y_FIUME, Y_MARCIAPIEDE, 1, max_x - 1, '~', COLORE_ACQUA);
    draw_rect_area(w, Y_RIVA, Y_FIUME, 1, max_x - 1, ' ', COLORE_ERBA);
    draw_rect_area(w, Y_TANE, Y_RIVA, 1, max_x - 1, ' ', COLORE_ERBA);

    draw_tane(w);
}

// Calcola i bordi orizzontali (inclusivi) delle 5 tane coerenti con il disegno
static void compute_tane_layout(void) {
    if (!game_win) return;
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);
    for (int i = 0; i < 5; ++i) {
        int tana_center = (max_x / 5) * i + (max_x / 10);
        int left = tana_center - (FROG_W / 2);
        if (left < 1) left = 1;
        int right = left + FROG_W - 1;
        if (right > max_x - 2) right = max_x - 2;
        tane_left[i] = left;
        tane_right[i] = right;
    }
}

// Crea e prerenderizza la finestra di background con le stesse dimensioni/posizione di game_win
static void init_background_layer(void) {
    if (!game_win) return;
    int win_h, win_w; getmaxyx(game_win, win_h, win_w);
    int start_y, start_x; getbegyx(game_win, start_y, start_x);
    bg_win = newwin(win_h, win_w, start_y, start_x);
    if (bg_win) {
        draw_background_into(bg_win);
    }
}

// Tempo rimanente della manche (in secondi, clamp a [0, MANCHE_TIME])
static int get_remaining_time_sec(void) {
    int elapsed = (int)(time(NULL) - manche_start);
    if (elapsed < 0) elapsed = 0;
    if (elapsed > MANCHE_TIME) elapsed = MANCHE_TIME;
    return MANCHE_TIME - elapsed;
}

// Aggiunge punteggio quando si chiude una tana: base + bonus tempo
static void add_score_for_den(void) {
    int rem = get_remaining_time_sec();
    long long add = SCORE_DEN_BASE + (long long)rem * SCORE_TIME_BONUS;
    if (add < 0) add = 0;
    score += add;
}

// Penalità leggera per timeout
static void add_score_for_timeout(void) {
    score -= SCORE_TIMEOUT_PENALTY;
    if (score < 0) score = 0;
}

// Penalità per morte (acqua o proiettile)
static void add_score_for_death(void) {
    score -= SCORE_DEATH_PENALTY;
    if (score < 0) score = 0;
}

// Gestisce i bounds della rana per evitare che esca dalla finestra
static void clamp_frog_position(void) {
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);

    // Limiti orizzontali
    if (frog_x < 1) frog_x = 1;
    if (frog_x + FROG_W > max_x - 1) frog_x = (max_x - 1) - FROG_W;

    // Limiti verticali (con logica specifica per ogni zona)
    if (frog_y < Y_TANE) frog_y = Y_TANE;

    int floor_y = Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H - FROG_H;
    if (frog_y > floor_y) frog_y = floor_y;
}

// Applica il movimento alla rana e controlla i bounds
static void move_frog(int dx, int dy) {
    frog_x += dx;
    frog_y += dy;
    clamp_frog_position();
}

// Posiziona la rana all'avvio sul marciapiede, centrata orizzontalmente
static void init_frog_state(void) {
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);
    frog_y = Y_MARCIAPIEDE;                 // parte sul marciapiede
    frog_x = (max_x - FROG_W) / 2;          // centrata
    clamp_frog_position();                  // applica bounds
}

// Determina il colore appropriato per la rana basato sulla zona
static int get_frog_color_pair(void) {
    int pair = COLORE_RANA; // colore default (strada)

    // Controlla se la rana è su un coccodrillo (ha priorità su tutto)
    int temp_dx = 0;
    bool on_croc = is_frog_on_croc(&temp_dx);
    if (on_croc) {
        pair = COLORE_RANA_SU_ERBA; // usa verde per indicare sicurezza
    } else if (frog_y >= Y_FIUME && frog_y < Y_MARCIAPIEDE) { // se è nel fiume (ma non su croc)
        pair = COLORE_RANA_SU_ACQUA; // usa colore rana su acqua
    } else if (frog_y >= Y_RIVA && frog_y < Y_FIUME) { // se è sulla riva/erba
        pair = COLORE_RANA_SU_ERBA; // usa colore rana su erba
    } else if (frog_y >= Y_TANE && frog_y < Y_RIVA) { // se è nella fascia tane
        pair = COLORE_RANA_SU_ERBA; // stesso colore erba
    }

    return pair;
}

// Disegna la rana con il colore appropriato per la zona (con caratteri speciali)
static void draw_frog(void) {
    int pair = get_frog_color_pair();

    wattron(game_win, COLOR_PAIR(pair));
    for (int i = 0; i < FROG_H; ++i) {
        for (int j = 0; j < FROG_W; ++j) {
            int index = i * FROG_W + j;  // calcola l'indice nell'array lineare
            const char *ch = frog_shape[index];
            if (ch[0] != ' ') {  // non disegnare spazi
                mvwprintw(game_win, frog_y + i, frog_x + j, "%s", ch);
            }
        }
    }
    wattroff(game_win, COLOR_PAIR(pair));
}

// Stato semplice dei coccodrilli e dichiarazioni (devono venire prima dell'uso)
typedef struct {
    int in_use;   // 0 = slot libero, 1 = occupato
    pid_t pid;    // pid del processo coccodrillo
    int x;        // posizione corrente (assoluta)
    int y;        // posizione corrente (assoluta)
    int x_speed;  // velocità orizzontale del coccodrillo
    int has_pos;  // 0 se non ha ancora una posizione valida
    int dx_frame; // delta x accumulato in questo frame (per riding rana)
} CrocState;

#define MAX_CROCS 16
static CrocState crocs[MAX_CROCS];

// Struttura per i proiettili (usata anche per le granate della rana)
typedef struct {
    int in_use;     // Slot attivo?
    pid_t pid;      // PID del processo proiettile
    int x, y;       // Posizione
    int direction;  // Direzione (-1 = sinistra, +1 = destra)
    int id;         // OBJ_PROJECTILE (coccodrillo) oppure OBJ_GRENADE (granata rana)
} ProjectileState;

#define MAX_PROJECTILES 32
static ProjectileState projectiles[MAX_PROJECTILES];

// Forward declaration
static CrocState* get_croc_slot(pid_t pid);
static ProjectileState* get_projectile_slot(pid_t pid);

// Disegna coccodrilli (stile semplice, come blocchi pieni) — simile a frogger_ultimate
// Disegna i coccodrilli come blocchi pieni (3x la larghezza della rana)
static void draw_crocs(void) {
    int max_y, max_x;                                  // dimensioni finestra di gioco
    getmaxyx(game_win, max_y, max_x);                  // ottieni righe/colonne

    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) continue;               // salta slot vuoti
        const char **sprite = (crocs[i].x_speed > 0) ? croc_sprite_right : croc_sprite_left;
        wattron(game_win, COLOR_PAIR(COLORE_CROC));
        for (int yy = 0; yy < CROC_H; yy++) {
            for (int xx = 0; xx < CROC_W && sprite[yy][xx] != '\0'; xx++) {
                char ch = sprite[yy][xx];
                if (ch == ' ') continue;              // non disegnare spazi
                int px = crocs[i].x + xx;             // x del carattere da disegnare
                int py = crocs[i].y + yy;             // y del carattere da disegnare
                if (px >= 1 && px < max_x - 1 && py >= 1 && py < max_y - 1) { // dentro i bordi
                    // Usa mvwaddstr per caratteri multibyte UTF-8
                    char temp[2] = {ch, '\0'};
                    mvwaddstr(game_win, py, px, temp);
                }
            }
        }
        wattroff(game_win, COLOR_PAIR(COLORE_CROC));
    }
}

// Disegna i proiettili come piccoli punti neri su sfondo blu
static void draw_projectiles(void) {
    wattron(game_win, COLOR_PAIR(COLORE_PROJECTILE)); // nero su blu
    int max_y, max_x; getmaxyx(game_win, max_y, max_x);
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].in_use) continue;
        int px = projectiles[i].x;
        int py = projectiles[i].y;
        if (px >= 1 && px < max_x - 1 && py >= 1 && py < max_y - 1) {
            // Caratteri speciali: 𖦹 per granate rana, ► per proiettili coccodrilli
            const char *ch = (projectiles[i].id == OBJ_GRENADE) ? "◆" : "►";
            mvwaddstr(game_win, py, px, ch);
        }
    }
    wattroff(game_win, COLOR_PAIR(COLORE_PROJECTILE));
}

// Libera gli slot dei coccodrilli fuori dallo schermo (evita saturazione di slot)
// Libera gli slot dei coccodrilli che sono usciti dai bordi della finestra
static void sweep_crocs_offscreen(void) {
    int max_y, max_x;                           // dimensioni finestra di gioco
    getmaxyx(game_win, max_y, max_x);           // ottieni righe/colonne
    int left_edge = 1;                          // bordo interno sinistro (+1 per bordo finestra)
    int right_edge = max_x - 2;                 // bordo interno destro (-1 per bordo, -1 indice)
    for (int i = 0; i < MAX_CROCS; i++) {       // scorri tutti gli slot
        if (!crocs[i].in_use) continue;         // salta gli slot liberi
        // se il processo del coccodrillo è terminato, libera lo slot
        if (crocs[i].pid > 0) {
            if (kill(crocs[i].pid, 0) == -1 && errno == ESRCH) {
                crocs[i].in_use = 0;
                crocs[i].pid = -1;
                continue;
            }
        }

        // Libera se completamente fuori dalla finestra interna
        if (crocs[i].x > right_edge || (crocs[i].x + CROC_W - 1) < left_edge) {
            crocs[i].in_use = 0;
            crocs[i].pid = -1;
            continue;
        }

        // Caso di uscita imminente: il figlio ha già incrementato e terminato,
        // l'ultimo x ricevuto è ancora al bordo. Considera off-screen ora.
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

// Libera gli slot dei proiettili fuori schermo o terminati
static void sweep_projectiles_offscreen(void) {
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);
    int left_edge = 1;
    int right_edge = max_x - 2;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].in_use) continue;

        // Se il processo del proiettile è terminato, libera lo slot
        if (projectiles[i].pid > 0) {
            if (kill(projectiles[i].pid, 0) == -1 && errno == ESRCH) {
                projectiles[i].in_use = 0;
                projectiles[i].pid = -1;
                continue;
            }
        }

        // Libera se completamente fuori schermo (consenti x==right_edge come ultimo visibile)
        if (projectiles[i].x < left_edge || projectiles[i].x > right_edge) {
            projectiles[i].in_use = 0;
            projectiles[i].pid = -1;
            continue;
        }

        // Se il processo è terminato, libera subito lo slot
        if (projectiles[i].pid > 0) {
            if (kill(projectiles[i].pid, 0) == -1 && errno == ESRCH) {
                projectiles[i].in_use = 0;
                projectiles[i].pid = -1;
                continue;
            }
        }
    }
}

// Verifica se la rana è sopra un coccodrillo. Se sì, restituisce true e
// scrive in out_dx la velocità orizzontale del coccodrillo (per "riding").
// Rileva se la rana è interamente appoggiata su un coccodrillo della stessa riga
static bool is_frog_on_croc(int* out_dx) {
    if (out_dx) *out_dx = 0;                 // azzera l'output se fornito

    int frog_left  = frog_x;                 // bordo sinistro della rana
    int frog_right = frog_x + FROG_W - 1;    // bordo destro della rana

    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) continue;     // ignora slot vuoti

        // stessa riga/flow: croc e rana hanno la stessa altezza (FROG_H)
        if (frog_y != crocs[i].y) continue; // deve stare sulla stessa riga (flusso)

        int croc_left  = crocs[i].x;        // bordo sinistro del coccodrillo
        int croc_right = crocs[i].x + CROC_W - 1; // bordo destro del coccodrillo

        // la rana deve essere almeno parzialmente appoggiata sul coccodrillo
        if (frog_left <= croc_right && frog_right >= croc_left) { // overlap orizzontale
            if (out_dx) *out_dx = crocs[i].dx_frame; // restituisce il delta del frame del croc
            return true;                            // conferma che è sopra
        }
    }
    return false;
}

// Se la rana è parzialmente appoggiata a un coccodrillo, spostala sul bordo di salita
static void snap_frog_onto_croc_edge_if_partial(void) {
    // La rana deve essere nella fascia fiume per applicare lo snap
    if (!(frog_y >= Y_FIUME && frog_y < Y_MARCIAPIEDE)) return;

    int frog_left  = frog_x;
    int frog_right = frog_x + FROG_W - 1;

    for (int i = 0; i < MAX_CROCS; i++) {
        if (!crocs[i].in_use) continue;
        if (crocs[i].y != frog_y) continue; // stesso flusso

        int croc_left  = crocs[i].x;
        int croc_right = crocs[i].x + CROC_W - 1;

        // Verifica sovrapposizione orizzontale parziale (qualche colonna in comune)
        bool overlap = (frog_left <= croc_right && frog_right >= croc_left);
        if (!overlap) continue;

        int overlap_left  = (frog_left   > croc_left)  ? frog_left  : croc_left;
        int overlap_right = (frog_right  < croc_right) ? frog_right : croc_right;
        int overlap_w = overlap_right - overlap_left + 1;

        // Se overlap è tra 1 e FROG_W-1 -> parziale; se == FROG_W è completo
        if (overlap_w <= 0 || overlap_w >= FROG_W) continue;

        // Decidi da quale bordo è salita la rana e allineala a quel bordo del coccodrillo
        if (frog_right > croc_right && frog_left <= croc_right) {
            // Entrata da destra: snappa il bordo sinistro della rana al bordo destro del croc - (FROG_W-1)
            frog_x = croc_right - (FROG_W - 1);
            clamp_frog_position();
            return;
        }
        if (frog_left < croc_left && frog_right >= croc_left) {
            // Entrata da sinistra: snappa il bordo sinistro della rana al bordo sinistro del croc
            frog_x = croc_left;
            clamp_frog_position();
            return;
        }
    }
}

// Verifica se la rana tocca (anche parzialmente) la tana con indice idx
static bool is_frog_inside_tana_index(int idx) {
    int frog_left  = frog_x;
    int frog_right = frog_x + FROG_W - 1;
    int frog_top   = frog_y;
    int frog_bottom= frog_y + FROG_H - 1;

    // Deve trovarsi nella fascia verticale delle tane
    if (frog_top != Y_TANE || frog_bottom != (Y_TANE + FROG_H - 1)) return false;

    // Overlap orizzontale (anche parziale) con i bordi della tana
    int left = tane_left[idx];
    int right = tane_right[idx];
    return (frog_left <= right && frog_right >= left);
}

// True se tutte e 5 le tane risultano chiuse
static bool all_tane_closed(void) {
    for (int i = 0; i < 5; ++i) {
        if (!tane_closed[i]) return false;
    }
    return true;
}

// Verifica collisioni tra proiettili e rana
static bool check_projectile_collision(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].in_use) continue;
        // Considera solo i proiettili dei coccodrilli per danneggiare la rana
        if (projectiles[i].id != OBJ_PROJECTILE) continue;

        // Collisione semplice: se il proiettile è nella stessa posizione della rana
        if (projectiles[i].x >= frog_x && projectiles[i].x < frog_x + FROG_W &&
            projectiles[i].y >= frog_y && projectiles[i].y < frog_y + FROG_H) {
            // Collisione! Rimuovi il proiettile
            projectiles[i].in_use = 0;
            if (projectiles[i].pid > 0) {
                terminate_process(projectiles[i].pid);
                projectiles[i].pid = -1;
            }
            return true; // collisione avvenuta
        }
    }
    return false; // nessuna collisione
}

// (Rimossa) Le granate usano ora gli slot dei proiettili

// Verifica collisioni granata vs proiettile: se collisione, rimuovi entrambi
static void check_grenade_vs_projectile(void) {
    // Collisione tra proiettili rana (OBJ_GRENADE) e proiettili coccodrillo (OBJ_PROJECTILE)
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].in_use || projectiles[i].id != OBJ_GRENADE) continue;
        for (int j = 0; j < MAX_PROJECTILES; j++) {
            if (!projectiles[j].in_use || projectiles[j].id != OBJ_PROJECTILE) continue;
            if (projectiles[i].y == projectiles[j].y) {
                int gx = projectiles[i].x;
                int px = projectiles[j].x;
                int gd = projectiles[i].direction;
                int pd = projectiles[j].direction;

                bool same_cell = (gx == px);
                bool crossing  = (abs(gx - px) == 1) && (gd != 0) && (pd != 0) && (gd == -pd);
                if (same_cell || crossing) {
                    if (projectiles[i].pid > 0) {
                        terminate_process(projectiles[i].pid);
                        projectiles[i].pid = -1;
                    }
                    projectiles[i].in_use = 0;

                    if (projectiles[j].pid > 0) {
                        terminate_process(projectiles[j].pid);
                        projectiles[j].pid = -1;
                    }
                    projectiles[j].in_use = 0;
                }
            }
        }
    }
}

// Trova uno slot per un pid esistente, altrimenti ne alloca uno nuovo
// Restituisce lo slot associato al pid del coccodrillo, o ne crea uno nuovo
static CrocState* get_croc_slot(pid_t pid) {
    // cerca se esiste già
    for (int i = 0; i < MAX_CROCS; i++) {
        if (crocs[i].in_use && crocs[i].pid == pid) {
            return &crocs[i];
        }
    }
    // altrimenti trova uno slot libero
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
    return NULL; // nessuno slot disponibile (caso limite)
}

static ProjectileState* get_projectile_slot(pid_t pid) {
    // cerca se esiste già
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].in_use && projectiles[i].pid == pid) {
            return &projectiles[i];
        }
    }
    // altrimenti trova uno slot libero
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
    return NULL; // nessuno slot disponibile (caso limite)
}



// Calcolo y del flusso (0 vicino al marciapiede, 7 vicino alla riva) — stile ultimate
// Converte l'indice del flusso nella coordinata Y corrispondente
static int flow_to_y(int flow_index) {
    return Y_MARCIAPIEDE - ((flow_index + 1) * FROG_H); // ogni flusso è alto quanto la rana
}

// Velocità per flusso: array inizializzato all'avvio partita (più varietà ma costante per flusso)
static int flow_speeds[N_FLUSSI];

// Inizializza velocità orizzontali per ogni flusso con una distribuzione semplice
static void init_flow_speeds_random(void) {
    // Distribuzione lenta in generale: più probabile 1, poi 2, raramente 3
    int choices[6] = {1,1,1,2,2,3};
    for (int i = 0; i < N_FLUSSI; i++) {
        flow_speeds[i] = choices[rand() % 6];
    }
}

// Tempo corrente in millisecondi (monotonic clock)
static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
}

// Processo singolo proiettile
static void projectile_process(int write_fd, int start_x, int start_y, int direction, int msg_id) {
    close(pipe_fds[0]); // chiude read-end non usata

    msg m;
    m.id = msg_id;                  // può essere OBJ_PROJECTILE o OBJ_GRENADE
    m.pid = getpid();
    m.x_speed = direction;          // comunica al padre la direzione

    int x = start_x;
    int y = start_y;
    int left_edge = 1;
    int right_edge = GAME_WIDTH - 2;

    // imposta write non-bloccante per evitare blocchi se la pipe è piena
    int wfl = fcntl(write_fd, F_GETFL, 0);
    if (wfl != -1) fcntl(write_fd, F_SETFL, wfl | O_NONBLOCK);

    while (1) {
        // Non inviare coordinate fuori schermo: consenti l'ultima colonna visibile (x == right_edge)
        if (x < left_edge || x > right_edge) {
            break;
        }

        m.x = x;
        m.y = y;
        ssize_t wr = write(write_fd, &m, sizeof(m));
        if (wr < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(2000);
                continue;
            } else {
                break;
            }
        }

        x += direction; // proiettili si muovono solo orizzontalmente

        usleep(50000); // movimento più veloce dei coccodrilli
    }

    close(write_fd);
    _exit(0);
}

// Rimossa la funzione grenade_process - le granate sono gestite direttamente dal padre

// Processo singolo coccodrillo: invia posizioni assolute (come in frogger_ultimate)
// Processo figlio: muove un singolo coccodrillo e invia posizioni assolute al padre
static void croc_process(int write_fd, int flow_index) {
    // il figlio coccodrillo non usa il lato di lettura della pipe
    close(pipe_fds[0]);                                // chiude la read-end (non serve qui)
    int dir = (flussi[flow_index] == 0) ? +1 : -1;     // 0: sx→dx, 1: dx→sx (sceglie direzione)
    int y = flow_to_y(flow_index);                     // riga Y del flusso scelto
    // Non usare ncurses nel figlio. Usiamo la larghezza della finestra di gioco.
    int left_edge = 1;                                 // bordo interno sinistro
    int right_edge = GAME_WIDTH - 2;                   // bordo interno destro

    int x = (dir > 0) ? (left_edge - CROC_W) : right_edge; // posizione iniziale appena fuori dallo schermo
    // velocità orizzontale (passi per frame): scala con il fattore globale
    int base = flow_speeds[flow_index] > 0 ? flow_speeds[flow_index] : 1; // garantisce >=1
    int speed = base;                                   // i passi per frame restano base

    msg m; m.id = OBJ_CROC; m.pid = getpid(); m.x_speed = dir * speed; // prepara il messaggio da inviare
    int shoot_cooldown = 0; // cooldown per evitare spari troppo frequenti

    while (1) {                                        // ciclo di vita del coccodrillo
        m.x = x; m.y = y;                              // aggiorna coordinate da inviare al padre
        write(write_fd, &m, sizeof(m));                // invia messaggio sulla pipe

        // Logica di sparo casuale
        if (shoot_cooldown <= 0) {
            int shoot_chance = rand() % 100; // probabilità 1 su 100 per frame
            if (shoot_chance < 5) { // ~5% di probabilità di sparo (per testing)
                pid_t projectile_pid = fork();
                if (projectile_pid == 0) {
                    // Processo proiettile: spara da una cella ESTERNA al corpo del coccodrillo
                    int projectile_x = (dir > 0) ? (x + CROC_W) : (x - 1);
                    projectile_process(write_fd, projectile_x, y, dir, OBJ_PROJECTILE);
                } else if (projectile_pid > 0) {
                    // Processo coccodrillo (padre) - continua normalmente
                    shoot_cooldown = 30; // cooldown di 30 frame (~0.5 secondi)
                } else {
                    // Errore nel fork
                    perror("fork projectile");
                }
            }
        } else {
            shoot_cooldown--;
        }

        x += dir * speed;                              // avanza orizzontalmente
        if ((dir > 0 && x > right_edge) ||             // se è uscito a destra
            (dir < 0 && x + CROC_W < left_edge))       // o completamente a sinistra
            break;                                     // termina il loop
        // rallenta in base al fattore globale CROC_SPEED
        usleep(CROC_SLEEP_US * CROC_SPEED);            // pausa tra un frame e l'altro

        // Reap non bloccante dei figli proiettile per evitare zombie
        // (in modo che il processo padre possa rilevare correttamente la loro terminazione)
        pid_t zr;
        while ((zr = waitpid(-1, NULL, WNOHANG)) > 0) {
            // niente: solo raccolta
        }
    }
    close(write_fd);                                   // chiude la write-end prima di uscire
    _exit(0);                                          // termina processo figlio
}

// Processo creatore di coccodrilli (come in frogger_ultimate)
// Processo figlio creatore: spawna periodicamente nuovi coccodrilli
static void croc_creator(int write_fd) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());       // inizializza RNG per il creatore
    init_flussi();                                           // imposta direzioni dei flussi
    init_flow_speeds_random();                               // imposta velocità iniziali dei flussi
    // chiude il lato di lettura: il creatore non legge dalla pipe
    close(pipe_fds[0]);                                      // chiude read-end non usata
    int last_flow = -1;                                      // ricorda l'ultimo flusso usato
    int last_last_flow = -1;                                 // penultimo flusso usato
    int last_last_last_flow = -1;                            // terzultimo flusso usato
    int active_crocs = 0;                                    // numero di coccodrilli attivi
    const int MAX_ACTIVE_CROCS = 16;                         // limite massimo per non saturare
    while (1) {                                              // ciclo infinito di spawn
        // se troppi coccodrilli sono attivi, aspetta e riprova
        if (active_crocs >= MAX_ACTIVE_CROCS) {              // controllo limite
            pid_t rpid;                                      // variabile per waitpid non bloccante
            while ((rpid = waitpid(-1, NULL, WNOHANG)) > 0) { // raccogli figli terminati
                if (active_crocs > 0) active_crocs--;         // decrementa numero attivi
            }
            usleep(250000); // 250 ms                         // attesa prima di riprovare
            continue;                                         // ricomincia ciclo
        }
        // Scegli un flusso casuale tra 0 e N_FLUSSI-1
        int flow = rand() % N_FLUSSI; // 0..7
    
        // Controlla anche il terzultimo flusso usato per evitare ripetizioni
        // Dobbiamo tenere traccia di last_last_last_flow, last_last_flow, last_flow
        // Se il flusso scelto è uguale a uno degli ultimi tre, aspetta e riprova

        if (flow == last_flow || flow == last_last_flow || flow == last_last_last_flow) {
            usleep(CREATOR_SLEEP_US); // attende un po' prima di riprovare
            continue;                 // salta questo ciclo e riprova
        }

        // Aggiorna la memoria dei flussi usati: sposta indietro la storia
        last_last_last_flow = last_last_flow; // il terzultimo diventa il quartultimo
        last_last_flow = last_flow;           // il penultimo diventa il terzultimo
        last_flow = flow;                     // aggiorna l'ultimo flusso usato
        // memorizza scelta

        pid_t pid = fork();                                   // crea un figlio coccodrillo
        if (pid == 0) {
            // Figlio coccodrillo: invia posizioni e termina a fine corsa
            croc_process(write_fd, flow);                     // esegue logica coccodrillo
        } else if (pid > 0) {
            active_crocs++;                                   // incrementa contatore attivi
        } else {
            // Errore nel fork
            perror("fork croc");
        }
        // reap non bloccante dei figli terminati per evitare zombie e aggiornare conteggio
        pid_t rpid;                                          // variabile per waitpid non bloccante
        while ((rpid = waitpid(-1, NULL, WNOHANG)) > 0) {    // raccogli eventuali terminati
            if (active_crocs > 0) active_crocs--;            // aggiorna contatore
        }
        // spawn più rado: tra 0.8s e 1.6s circa
        int extra = (rand() % 800) * 1000; // 0..800ms        // jitter casuale
        usleep(800000 + extra);                               // attesa prima di un nuovo spawn
    }
}
// Processo figlio: legge l'input del giocatore e invia delta movimento al padre
static void frog_process(int write_fd, int start_x, int start_y) {
    // Usa ncurses getch con KEY_* (stile frogger_ultimate). Niente disegno nel figlio.
    keypad(stdscr, TRUE);                 // abilita tasti speciali su stdscr
    nodelay(stdscr, TRUE);                // getch non blocca (ritorna subito)

    msg m;                                // struttura messaggi verso il padre
    m.id  = OBJ_RANA;                     // indica che il messaggio viene dalla rana
    m.pid = getpid();                     // salva pid del processo figlio
    m.x   = 0;                            // delta x da inviare (inizialmente 0)
    m.y   = 0;                            // delta y da inviare (inizialmente 0)

    // Latch: invia la richiesta di granate solo al fronte di pressione (key down)
    int space_latch = 0;                  // 0 = rilasciato, 1 = tenuto premuto
    int i_latch = 0;                      // evita ripetizione teletrasporto

    while (1) {
        int input = getch();              // legge l'ultimo tasto premuto (o -1)
        int dx = 0, dy = 0;               // delta di movimento da calcolare

        if (input == 'q' || input == 'Q') { // richiesta uscita
            // invia un messaggio di quit al padre prima di terminare
            m.id = OBJ_QUIT;                // cambia tipo messaggio in QUIT
            m.x = 0; m.y = 0; m.x_speed = 0; // azzera i campi di movimento
            write(write_fd, &m, sizeof(m));  // invia la richiesta
            break;                           // esce dal ciclo (termina il figlio)
        }
        else if (input == KEY_UP)    dy = -FROG_H;   // salta di una altezza rana verso l'alto
        else if (input == KEY_DOWN)  dy = +FROG_H;   // salta verso il basso
        else if (input == KEY_LEFT)  dx = -FROG_W;   // salta a sinistra (larghezza rana)
        else if (input == KEY_RIGHT) dx = +FROG_W;   // salta a destra (larghezza rana)
        else if (input == ' ' && space_latch == 0) {
            // Invia solo una richiesta al padre: sarà lui a forcare i 2 proiettili granata
            m.id = OBJ_GRENADE;
            m.x = frog_x;       // passa la posizione corrente della rana al padre
            m.y = frog_y;
            m.x_speed = 0;
            write(write_fd, &m, sizeof(m));
            space_latch = 1;              // evita richieste ripetute finché resta premuto
            dx = 0; dy = 0;
        }
        else if (input == 'i' && i_latch == 0) {
            // Richiesta di teletrasporto alla riva superiore
            m.id = OBJ_TELEPORT;
            m.x = 0; m.y = 0; m.x_speed = 0;
            write(write_fd, &m, sizeof(m));
            i_latch = 1;
            dx = 0; dy = 0;
        }

        if (dx != 0 || dy != 0) {        // se c'è un movimento da inviare
            m.id = OBJ_RANA;             // imposta tipo messaggio per movimento
            m.x = dx;                     // imposta delta x
            m.y = dy;                     // imposta delta y
            m.x_speed = 0;                // velocità non usata per la rana
            write(write_fd, &m, sizeof(m)); // invia messaggio di movimento
        }

        // Se la barra spaziatrice non è attualmente premuta, sblocca il latch
        if (input != ' ') space_latch = 0;
        if (input != 'i') i_latch = 0;

        usleep(30000);
    }

    close(write_fd);                      // chiude la write-end della pipe
    _exit(0);                             // termina il processo figlio
}


// Reimposta timer e riposiziona la rana per l'inizio di una nuova manche
static void start_new_manche(void) {
    manche_start = time(NULL);           // aggiorna l'orologio d'inizio manche
    int max_y, max_x;                    // dimensioni finestra
    getmaxyx(game_win, max_y, max_x);    // ottieni righe/colonne
    frog_y = Y_MARCIAPIEDE;              // piazza la rana sul marciapiede
    frog_x = (max_x - FROG_W) / 2;       // centra la rana orizzontalmente
    if (frog_x < 1) frog_x = 1;          // evita che tocchi il bordo sinistro

    // Le granate sono processi separati, non oggetti da resettare
}


// Disegna l'interfaccia: vite e tempo rimanente della manche
static void draw_ui(void) {
    int max_y, max_x;                         // dimensioni finestra
    getmaxyx(game_win, max_y, max_x);         // ottieni righe/colonne

    int y = Y_UI; // riga base UI             // riga a cui stampare la UI

    // tempo rimanente
    int elapsed = (int)(time(NULL) - manche_start); // quanti secondi sono passati
    if (elapsed < 0) elapsed = 0;                   // clamp inferiore
    if (elapsed > MANCHE_TIME) elapsed = MANCHE_TIME; // clamp superiore
    int remaining = MANCHE_TIME - elapsed;          // tempo rimanente

    // vite
    mvwprintw(game_win, y, 2, "LIVES: ");          // stampa etichetta vite
    int cx = 10;                                     // colonna corrente
    for (int i = 0; i < lives; i++) {                // stampa emoji rana per ogni vita
        mvwprintw(game_win, y, cx, "🐸 ");          // emoji rana
        cx += 3;                                     // sposta a destra
    }

    // punteggio
    mvwprintw(game_win, y, 40, "SCORE: %lld", score);

    // barra tempo
    int bar_w = 30;                                  // larghezza barra in caratteri
    int filled = (remaining * bar_w) / MANCHE_TIME;  // porzione riempita proporzionalmente
    if (filled < 0) filled = 0;                      // clamp inferiore
    if (filled > bar_w) filled = bar_w;              // clamp superiore

    mvwprintw(game_win, y + 1, 2, "TIME: [");       // inizio barra
    for (int i = 0; i < bar_w; i++) {                // disegna la barra
        char c = (i < filled) ? '=' : ' ';           // riempito o vuoto
        mvwaddch(game_win, y + 1, 9 + i, c);         // stampa il carattere
    }
    mvwprintw(game_win, y + 1, 9 + bar_w, "] %ds", remaining); // fine barra + valore numerico
}


// Inizializza tutte le strutture dati del gioco
static void init_game_data(void) {
    // Generatore numeri casuali per il processo padre
    srand(time(NULL));

    // Stato iniziale della rana
    init_frog_state();

    // Le granate ora sono processi, non oggetti da inizializzare
}

// Inizializza tutto il sistema di gioco
static void init_game_system(void) {
    init_screen_and_colors();                   // ncurses e colori
    center_and_create_game_window();            // finestra di gioco
    init_background_layer();                    // background prerenderizzato
}

// Processo padre: setup, fork dei figli, ciclo di gioco e pulizia finale
int main(void) {                                // entry point del processo padre (gioco)
    init_game_system();                         // inizializza tutto il sistema

// Dimensioni interne finestra (servono per clamp)
int max_y, max_x;                               // dimensioni finestra
getmaxyx(game_win, max_y, max_x);               // ottiene righe/colonne

// Crea pipe per comunicazione padre<-figli (non bloccante lato lettura)
if (pipe(pipe_fds) == -1) {                     // crea pipe (padre legge, figli scrivono)
    endwin(); perror("pipe"); return 1;        // errore: chiudi ncurses ed esci
}

// Calcola posizione iniziale (passala pure anche se il figlio non la usa ora)
int start_rx = (max_x - FROG_W) / 2;            // x iniziale rana (centrata)
int start_ry = Y_MARCIAPIEDE;                   // y iniziale rana (marciapiede)

// Fork
pid_t frog_pid = fork();
if (frog_pid < 0) { endwin(); perror("fork"); return 1; }

if (frog_pid == 0) {
    // FIGLIO (produttore)
    close(pipe_fds[0]); // chiude read end
    // Evita side-effect ncurses dal figlio come in frogger_ultimate
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
    }
    frog_process(pipe_fds[1], start_rx, start_ry);
    _exit(0);
}

// Fork del creatore (come in frogger_ultimate): usa la stessa write-end
pid_t creator_pid = fork();
if (creator_pid < 0) { endwin(); perror("fork"); return 1; }
if (creator_pid == 0) {
    // processo creatore: non usare ncurses, invia solo su pipe
    croc_creator(pipe_fds[1]);
    _exit(0);
}

// PADRE (consumatore): manteniamo aperto il lato di scrittura, così i figli
// granata creati dal padre potranno scrivere sulla pipe.

// 2. Otteniamo i flag correnti del file descriptor della pipe (lato lettura).
//    Questo serve per non sovrascrivere altri flag già impostati.
int fl = fcntl(pipe_fds[0], F_GETFL, 0);          // leggi flag correnti

// 3. Impostiamo il flag O_NONBLOCK sul file descriptor della pipe.
//    In questo modo, le operazioni di lettura (read) su questa pipe saranno non bloccanti:
//    - Se non ci sono dati disponibili, read restituirà subito -1 e imposterà errno a EAGAIN/EWOULDBLOCK
//    - Questo è utile per non bloccare il ciclo principale del gioco in attesa di dati dal figlio.
fcntl(pipe_fds[0], F_SETFL, fl | O_NONBLOCK);     // abilita modalità non bloccante

    // Inizializza tutte le strutture dati del gioco
    init_game_data();

    start_new_manche();

// Disegno iniziale per vedere subito la rana
if (bg_win) {
    copywin(bg_win, game_win, 0, 0, 0, 0, GAME_HEIGHT - 1, GAME_WIDTH - 1, FALSE);
} else {
    draw_background();                           // fallback
}
draw_frog();                                     // rana al centro
draw_ui();                                       // UI con vite/tempo
wrefresh(game_win);                              // mostra
napms(800);                                      // piccola pausa

int running = 1;

while (running) {

    int elapsed = (int)(time(NULL) - manche_start);
if (elapsed >= MANCHE_TIME) {
    add_score_for_timeout();
    lives--;
    if (lives <= 0) {
        bool again = show_end_screen(END_DEFEAT, score);
        if (again) {
            restart_game(&frog_pid, &creator_pid);
        } else {
            running = 0; // fine gioco
        }
    } else {
        start_new_manche();
    }
}

    // Accumula input rana per applicarlo dopo il riding
    int acc_dx = 0;
    int acc_dy = 0;

    // Azzera i delta di movimento per frame dei coccodrilli
    for (int i = 0; i < MAX_CROCS; i++) {
        if (crocs[i].in_use) crocs[i].dx_frame = 0;
    }

    // Dreniamo i messaggi dalla pipe con un limite per frame per evitare starvation
    for (int drained = 0; drained < MAX_MSGS_PER_FRAME; drained++) {
        msg m; // Alloco una variabile di tipo msg per ricevere il messaggio dalla pipe.
        ssize_t n = read(pipe_fds[0], &m, sizeof(m)); // Leggo dalla pipe (lato lettura) un messaggio di dimensione msg.
        if (n > 0) { // Se ho letto effettivamente dei dati (n > 0)...
            if (m.id == OBJ_RANA) { // Se il messaggio riguarda la rana...
                // Accumula il movimento (applicheremo dopo il riding)
                acc_dx += m.x;
                acc_dy += m.y;
            } else if (m.id == OBJ_TELEPORT && m.pid == frog_pid) {
                // Teletrasporto: porta la rana sulla riva superiore (erba, sotto le tane)
                frog_y = Y_RIVA; // riga della riva superiore
                // Mantieni la x corrente e applica i bounds
                if (frog_x < 1) frog_x = 1;
                if (frog_x + FROG_W > max_x - 1) frog_x = (max_x - 1) - FROG_W;
            } else if (m.id == OBJ_GRENADE && m.pid == frog_pid) {
                // Consenti il fuoco solo se la rana è nella fascia fiume
                if (frog_y >= Y_FIUME && frog_y < Y_MARCIAPIEDE) {
                    // Il padre crea due processi proiettile: a sinistra e a destra
                    // Usa la posizione corrente della rana mantenuta dal padre
                    long long t = now_ms();
                    if (t - last_grenade_ms < GRENADE_COOLDOWN_MS) {
                        // cooldown non ancora passato: ignora richiesta
                    } else {
                        last_grenade_ms = t;
                        int gx = frog_x;
                        int gy = frog_y;
                        pid_t lg = fork();
                        if (lg == 0) {
                            // Spawn a sinistra: inizia subito fuori dalla rana
                            projectile_process(pipe_fds[1], gx - 1, gy, -1, OBJ_GRENADE);
                            _exit(0);
                        }
                        pid_t rg = fork();
                        if (rg == 0) {
                            // Spawn a destra: inizia subito fuori dalla rana
                            projectile_process(pipe_fds[1], gx + FROG_W, gy, +1, OBJ_GRENADE);
                            _exit(0);
                        }
                    }
                } else {
                    // Fuori dal fiume: ignora la richiesta di granata
                }
            } else if (m.id == OBJ_CROC) { // Se il messaggio riguarda un coccodrillo...
                CrocState* cs = get_croc_slot(m.pid); // Cerco (o alloco) lo slot del coccodrillo corrispondente al pid ricevuto.
                if (cs) {
                    int prev_x = cs->x;
                    if (cs->has_pos) {
                        cs->dx_frame += (m.x - prev_x); // accumula il delta mosso in questo frame
                    } else {
                        cs->has_pos = 1; // prima posizione valida
                        // niente delta al primo update per evitare salti
                    }
                    cs->x = m.x; // Aggiorna la posizione x del coccodrillo.
                    cs->y = m.y; // Aggiorna la posizione y del coccodrillo.
                    cs->x_speed = m.x_speed; // Aggiorna la velocità orizzontale del coccodrillo.
                }
            } else if (m.id == OBJ_PROJECTILE) { // Se il messaggio riguarda un proiettile coccodrillo...
                ProjectileState* ps = get_projectile_slot(m.pid);
                if (ps) {
                    ps->id = OBJ_PROJECTILE;
                    // Se è la prima volta che riceviamo un messaggio da questo proiettile,
                    // determina la direzione dal coccodrillo che lo ha sparato
                    if (ps->direction == 0) {
                        // Cerca il coccodrillo alla stessa altezza del proiettile
                        for (int i = 0; i < MAX_CROCS; i++) {
                            if (crocs[i].in_use && crocs[i].y == m.y) {
                                // Determina direzione dal movimento del coccodrillo
                                ps->direction = (crocs[i].x_speed > 0) ? 1 : -1;
                                break;
                            }
                        }
                        if (ps->direction == 0) ps->direction = 1; // fallback
                    }
                    ps->x = m.x;
                    ps->y = m.y;
                }
            } else if (m.id == OBJ_GRENADE) { // Se il messaggio riguarda una granata (proiettile rana)
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
                // richiesta di uscita dal figlio rana
                running = 0;
                break;
            }
            continue; // Dopo aver gestito il messaggio, torno all'inizio del ciclo per leggere altri messaggi (entro il limite).
        }
        if (n == 0) { // Se read restituisce 0, la pipe è stata chiusa dall'altra estremità.
            running = 0; // Imposto running a 0 per terminare il ciclo principale del gioco.
            break; // Esco dal ciclo di lettura messaggi.
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // Se read restituisce -1 e errno è EAGAIN/EWOULDBLOCK, non ci sono più messaggi disponibili al momento (pipe non bloccante).
            break; // Esco dal ciclo di lettura messaggi.
        }
        if (n < 0) { // Se read restituisce -1 per altri motivi (errore vero)...
            perror("read"); // Stampo l'errore.
            running = 0; // Imposto running a 0 per terminare il ciclo principale del gioco.
            break; // Esco dal ciclo di lettura messaggi.
        }
    }

    // Reap non bloccante dei figli proiettile/granata creati dal padre per evitare zombie
    {
        pid_t zr;
        while ((zr = waitpid(-1, NULL, WNOHANG)) > 0) {
            // solo raccolta
        }
    }

    // Riding: se la rana è su un coccodrillo, prima si muove con lui
    int ride_dx = 0;
    bool on_croc = is_frog_on_croc(&ride_dx);
    // Se il giocatore sta dando un input orizzontale in questo frame,
    // non applichiamo il riding per non contrastare il movimento volontario.
    if (on_croc) {
        frog_x += ride_dx;
    }
    // Salva la posizione precedente (dopo il riding, prima dell'input del giocatore)
    int prev_frog_x = frog_x;
    int prev_frog_y = frog_y;
    // Ora applica l'input dell'utente (accumulato)
    frog_x += acc_dx;
    frog_y += acc_dy;
    // Clamps: impedisce alla rana di uscire dai bordi e dalla fascia inferiore
    if (frog_x < 1) frog_x = 1;
    if (frog_x + FROG_W > max_x - 1) frog_x = (max_x - 1) - FROG_W;
    if (frog_y < Y_TANE) frog_y = Y_TANE;
    {
        int floor_y = Y_MARCIAPIEDE + ZONE_MARCIAPIEDE_H - FROG_H;
        if (frog_y > floor_y) frog_y = floor_y;
    }

    // Se la rana è parzialmente su un coccodrillo, agganciala al bordo di salita
    snap_frog_onto_croc_edge_if_partial();

    // Gestione fascia delle tane (senza perdita vita)
    if (frog_y == Y_TANE) {
        int inside_idx = -1;
        for (int i = 0; i < 5; ++i) {
            if (!tane_closed[i] && is_frog_inside_tana_index(i)) {
                inside_idx = i;
                break;
            }
        }

        if (inside_idx >= 0) {
            // Chiudi la tana aperta, aggiorna background e avvia nuova manche
            tane_closed[inside_idx] = 1;
            add_score_for_den();
            if (bg_win) {
                draw_background_into(bg_win);
            }
            if (all_tane_closed()) {
                bool again = show_end_screen(END_VICTORY, score);
                if (again) {
                    restart_game(&frog_pid, &creator_pid);
                    continue;
                } else {
                    running = 0;
                    continue;
                }
            }
            start_new_manche();
            continue;
        } else {
            // Zona superiore non valida (fuori tana aperta o tana già chiusa): perde una vita e la manche
            add_score_for_death();
            lives--;
            if (lives <= 0) {
                bool again = show_end_screen(END_DEFEAT, score);
                if (again) {
                    restart_game(&frog_pid, &creator_pid);
                    continue;
                } else {
                    running = 0;
                    continue;
                }
            } else {
                start_new_manche();
                continue;
            }
        }
    }

    // Gestisce le collisioni e la morte della rana
    handle_frog_collisions(&running, &frog_pid, &creator_pid);
    if (!running) continue; // Salta il resto del frame se game over

    // Controlla collisioni granata vs proiettile
    check_grenade_vs_projectile();

    // Disegna tutto il frame di gioco
    draw_game_frame();

    // Piccola pausa
    napms(16);
}

// Chiusura del main: cleanup finale e uscita
full_cleanup(frog_pid, creator_pid);
return 0;
}

// Termina un singolo processo in modo sicuro
static void terminate_process(pid_t pid) {
    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, WNOHANG);
    }
}

// Termina tutti i processi coccodrilli attivi
static void cleanup_crocs(void) {
    for (int i = 0; i < MAX_CROCS; i++) {
        if (crocs[i].in_use && crocs[i].pid > 0) {
            terminate_process(crocs[i].pid);
            crocs[i].in_use = 0;
            crocs[i].pid = -1;
        }
    }
}

// Termina tutti i processi proiettili attivi
static void cleanup_projectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].in_use && projectiles[i].pid > 0) {
            terminate_process(projectiles[i].pid);
            projectiles[i].in_use = 0;
            projectiles[i].pid = -1;
        }
    }
}

// Termina tutti i processi granate attivi
// (Rimossa) cleanup granate: non più necessario, gestite come proiettili

// Chiude le pipe se ancora aperte
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

// Gestisce le collisioni della rana e determina se deve morire
static void handle_frog_collisions(int* running, pid_t* frog_pid, pid_t* creator_pid) {
    // Ri-controlla se la rana è su un coccodrillo dopo tutto il movimento
    int final_ride_dx = 0;
    bool final_on_croc = is_frog_on_croc(&final_ride_dx);

    // Controlla se la rana è in acqua e non su un coccodrillo (morte!)
    bool frog_in_water = (frog_y >= Y_FIUME && frog_y < Y_MARCIAPIEDE) && !final_on_croc;
    if (frog_in_water) {
        add_score_for_death();
        lives--;
        if (lives <= 0) {
            bool again = show_end_screen(END_DEFEAT, score);
            if (again) {
                restart_game(frog_pid, creator_pid);
            } else {
                // Game over
                *running = 0;
            }
        } else {
            // Reset manche
            start_new_manche();
        }
        return;
    }

    // Controlla collisioni con proiettili (morte!)
    if (check_projectile_collision()) {
        add_score_for_death();
        lives--;
        if (lives <= 0) {
            bool again = show_end_screen(END_DEFEAT, score);
            if (again) {
                restart_game(frog_pid, creator_pid);
            } else {
                // Game over
                *running = 0;
            }
        } else {
            // Reset manche
            start_new_manche();
        }
        return;
    }
}

// (Rimossa) Le granate sono ora gestite come proiettili nella stessa struttura

// Disegna l'intero frame di gioco
static void draw_game_frame(void) {
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);

    // Sfondo: pulizia completa garantita
    if (bg_win) {
        // Ripristina fondo prerenderizzato
        copywin(bg_win, game_win, 0, 0, 0, 0, GAME_HEIGHT - 1, GAME_WIDTH - 1, FALSE);
    } else {
        // Fallback: ridisegna lo sfondo e bordo
        werase(game_win);
        draw_background();
    }

    // Cleanup entità fuori schermo
    sweep_crocs_offscreen();
    sweep_projectiles_offscreen();

    // Disegna tutti gli elementi
    draw_crocs();
    draw_projectiles();
    draw_frog();
    draw_ui();

    // Debug: mostra coordinate rana in alto
    mvwprintw(game_win, 0, 2, "frog x=%d y=%d   ", frog_x, frog_y);

    // Mostra il frame
    wrefresh(game_win);
}

// Mostra una schermata di fine partita (vittoria/sconfitta) con score e chiede replay (Y/N)
static bool show_end_screen(int result, long long final_score) {
    // Pulisci e disegna bordo
    werase(game_win);
    box(game_win, 0, 0);

    // Dimensioni per centratura
    int max_y, max_x;
    getmaxyx(game_win, max_y, max_x);

    // Titolo in base al risultato
    const char *title = (result == END_VICTORY) ? "YOU WIN!" : "GAME OVER!";
    int title_x = (int)((max_x - (int)strlen(title)) / 2);
    int title_y = max_y / 3;
    mvwprintw(game_win, title_y, title_x, "%s", title);

    // Riga score
    char score_line[64];
    snprintf(score_line, sizeof(score_line), "SCORE: %lld", final_score);
    int score_x = (int)((max_x - (int)strlen(score_line)) / 2);
    int score_y = title_y + 2;
    mvwprintw(game_win, score_y, score_x, "%s", score_line);

    // Prompt replay
    const char *prompt = "Play again? (Y/N)";
    int prompt_x = (int)((max_x - (int)strlen(prompt)) / 2);
    int prompt_y = score_y + 2;
    mvwprintw(game_win, prompt_y, prompt_x, "%s", prompt);

    // Mostra
    wrefresh(game_win);

    // Lettura blocccante su game_win
    nodelay(game_win, FALSE);
    while (1) {
        int ch = wgetch(game_win);
        if (ch == 'y' || ch == 'Y') return true;
        if (ch == 'n' || ch == 'N' || ch == 'q' || ch == 'Q') return false;
    }
}

// Riavvia completamente la partita: uccide i processi attuali, resetta lo stato e ri-forka
static void restart_game(pid_t* frog_pid, pid_t* creator_pid) {
    // Termina figli attuali
    if (frog_pid && *frog_pid > 0) terminate_process(*frog_pid);
    if (creator_pid && *creator_pid > 0) terminate_process(*creator_pid);
    cleanup_crocs();
    cleanup_projectiles();

    // Svuota pipe e ri-creala per sicurezza
    cleanup_pipes();
    if (pipe(pipe_fds) == -1) {
        endwin(); perror("pipe"); exit(1);
    }
    int fl = fcntl(pipe_fds[0], F_GETFL, 0);
    fcntl(pipe_fds[0], F_SETFL, fl | O_NONBLOCK);

    // Reset totale stato logico
    lives = LIVES_START;
    score = 0;
    for (int i = 0; i < 5; ++i) tane_closed[i] = 0;
    last_grenade_ms = -1000000000LL;
    // Pulisci slot locali
    for (int i = 0; i < MAX_CROCS; ++i) { crocs[i].in_use = 0; crocs[i].pid = -1; }
    for (int i = 0; i < MAX_PROJECTILES; ++i) { projectiles[i].in_use = 0; projectiles[i].pid = -1; projectiles[i].direction = 0; }

    // Recompute layout (se finestra cambiata)
    compute_tane_layout();
    if (bg_win) draw_background_into(bg_win); else draw_background();

    // Riforka rana
    int max_y, max_x; getmaxyx(game_win, max_y, max_x);
    int start_rx = (max_x - FROG_W) / 2;
    int start_ry = Y_MARCIAPIEDE;

    pid_t fp = fork();
    if (fp < 0) { endwin(); perror("fork"); exit(1); }
    if (fp == 0) {
        close(pipe_fds[0]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); }
        frog_process(pipe_fds[1], start_rx, start_ry);
        _exit(0);
    }
    if (frog_pid) *frog_pid = fp;

    // Riforka creatore
    pid_t cp = fork();
    if (cp < 0) { endwin(); perror("fork"); exit(1); }
    if (cp == 0) {
        croc_creator(pipe_fds[1]);
        _exit(0);
    }
    if (creator_pid) *creator_pid = cp;

    // Re-init dati di gioco e prima manche
    init_game_data();
    start_new_manche();

    // Primo frame
    if (bg_win) copywin(bg_win, game_win, 0, 0, 0, 0, GAME_HEIGHT - 1, GAME_WIDTH - 1, FALSE);
    draw_frog();
    draw_ui();
    wrefresh(game_win);
}

// Cleanup completo di tutte le risorse
static void full_cleanup(pid_t frog_pid, pid_t creator_pid) {
    // Termina processo rana
    if (frog_pid > 0) {
        terminate_process(frog_pid);
    }

    // Termina processo creatore
    if (creator_pid > 0) {
        terminate_process(creator_pid);
    }

    // Cleanup processi di gioco
    cleanup_crocs();
    cleanup_projectiles();

    // Chiudi pipe
    cleanup_pipes();

    // Cleanup ncurses
    if (game_win) {
        delwin(game_win);
        game_win = NULL;
    }
    if (bg_win) {
        delwin(bg_win);
        bg_win = NULL;
    }
    endwin();

    // Raccogli eventuali zombie rimasti
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Continua fino a quando non ci sono più figli da raccogliere
    }
}

// (Rimosso: il cleanup finale e il return vengono ora gestiti in full_cleanup e alla chiusura del main)



