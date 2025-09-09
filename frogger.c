#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h> // Per waitpid
#include <fcntl.h>    // Per fcntl (non-blocking pipe)
#include <errno.h>    // Per errno
#include <signal.h>   // Per kill


#define FROG_HEIGHT 2
#define FROG_WIDTH 3
#define COLORE_RANA 1

// Definisce la forma 3x3 della rana
const char *frog_shape[FROG_HEIGHT] = {
    // Altezza 2
    "@-@", // Larghezza 3
    "l-l",
};

// --- Aggiungo i colori per lo sfondo ---
#define COLORE_ACQUA 2
#define COLORE_STRADA 3
#define COLORE_ERBA 4
#define COLORE_TANA 5

// --- Nuovi colori per la rana su diverse superfici ---
#define COLORE_RANA_SU_ACQUA 6
#define COLORE_RANA_SU_ERBA 7


// --- Definizioni dimensionali del gioco (secondo specifiche) ---
#define GAME_WIN_WIDTH 101

// Altezze delle zone di gioco (basate su altezza rana)
#define ZONA_TANE_H (FROG_HEIGHT)
#define SPONDA_SUPERIORE_H (FROG_HEIGHT)
#define FIUME_H (8 * FROG_HEIGHT)
#define MARCIAPIEDE_H (FROG_HEIGHT)
#define SPAZIO_INFERIORE_H 3 // Spazio per UI (vite, tempo, ecc.)

// Altezza totale della finestra di gioco (contenuto + bordi)
#define GAME_WIN_HEIGHT (ZONA_TANE_H + SPONDA_SUPERIORE_H + FIUME_H + MARCIAPIEDE_H + SPAZIO_INFERIORE_H + 2)

// Coordinate Y di inizio di ogni zona (calcolate dall'alto)
#define ZONA_TANE_Y 1
#define SPONDA_SUPERIORE_Y (ZONA_TANE_Y + ZONA_TANE_H)
#define FIUME_Y (SPONDA_SUPERIORE_Y + SPONDA_SUPERIORE_H)
#define MARCIAPIEDE_Y (FIUME_Y + FIUME_H)
#define N_FLUSSI 8
int direzioni[N_FLUSSI] = {1, 1, 1, 1, 1, 1, 1, 1};
int velocita[N_FLUSSI] = {1, 1, 1, 1, 1, 1, 1, 1};
// --- Aggiunto ID per la rana ---
#define FROG_ID 0
#define BULL_ID 2
#define N_TANE 5

// Struttura per la comunicazione via pipe
// La rana invia le sue coordinate assolute
typedef struct
{
    int id;
    pid_t pid;
    int x; // Coordinata X assoluta
    int y; // Coordinata Y assoluta
} msg;

