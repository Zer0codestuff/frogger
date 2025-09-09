/*
 * Frogger Resurrection - Versione Semplificata
 * Implementa tutti i requisiti del progetto con codice pulito e organizzato
 */

#include <ncurses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

// =============================================================================
// CONFIGURAZIONE GIOCO
// =============================================================================

#define GAME_WIDTH      101
#define GAME_HEIGHT     35
#define FROG_WIDTH      3
#define FROG_HEIGHT     2
#define CROC_WIDTH      9
#define NUM_STREAMS     8
#define NUM_DENS        5
#define START_LIVES     5
#define LEVEL_TIME      60
#define MAX_CROCS       16
#define MAX_PROJECTILES 32
#define MAX_GRENADES    16

// Codici messaggi
#define MSG_FROG        1
#define MSG_CROC        2
#define MSG_PROJECTILE  3
#define MSG_GRENADE     4
#define MSG_QUIT        5

// Zone verticali
#define Y_DENS          2
#define Y_GRASS         4
#define Y_RIVER         6
#define Y_SIDWALK       (Y_RIVER + NUM_STREAMS * FROG_HEIGHT)
#define Y_UI            (Y_SIDWALK + FROG_HEIGHT + 1)

// =============================================================================
// STRUTTURE DATI
// =============================================================================

typedef struct {
    int type;
    int x, y;
    int data; // velocità per croc, direzione per proiettili/granate
    int pid;
} GameMessage;

typedef struct {
    int in_use;
    int x, y;
    int pid;
} Entity;

typedef struct {
    int x, y;
    int active;
} Den;

// Variabili globali
static int pipe_fds[2];
static int frog_x, frog_y;
static int lives = START_LIVES;
static int score = 0;
static time_t level_start;
static int dens_reached = 0;
static int running = 1;

static Entity crocs[MAX_CROCS];
static Entity projectiles[MAX_PROJECTILES];
static Entity grenades[MAX_GRENADES];
static Den dens[NUM_DENS];

// Sprite rana
static const char *frog_sprite[FROG_HEIGHT] = {
    "@-@",
    "l-l"
};

// =============================================================================
// FUNZIONI AUSILIARIE
// =============================================================================

static int get_free_entity_slot(Entity *entities, int max_count) {
    for (int i = 0; i < max_count; i++) {
        if (!entities[i].in_use) return i;
    }
    return -1;
}

static void send_message(const GameMessage *msg) {
    write(pipe_fds[1], msg, sizeof(GameMessage));
}

static void cleanup_entity(Entity *entity) {
    if (entity->in_use && entity->pid > 0) {
        kill(entity->pid, SIGKILL);
        waitpid(entity->pid, NULL, 0);
    }
    entity->in_use = 0;
    entity->pid = 0;
}

// =============================================================================
// INIZIALIZZAZIONE GRAFICA
// =============================================================================

static void init_game(void) {
    // Inizializza ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    start_color();

    // Definizione colori
    init_pair(1, COLOR_GREEN, COLOR_BLACK);   // Coccodrilli
    init_pair(2, COLOR_CYAN, COLOR_BLUE);     // Fiume
    init_pair(3, COLOR_WHITE, COLOR_BLACK);   // Marciapiede
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // Tane
    init_pair(5, COLOR_RED, COLOR_BLACK);     // Rana/granate
    init_pair(6, COLOR_RED, COLOR_BLUE);      // Rana in acqua

    // Crea pipe
    if (pipe(pipe_fds) < 0) {
        perror("pipe");
        exit(1);
    }

    // Pipe non bloccante
    int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);

    // Inizializza tane
    for (int i = 0; i < NUM_DENS; i++) {
        dens[i].x = (GAME_WIDTH / (NUM_DENS + 1)) * (i + 1) - FROG_WIDTH/2;
        dens[i].y = Y_DENS;
        dens[i].active = 1;
    }

    // Posizione iniziale rana
    frog_x = GAME_WIDTH / 2 - FROG_WIDTH/2;
    frog_y = Y_SIDWALK;

    level_start = time(NULL);
}