// Funzione helper per disegnare lo sfondo
void draw_background(WINDOW *win)
{
    if (!win)
        return;
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    // Bordo finestra
    box(win, 0, 0);

    // 1. Marciapiede di partenza
    wattron(win, COLOR_PAIR(COLORE_STRADA)); // Usiamo un colore diverso per il marciapiede
    for (int y = MARCIAPIEDE_Y; y < MARCIAPIEDE_Y + MARCIAPIEDE_H; ++y)
    {
        for (int x = 1; x < max_x - 1; ++x)
        {
            mvwaddch(win, y, x, ' ');
        }
    }
    wattroff(win, COLOR_PAIR(COLORE_STRADA));

    // 2. Fiume (ACQUA)
    wattron(win, COLOR_PAIR(COLORE_ACQUA));
    for (int y = FIUME_Y; y < MARCIAPIEDE_Y; ++y)
    {
        for (int x = 1; x < max_x - 1; ++x)
        {
            mvwaddch(win, y, x, '~');
        }
    }
    wattroff(win, COLOR_PAIR(COLORE_ACQUA));

    // 3. Sponda superiore (ERBA)
    wattron(win, COLOR_PAIR(COLORE_ERBA));
    for (int y = SPONDA_SUPERIORE_Y; y < FIUME_Y; ++y)
    {
        for (int x = 1; x < max_x - 1; ++x)
        {
            mvwaddch(win, y, x, ' ');
        }
    }
    wattroff(win, COLOR_PAIR(COLORE_ERBA));

    // 4. Zona Tane (su erba)
    wattron(win, COLOR_PAIR(COLORE_ERBA));
    for (int y = ZONA_TANE_Y; y < SPONDA_SUPERIORE_Y; ++y)
    {
        for (int x = 1; x < max_x - 1; ++x)
        {
            mvwaddch(win, y, x, ' ');
        }
    }
    wattroff(win, COLOR_PAIR(COLORE_ERBA));

    // Disegna le tane aperte
    wattron(win, COLOR_PAIR(COLORE_TANA));
    for (int i = 0; i < 5; ++i)
    {
        int tana_x = (max_x / 5) * i + (max_x / 10) - (FROG_WIDTH / 2);
        for (int y_offset = 0; y_offset < FROG_HEIGHT; ++y_offset)
        {
            for (int x_offset = 0; x_offset < FROG_WIDTH; ++x_offset)
            {
                if (tana_x + x_offset < max_x - 1 && tana_x + x_offset > 0)
                {
                    mvwaddch(win, ZONA_TANE_Y + y_offset, tana_x + x_offset, ' ');
                }
            }
        }
    }
    wattroff(win, COLOR_PAIR(COLORE_TANA));
}

// Funzione helper per disegnare la rana
void draw_frog(WINDOW *win, int y, int x)
{
    if (!win)
        return; // Controllo sicurezza

    // Scegli la coppia di colori corretta in base alla posizione Y della rana
    int color_pair_id;

    if (y >= MARCIAPIEDE_Y)
    {
        // La rana è sul marciapiede -> usa il colore base della rana (sfondo nero)
        color_pair_id = COLORE_RANA;
    }
    else if (y >= FIUME_Y)
    {
        // La rana è nel fiume -> usa il colore per la rana sull'acqua (sfondo blu)
        color_pair_id = COLORE_RANA_SU_ACQUA;
    }
    else
    {
        // La rana è sulla sponda superiore o nella zona tane -> usa il colore per la rana sull'erba (sfondo verde)
        // Entrambe le zone 'sponda' e 'tane' hanno uno sfondo d'erba di base
        color_pair_id = COLORE_RANA_SU_ERBA;
    }

    wattron(win, COLOR_PAIR(color_pair_id));
    for (int i = 0; i < FROG_HEIGHT; ++i)
    {
        mvwaddstr(win, y + i, x, frog_shape[i]);
    }
    wattroff(win, COLOR_PAIR(color_pair_id));
}


    