static void draw_background(void) {
    clear();

    // Disegna UI
    attron(COLOR_PAIR(5));
    mvprintw(0, 2, "FROGGER RESURRECTION - Vite: %d  Punteggio: %d", lives, score);
    attroff(COLOR_PAIR(5));

    // Disegna tane
    attron(COLOR_PAIR(4));
    for (int i = 0; i < NUM_DENS; i++) {
        if (dens[i].active) {
            for (int y = 0; y < FROG_HEIGHT; y++) {
                for (int x = 0; x < FROG_WIDTH; x++) {
                    mvaddch(Y_DENS + y, dens[i].x + x, ' ');
                }
            }
        }
    }
    attroff(COLOR_PAIR(4));

    // Disegna prato
    attron(COLOR_PAIR(1));
    for (int y = Y_GRASS; y < Y_RIVER; y++) {
        for (int x = 1; x < GAME_WIDTH - 1; x++) {
            mvaddch(y, x, '.');
        }
    }
    attroff(COLOR_PAIR(1));

    // Disegna fiume
    attron(COLOR_PAIR(2));
    for (int y = Y_RIVER; y < Y_SIDWALK; y++) {
        for (int x = 1; x < GAME_WIDTH - 1; x++) {
            mvaddch(y, x, '~');
        }
    }
    attroff(COLOR_PAIR(2));

    // Disegna marciapiede
    attron(COLOR_PAIR(3));
    for (int y = Y_SIDWALK; y < Y_SIDWALK + FROG_HEIGHT; y++) {
        for (int x = 1; x < GAME_WIDTH - 1; x++) {
            mvaddch(y, x, ' ');
        }
    }
    attroff(COLOR_PAIR(3));

    // Disegna bordo
    box(stdscr, 0, 0);
}

// =============================================================================
// PROCESSI FIGLI
// =============================================================================

static void frog_process(void) {
    close(pipe_fds[0]);

    GameMessage msg;
    msg.type = MSG_FROG;

    while (running) {
        int key = getch();
        int dx = 0, dy = 0;

        if (key == 'q') {
            msg.type = MSG_QUIT;
            send_message(&msg);
            break;
        } else if (key == ' ') {
            msg.type = MSG_GRENADE;
            msg.x = 0; msg.y = 0;
            send_message(&msg);
        } else if (key == KEY_UP) {
            dy = -FROG_HEIGHT;
        } else if (key == KEY_DOWN) {
            dy = FROG_HEIGHT;
        } else if (key == KEY_LEFT) {
            dx = -FROG_WIDTH;
        } else if (key == KEY_RIGHT) {
            dx = FROG_WIDTH;
        }

        if (dx != 0 || dy != 0) {
            msg.type = MSG_FROG;
            msg.x = dx;
            msg.y = dy;
            send_message(&msg);
        }

        usleep(30000);
    }

    close(pipe_fds[1]);
    _exit(0);
}

static void croc_process(int stream_index) {
    close(pipe_fds[0]);

    // Direzione alternata per corsie
    int direction = (stream_index % 2 == 0) ? 1 : -1;
    int y = Y_RIVER + stream_index * FROG_HEIGHT;
    int speed = 1 + (stream_index % 3); // Velocità variabile
    int shoot_cooldown = 0;

    GameMessage msg;
    msg.type = MSG_CROC;
    msg.y = y;

    // Posizione iniziale
    if (direction > 0) {
        msg.x = 1 - CROC_WIDTH;
    } else {
        msg.x = GAME_WIDTH - 2;
    }

    while (running) {
        // Aggiorna posizione
        msg.x += direction * speed;

        // Se esce dal campo, termina
        if ((direction > 0 && msg.x > GAME_WIDTH - 2) ||
            (direction < 0 && msg.x + CROC_WIDTH < 1)) {
            break;
        }

        send_message(&msg);

        // Sparo casuale
        if (shoot_cooldown <= 0) {
            if (rand() % 200 < 3) { // 1.5% probabilità
                GameMessage projectile_msg;
                projectile_msg.type = MSG_PROJECTILE;
                projectile_msg.x = (direction > 0) ? msg.x + CROC_WIDTH : msg.x;
                projectile_msg.y = y;
                projectile_msg.data = direction;
                send_message(&projectile_msg);
                shoot_cooldown = 50;
            }
        } else {
            shoot_cooldown--;
        }

        usleep(80000 + (stream_index * 10000));
    }

    close(pipe_fds[1]);
    _exit(0);
}

static void projectile_process(int start_x, int start_y, int direction) {
    close(pipe_fds[0]);

    GameMessage msg;
    msg.type = MSG_PROJECTILE;
    msg.x = start_x;
    msg.y = start_y;
    msg.data = direction;

    while (running) {
        msg.x += direction;

        // Se esce dal campo, termina
        if (msg.x < 1 || msg.x >= GAME_WIDTH - 1) break;

        send_message(&msg);
        usleep(40000);
    }

    close(pipe_fds[1]);
    _exit(0);
}

static void grenade_process(int start_x, int start_y, int direction) {
    close(pipe_fds[0]);

    GameMessage msg;
    msg.type = MSG_GRENADE;
    msg.x = start_x;
    msg.y = start_y;
    msg.data = direction;

    while (running) {
        msg.x += direction;

        // Se esce dal campo, termina
        if (msg.x < 1 || msg.x >= GAME_WIDTH - 1) break;

        send_message(&msg);
        usleep(30000);
    }

    close(pipe_fds[1]);
    _exit(0);
}

// =============================================================================
// LOGICA DI GIOCO
// =============================================================================

static void handle_message(const GameMessage *msg) {
    switch (msg->type) {
        case MSG_FROG: {
            frog_x += msg->x;
            frog_y += msg->y;

            // Clamps
            if (frog_x < 1) frog_x = 1;
            if (frog_x + FROG_WIDTH > GAME_WIDTH - 1) frog_x = GAME_WIDTH - 1 - FROG_WIDTH;
            if (frog_y < Y_DENS) frog_y = Y_DENS;
            if (frog_y > Y_SIDWALK + FROG_HEIGHT - 1) frog_y = Y_SIDWALK + FROG_HEIGHT - 1;
            break;
        }

        case MSG_CROC: {
            int slot = get_free_entity_slot(crocs, MAX_CROCS);
            if (slot >= 0) {
                crocs[slot].in_use = 1;
                crocs[slot].x = msg->x;
                crocs[slot].y = msg->y;
                crocs[slot].pid = msg->pid;
            }
            break;
        }

        case MSG_PROJECTILE: {
            int slot = get_free_entity_slot(projectiles, MAX_PROJECTILES);
            if (slot >= 0) {
                projectiles[slot].in_use = 1;
                projectiles[slot].x = msg->x;
                projectiles[slot].y = msg->y;
                projectiles[slot].pid = msg->pid;
            }
            break;
        }

        case MSG_GRENADE: {
            // Lancia due granate in direzioni opposte
            for (int dir = -1; dir <= 1; dir += 2) {
                pid_t pid = fork();
                if (pid == 0) {
                    grenade_process(frog_x + FROG_WIDTH/2, frog_y, dir);
                } else if (pid > 0) {
                    int slot = get_free_entity_slot(grenades, MAX_GRENADES);
                    if (slot >= 0) {
                        grenades[slot].in_use = 1;
                        grenades[slot].x = frog_x + FROG_WIDTH/2;
                        grenades[slot].y = frog_y;
                        grenades[slot].pid = pid;
                    }
                }
            }
            break;
        }

        case MSG_QUIT:
            running = 0;
            break;
    }
}