// --- Funzione eseguita dal processo figlio (Rana) ---
// Nota: il processo figlio NON deve usare ncurses per evitare conflitti di refresh.
// Legge direttamente da stdin (non bloccante) i caratteri inviati dal terminale.
// Gli arrow keys arrivano come sequenze di 3 byte: ESC '[' <A/B/C/D>.
// Il processo converte tali sequenze in movimenti e li invia al padre via pipe.
void frog_process(int pipe_write_fd, int start_x, int start_y)
{
    // Rende stdin non bloccante
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    msg move_req;
    move_req.id = FROG_ID;
    move_req.pid = getpid();
    move_req.x = start_x;
    move_req.y = start_y;

    // Invia la posizione iniziale al padre
    write(pipe_write_fd, &move_req, sizeof(msg));

    unsigned char buf[3];
    while (1)
    {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0)
        {
            // Nessun input – piccola pausa per ridurre CPU spin
            usleep(16000); // ~60 FPS equivalente
            continue;
        }

        int dx = 0;
        int dy = 0;

        // Gestisce input
        if (buf[0] == 'q')
        {
            break; // Esci
        }
        else if (buf[0] == 27) // ESC sequence (possibile freccia)
        {
            // Aspettiamo altri 2 byte se disponibili
            // read potrebbe averli già presi in buf[1], buf[2]
            if (n == 1)
            {
                // Prova a leggere il resto della sequenza
                read(STDIN_FILENO, buf + 1, 2);
            }
            if (buf[1] == '[')
            {
                switch (buf[2])
                {
                case 'A': // Freccia su
                    dy = -FROG_HEIGHT;
                    break;
                case 'B': // Freccia giù
                    dy = FROG_HEIGHT;
                    break;
                case 'D': // Freccia sinistra
                    dx = -FROG_WIDTH;
                    break;
                case 'C': // Freccia destra
                    dx = FROG_WIDTH;
                    break;
                default:
                    break;
                }
            }
        }
        // (Optionally handle WASD keys)
        else if (buf[0] == 'w')
            dy = -FROG_HEIGHT;
        else if (buf[0] == 's')
            dy = FROG_HEIGHT;
        else if (buf[0] == 'a')
            dx = -FROG_WIDTH;
        else if (buf[0] == 'd')
            dx = FROG_WIDTH;

        if (dx != 0 || dy != 0)
        {
            move_req.x += dx;
            move_req.y += dy;
            write(pipe_write_fd, &move_req, sizeof(msg));
        }
    }

    // Pulizia e uscita
    close(pipe_write_fd);
    exit(0);
}