static void check_collisions(void) {
    // Collisione rana in acqua
    if (frog_y >= Y_RIVER && frog_y < Y_SIDWALK) {
        int on_croc = 0;
        for (int i = 0; i < MAX_CROCS; i++) {
            if (crocs[i].in_use &&
                frog_y == crocs[i].y &&
                frog_x >= crocs[i].x &&
                frog_x < crocs[i].x + CROC_WIDTH) {
                on_croc = 1;
                break;
            }
        }
        if (!on_croc) {
            lives--;
            if (lives <= 0) {
                running = 0;
            } else {
                frog_x = GAME_WIDTH / 2 - FROG_WIDTH/2;
                frog_y = Y_SIDWALK;
                level_start = time(NULL);
            }
        }
    }

    // Collisione rana-proiettile
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].in_use &&
            frog_x <= projectiles[i].x && projectiles[i].x < frog_x + FROG_WIDTH &&
            frog_y <= projectiles[i].y && projectiles[i].y < frog_y + FROG_HEIGHT) {
            cleanup_entity(&projectiles[i]);
            lives--;
            if (lives <= 0) {
                running = 0;
            } else {
                frog_x = GAME_WIDTH / 2 - FROG_WIDTH/2;
                frog_y = Y_SIDWALK;
                level_start = time(NULL);
            }
        }
    }

    // Collisione granata-proiettile
    for (int g = 0; g < MAX_GRENADES; g++) {
        if (!grenades[g].in_use) continue;
        for (int p = 0; p < MAX_PROJECTILES; p++) {
            if (!projectiles[p].in_use) continue;
            if (grenades[g].x == projectiles[p].x && grenades[g].y == projectiles[p].y) {
                cleanup_entity(&grenades[g]);
                cleanup_entity(&projectiles[p]);
            }
        }
    }

    // Collisione rana-tana
    if (frog_y == Y_DENS) {
        for (int i = 0; i < NUM_DENS; i++) {
            if (dens[i].active &&
                frog_x >= dens[i].x && frog_x < dens[i].x + FROG_WIDTH) {
                dens[i].active = 0;
                dens_reached++;
                score += 100;

                if (dens_reached >= NUM_DENS) {
                    running = 0; // Vittoria!
                } else {
                    frog_x = GAME_WIDTH / 2 - FROG_WIDTH/2;
                    frog_y = Y_SIDWALK;
                    level_start = time(NULL);
                }
                break;
            }
        }
    }
}

static void draw_entities(void) {
    // Disegna coccodrilli
    attron(COLOR_PAIR(1));
    for (int i = 0; i < MAX_CROCS; i++) {
        if (crocs[i].in_use) {
            for (int yy = 0; yy < FROG_HEIGHT; yy++) {
                for (int xx = 0; xx < CROC_WIDTH; xx++) {
                    int px = crocs[i].x + xx;
                    int py = crocs[i].y + yy;
                    if (px >= 1 && px < GAME_WIDTH - 1 && py >= 1 && py < GAME_HEIGHT - 1) {
                        mvaddch(py, px, 'C');
                    }
                }
            }
        }
    }
    attroff(COLOR_PAIR(1));

    // Disegna proiettili
    attron(COLOR_PAIR(5));
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].in_use) {
            mvaddch(projectiles[i].y, projectiles[i].x, '*');
        }
    }
    attroff(COLOR_PAIR(5));

    // Disegna granate
    attron(COLOR_PAIR(5));
    for (int i = 0; i < MAX_GRENADES; i++) {
        if (grenades[i].in_use) {
            mvaddch(grenades[i].y, grenades[i].x, 'o');
        }
    }
    attroff(COLOR_PAIR(5));

    // Disegna rana
    int frog_color = (frog_y >= Y_RIVER && frog_y < Y_SIDWALK) ? 6 : 5;
    attron(COLOR_PAIR(frog_color));
    for (int i = 0; i < FROG_HEIGHT; i++) {
        mvprintw(frog_y + i, frog_x, frog_sprite[i]);
    }
    attroff(COLOR_PAIR(frog_color));
}