int main()
{
    int frog_y, frog_x;
    WINDOW *game_win = NULL; // Inizializza a NULL
    pid_t frog_pid;          // PID del processo figlio (rana)
    int pipe_fds[2];         // Descrittori per la pipe [0]=read, [1]=write

    // Inizializza ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    init_pair(COLORE_RANA, COLOR_RED, COLOR_BLACK); // Rana su marciapiede (sfondo nero)
    init_pair(COLORE_ACQUA, COLOR_CYAN, COLOR_BLUE);
    init_pair(COLORE_RANA_SU_ACQUA, COLOR_RED, COLOR_BLUE); // Rana su acqua (sfondo blu)
    init_pair(COLORE_RANA_SU_ERBA, COLOR_RED, COLOR_GREEN); // Rana su erba (sfondo verde)
    init_pair(COLORE_STRADA, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLORE_ERBA, COLOR_BLACK, COLOR_GREEN);
    init_pair(COLORE_TANA, COLOR_BLACK, COLOR_YELLOW);


    // Rimosso keypad e timeout per stdscr, non sono più necessari nel padre
    
    // Ottiene dimensioni schermo
    int term_max_y, term_max_x;
    getmaxyx(stdscr, term_max_y, term_max_x);

    // Definisce dimensioni e posizione finestra
    int win_height = GAME_WIN_HEIGHT;
    int win_width = GAME_WIN_WIDTH;
    int start_y = (term_max_y - win_height) / 2;
    int start_x = (term_max_x - win_width) / 2;
    if (start_y < 0)
        start_y = 0;
    if (start_x < 0)
        start_x = 0;

    // Crea la finestra di gioco
    game_win = newwin(win_height, win_width, start_y, start_x);

    // --- AGGIUNTA: Controllo per la creazione della finestra ---
    if (game_win == NULL)
    {
        endwin();
        fprintf(stderr, "Errore: il terminale è troppo piccolo per la finestra di gioco.\n");
        fprintf(stderr, "Dimensioni richieste: %d righe, %d colonne.\n", win_height, win_width);
        fprintf(stderr, "Dimensioni attuali:   %d righe, %d colonne.\n", term_max_y, term_max_x);
        return 1;
    }
    // --- FINE AGGIUNTA ---

    keypad(game_win, TRUE);  // Abilita tasti funzione per la finestra di gioco
    wtimeout(game_win, 100); // Timeout per wgetch nel figlio
    box(game_win, 0, 0);

    // Ottiene dimensioni interne finestra
    int win_max_y, win_max_x;
    getmaxyx(game_win, win_max_y, win_max_x);

    // Inizializza posizione rana nel padre
    frog_y = MARCIAPIEDE_Y; // Posiziona la rana sul marciapiede di partenza
    frog_x = (win_max_x - FROG_WIDTH) / 2;

    // --- Crea la Pipe ---
    if (pipe(pipe_fds) == -1)
    {
        perror("pipe");
        endwin();
        exit(1);
    }

    // --- Crea il Processo Figlio (Rana) ---
    frog_pid = fork();

    if (frog_pid < 0)
    {
        perror("fork");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        delwin(game_win);
        endwin();
        exit(1);
    }

    if (frog_pid == 0)
    {
        // --- Codice del Processo Figlio (Rana) ---
        close(pipe_fds[0]); // Il figlio non legge dalla pipe
        // Esegui la funzione del processo rana, passando le coordinate iniziali
        frog_process(pipe_fds[1], frog_x, frog_y);
        // Non dovrebbe mai arrivare qui, frog_process termina con exit()
    }
    else
    {
        // --- Codice del Processo Padre (Gioco Principale) ---
        close(pipe_fds[1]); // Il padre non scrive sulla pipe

        // Imposta la lettura dalla pipe come non bloccante
        int flags = fcntl(pipe_fds[0], F_GETFL, 0);
        fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);

        // Disegna lo sfondo una volta all'inizio
        draw_background(game_win);
        wrefresh(game_win);

        // Variabili per tracciare la posizione precedente della rana
        int last_frog_y = -1, last_frog_x = -1;

        // Ciclo di gioco del Padre
        while (1) // Il loop termina quando la pipe si chiude
        {
            // Leggi dalla pipe (messaggi dalla rana)
            msg received_req;
            int bytes_read = read(pipe_fds[0], &received_req, sizeof(msg));

            if (bytes_read > 0)
            {
                // Dati ricevuti da un processo
                if (received_req.id == FROG_ID)
                {
                    // La nuova posizione della rana è quella ricevuta
                    int new_y = received_req.y;
                    int new_x = received_req.x;

                    // Controlla i bordi per la posizione richiesta
                    if (new_y >= ZONA_TANE_Y && new_y <= MARCIAPIEDE_Y)
                    {
                        frog_y = new_y;
                    }
                    if (new_x >= 1 && (new_x + FROG_WIDTH - 1) <= win_max_x - 2)
                    {
                        frog_x = new_x;
                    }
                }
            }
            else if (bytes_read == 0)
            {
                // Pipe chiusa dal figlio (ha premuto 'q'), usciamo dal loop
                break;
            }
            else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                // Errore reale sulla pipe
                perror("read pipe");
                break; // Esci in caso di errore pipe
            }
            
            // Aggiorna la grafica ad ogni iterazione senza flickering
            draw_background(game_win);
            draw_frog(game_win, frog_y, frog_x);
            wrefresh(game_win);

            // Aggiorna la posizione precedente
            last_frog_y = frog_y;
            last_frog_x = frog_x;

            // Piccolo ritardo per non sovraccaricare la CPU
            usleep(16000); // Circa 60 FPS
        }

        // --- Uscita dal gioco ---
        // Invia segnale di terminazione al processo figlio se è ancora vivo
        if (frog_pid > 0)
        {
            kill(frog_pid, SIGKILL);    // Termina forzatamente il figlio
            waitpid(frog_pid, NULL, 0); // Attendi che il figlio termini (evita zombie)
        }

        // Pulisce ncurses
        close(pipe_fds[0]); // Chiudi l'estremo di lettura
        if (game_win)
            delwin(game_win);
        endwin();
    }

    return 0;
}