static void cleanup_entities(void) {
    // Cleanup coccodrilli
    for (int i = 0; i < MAX_CROCS; i++) {
        cleanup_entity(&crocs[i]);
    }

    // Cleanup proiettili
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        cleanup_entity(&projectiles[i]);
    }

    // Cleanup granate
    for (int i = 0; i < MAX_GRENADES; i++) {
        cleanup_entity(&grenades[i]);
    }
}

static void show_game_over(int victory) {
    clear();
    if (victory) {
        mvprintw(GAME_HEIGHT/2 - 2, GAME_WIDTH/2 - 10, "VITTORIA!");
        mvprintw(GAME_HEIGHT/2, GAME_WIDTH/2 - 15, "Hai completato tutte le tane!");
    } else {
        mvprintw(GAME_HEIGHT/2 - 2, GAME_WIDTH/2 - 10, "GAME OVER");
        mvprintw(GAME_HEIGHT/2, GAME_WIDTH/2 - 15, "Hai perso tutte le vite!");
    }
    mvprintw(GAME_HEIGHT/2 + 2, GAME_WIDTH/2 - 10, "Punteggio: %d", score);
    mvprintw(GAME_HEIGHT/2 + 4, GAME_WIDTH/2 - 15, "Premi Q per uscire");
    refresh();
}

// =============================================================================
// MAIN
// =============================================================================

int main(void) {
    srand(time(NULL));

    init_game();

    // Fork processo rana
    pid_t frog_pid = fork();
    if (frog_pid == 0) {
        frog_process();
    }

    // Fork processi coccodrilli
    for (int i = 0; i < NUM_STREAMS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            croc_process(i);
        }
    }

    // Loop principale del padre
    while (running) {
        // Controlla timeout
        time_t now = time(NULL);
        if (now - level_start >= LEVEL_TIME) {
            lives--;
            if (lives <= 0) {
                running = 0;
            } else {
                frog_x = GAME_WIDTH / 2 - FROG_WIDTH/2;
                frog_y = Y_SIDWALK;
                level_start = now;
            }
        }

        // Ricevi messaggi
        GameMessage msg;
        while (read(pipe_fds[0], &msg, sizeof(msg)) > 0) {
            handle_message(&msg);
        }

        // Pulisci entità fuori schermo
        for (int i = 0; i < MAX_CROCS; i++) {
            if (crocs[i].in_use &&
                (crocs[i].x < 1 - CROC_WIDTH || crocs[i].x >= GAME_WIDTH - 1)) {
                cleanup_entity(&crocs[i]);
            }
        }

        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (projectiles[i].in_use &&
                (projectiles[i].x < 1 || projectiles[i].x >= GAME_WIDTH - 1)) {
                cleanup_entity(&projectiles[i]);
            }
        }

        for (int i = 0; i < MAX_GRENADES; i++) {
            if (grenades[i].in_use &&
                (grenades[i].x < 1 || grenades[i].x >= GAME_WIDTH - 1)) {
                cleanup_entity(&grenades[i]);
            }
        }

        check_collisions();
        draw_background();
        draw_entities();
        refresh();

        napms(16);
    }

    // Cleanup
    close(pipe_fds[0]);
    close(pipe_fds[1]);

    cleanup_entities();

    // Termina processi figli
    if (frog_pid > 0) {
        kill(frog_pid, SIGKILL);
        waitpid(frog_pid, NULL, 0);
    }

    // Termina tutti i processi zombie
    while (waitpid(-1, NULL, WNOHANG) > 0);

    // Mostra risultato finale
    show_game_over(dens_reached >= NUM_DENS);

    // Attendi input per uscire
    nodelay(stdscr, FALSE);
    while (getch() != 'q');

    endwin();
    return 0;
}
