/*
 * FROGGER ULTIMATE - VERSIONE COMPLETA E COMMENTATA
 * ============================================================
 *
 * Questo è il codice completo di Frogger implementato con:
 * - Processi per ogni entità (rana, coccodrilli, proiettili)
 * - Matrice di gioco per gestire le posizioni
 * - Pipe per la comunicazione tra processi
 * - ncurses per la grafica testuale avanzata
 * - Sprite e colori personalizzati
 *
 * SPIEGAZIONE GENERALE PER PRINCIPIANTI:
 * =====================================
 *
 * Immagina di creare un videogioco dove:
 * 1. La "rana" deve attraversare una strada con auto e un fiume con tronchi
 * 2. Tutto si muove contemporaneamente
 * 3. Devi gestire input da tastiera, grafica, collisioni
 *
 * In questo codice complesso, ogni "oggetto" (rana, coccodrillo, proiettile)
 * diventa un processo separato che "vive" da solo e comunica con gli altri
 * attraverso delle "tubature" (pipe).
 *
 * La "matrice di gioco" è come una grande tabella che tiene traccia
 * di cosa c'è in ogni posizione dello schermo.
 *
 * INIZIAMO CON LE INCLUDE - LE "SCATOLE DEGLI STRUMENTI"
 */

/*
 * LE INCLUDE - COME PRENDERE GLI STRUMENTI DAL MAGAZZINO
 * ======================================================
 *
 * Prima di iniziare a costruire, devi prendere gli strumenti.
 * Ogni #include è come andare in un negozio specializzato:
 */

// Queste sono le "librerie standard" che vengono con C
#include <locale.h>     // Gestisce caratteri speciali e lingue (per UTF-8)
#include <ncurses.h>    // LA GRANDE LIBRERIA PER GRAFICA TESTUALE
                       // Ti permette di controllare ogni carattere sullo schermo
#include <stdio.h>      // Input/Output standard (printf, scanf)
#include <stdlib.h>     // Funzioni utili (rand, malloc, exit)
#include <stdbool.h>    // Tipo bool (true/false) - novità in C99
#include <string.h>     // Manipolazione stringhe (strcpy, strlen)
#include <time.h>       // Gestione del tempo (clock, time)

// Queste sono per i PROCESSI - la parte più avanzata
#include <unistd.h>     // fork(), pipe(), getpid() - creare nuovi processi
#include <sys/wait.h>   // waitpid() - aspettare che un processo finisca
#include <sys/time.h>   // Per timing preciso
#include <sys/fcntl.h>  // Controllo avanzato dei file (per pipe)
#include <fcntl.h>      // Gestione file e pipe
#include <errno.h>      // Gestione errori del sistema
#include <signal.h>     // Segnali tra processi (kill, SIGKILL)
#include <math.h>       // Funzioni matematiche (round)

/*
 * NOTE PER IL FUTURO - COSE DA MIGLIORARE
 * =======================================
 * Questi sono commenti che il programmatore ha lasciato per sé
 */
 //Da fare:
// sistemare lo sfondo del proiettile RANA del marciapiede
// rendere distinguibili i proiettili dells rana e dei coccodrilli (cambiare i colori)

/*
 * LE MACRO - VALORI CHE NON CAMBIANO MAI
 * =======================================
 *
 * Le macro sono come delle costanti magiche che usi in tutto il programma.
 * Sono come scrivere lo stesso numero 100 volte, ma se devi cambiarlo,
 * lo cambi solo qui!
 */

// Impostazioni iniziali di ncurses
#define CURS 0        // Il cursore deve essere invisibile (0 = nascosto)
#define ND 0          // nodelay = non bloccare l'input (0 = aspetta input)
#define KP 1          // keypad = abilita tasti speciali (1 = sì)

// DIMENSIONI DELLO SCHERMO - LA TELA DOVE DISEGNI
// Queste sono le misure della nostra "tela" di gioco
#define DIM_X 155     // Larghezza totale della finestra di gioco (155 caratteri)
#define DIM_Y 42      // Altezza totale della finestra (42 righe)

// Posizioni per l'interfaccia (vite e tempo)
#define VITE_X 1      // Posizione X dove mostrare le vite (colonna 1)
#define VITE_Y DIM_Y-2 // Posizione Y per le vite (penultima riga)
#define START_TIME_X 2 // Inizio della barra del tempo
#define END_TIME_X DIM_X-3 // Fine della barra del tempo
#define TIME_Y 1      // Riga dove sta la barra del tempo

// Dimensioni delle ZONE del gioco
#define TANEX DIM_X-2    // Larghezza zona tane
#define TANEY 3          // Altezza zona tane
#define RIVA_X DIM_X-2   // Larghezza riva
#define RIVA_Y 4         // Altezza riva
#define FIUME_X DIM_X-2  // Larghezza fiume
#define FIUME_Y 24       // Altezza fiume (8 corsie)
#define MARCIAPIEDE_X DIM_X-2  // Larghezza marciapiede
#define MARCIAPIEDE_Y 4        // Altezza marciapiede

// Posizioni di INIZIO di ogni zona (coordinate X,Y)
#define TANESX 1        // Inizio X zona tane
#define TANESY 3        // Inizio Y zona tane
#define RIVASX 1        // Inizio X riva
#define RIVASY TANESY+TANEY    // Inizio Y riva (dopo tane)
#define FIUMESX 1       // Inizio X fiume
#define FIUMESY RIVASY+RIVA_Y  // Inizio Y fiume (dopo riva)
#define MARCIAPIEDESX 1         // Inizio X marciapiede
#define MARCIAPIEDESY FIUMESY+FIUME_Y  // Inizio Y marciapiede (dopo fiume)
#define FLUX_Y FIUME_Y/8        // Altezza di una corsia del fiume
#define TANE_LARGE 16            // Larghezza di ogni tana

/*
 * COLORI - LA PALETTE PER IL TUO GIOCO
 * ====================================
 *
 * ncurses usa una tabella di colori numerati.
 * Ogni numero corrisponde a una coppia colore-testo/sfondo
 */
#define COL_VITE 1           // Rosso su nero per le vite
#define COL_TIME 2           // Nero su rosso per la barra tempo
#define COL_RIVA 3           // Blu scuro per la riva
#define COL_MARCIAPIEDE 4    // Rosso scuro per il marciapiede
#define COL_FIUME 5          // Giallo su blu per il fiume
#define COL_RANA 6           // Colori base per la rana
#define COL_RANA_RIVA 7      // Rana sulla riva
#define COL_RANA_FIUME 8     // Rana in acqua
#define COL_RANA_MARC 9      // Rana sul marciapiede
#define COL_COCCODRILLI 10   // Coccodrilli
#define COL_TANE 11          // Tane
#define COL_RANA_CROC 12     // Rana sui coccodrilli
#define COL_BULL_CROC 13     // Proiettili coccodrilli
#define COL_BULL_RANA 14     // Proiettili rana

// COLORI PERSONALIZZATI - Oltre la tavolozza standard
#define VERDE_RANA 100       // Verde chiaro personalizzato per la rana
#define ROSSO_ARGINE 101     // Rosso scuro personalizzato per gli argini

/*
 * ID - COME IDENTIFICARE OGNI TIPO DI OGGETTO
 * ===========================================
 *
 * Ogni oggetto nel gioco ha un numero identificativo unico.
 * È come dare un nome/codice a ogni tipo di personaggio.
 */
#define FROG_ID 0        // La rana
#define FCROC_ID 1       // Il processo "padre" che crea coccodrilli
#define CROC_ID 2        // Un singolo coccodrillo
#define BULL_ID 3        // Proiettile della rana
#define BULL_CROC_ID 4   // Proiettile dei coccodrilli

/*
 * TESTI E MESSAGGI
 */
#define AVVISO_DIM_SCHERMO "INGRANDIRE LO SCHERMO" // Messaggio se schermo troppo piccolo

/*
 * DIMENSIONI DEI PERSONAGGI - QUANTO "SPAZIO" OCCUPANO
 * ====================================================
 *
 * Ogni personaggio occupa un rettangolo di caratteri
 */
#define FROG_X 4        // Rana: 4 caratteri di larghezza
#define FROG_Y 3        // Rana: 3 righe di altezza
#define CROC_X 16       // Coccodrillo: 16 caratteri di larghezza
#define CROC_Y FROG_Y   // Coccodrillo: stessa altezza della rana
#define BULL_X 3        // Proiettile: 3 caratteri larghezza
#define BULL_Y 2        // Proiettile: 2 righe altezza
#define TANA_X 6        // Tana: 6 caratteri larghezza
#define TANA_Y 4        // Tana: 4 righe altezza

/*
 * VALORI DI GIOCO - REGOLE E COSTANTI
 * ===================================
 */
#define VITE 5                 // Numero vite iniziali
#define SYMB_VITE "♥ "        // Simbolo per ogni vita
#define SYMB_BULL "☣"         // Simbolo proiettile rana
#define RIGHT_BULL "⁍"        // Simbolo proiettile coccodrillo destra
#define LEFT_BULL "⁌"         // Simbolo proiettile coccodrillo sinistra
#define TIMEL 60               // Tempo limite in secondi
#define TAB 32                 // Codice ASCII della barra spaziatrice

// Posizione iniziale della rana
#define FROG_START_X DIM_X/2   // Al centro orizzontale
#define FROG_START_Y MARCIAPIEDESY  // Sul marciapiede

// Configurazione del gioco
#define N_FLUSSI 8             // 8 corsie nel fiume
#define CROC_SPEED 1           // Velocità coccodrilli
#define BULL_SPEED 1           // Velocità proiettili
#define BULL_CHANCE 100        // Probabilità di sparo (1 su 100)

/*
 * TEMPI DI ATTESA - VELOCITÀ DEL GIOCO
 * ====================================
 *
 * Questi valori in microsecondi controllano quanto velocemente
 * si muovono le cose. Più piccolo = più veloce
 */
#define FROG_SLEEPS_TONIGHT 30000  // Tempo tra movimenti rana
#define MAIN_SLEEP 1               // Ciclo principale (molto veloce)
#define CREATOR_SLEEP 100000       // Tra la creazione di coccodrilli
#define CROC_SLEEP 90000           // Tra movimenti coccodrilli
#define BULL_SLEEP 60000           // Tra movimenti proiettili
#define FROG_BULL_SLEEP 500000     // Cooldown tra spari rana

/*
 * VARIABILI GLOBALI - LA "MEMORIA CONDIVISA"
 * ==========================================
 *
 * Queste variabili sono accessibili da TUTTE le funzioni.
 * Sono come una lavagna dove tutti possono leggere/scrivere.
 */

// La finestra principale del gioco
WINDOW *gamewin;  // Puntatore alla finestra ncurses

// Struttura per memorizzare posizione X,Y e PID di un oggetto
struct point {
   int x;         // Coordinata X (colonna)
   int y;         // Coordinata Y (riga)
   pid_t pid;     // ID del processo che controlla questo oggetto
   bool change;   // Flag: la posizione è cambiata?
   bool on_croc;  // Flag: la rana è su un coccodrillo?
   pid_t croc_pid; // PID del coccodrillo su cui sta la rana
};

// I messaggi che i processi si scambiano attraverso le pipe
struct msg {
    int id;        // Tipo di oggetto (FROG_ID, CROC_ID, ecc.)
    int x;         // Posizione X corrente
    int y;         // Posizione Y corrente
    char ch[4];    // Il carattere/sprite da disegnare
    pid_t pid;     // PID del processo che ha inviato il messaggio
    int x_speed;   // Velocità orizzontale
    int y_speed;   // Velocità verticale
    bool first;    // Flag per il primo messaggio di un oggetto
};

// Stato della partita (vince/perde)
struct game {
   bool manche_on;  // La manche è in corso?
   bool loss;       // Ha perso questa manche?
};

/*
 * SISTEMA DI COMUNICAZIONE - LE PIPE
 * ==================================
 *
 * LE PIPE SONO IL SISTEMA DI "POSTA" TRA PROCESSI!
 * ================================================
 *
 * Immagina: Hai una cassetta della posta dove diversi postini
 * (processi) mettono le loro lettere, e un postino capo (processo padre)
 * le ritira e le legge.
 *
 * pipe_fds[2] è un ARRAY DI 2 ELEMENTI:
 * ====================================
 * - pipe_fds[0] = Estremità di LETTURA (read)
 * - pipe_fds[1] = Estremità di SCRITTURA (write)
 *
 * COME FUNZIONA:
 * =============
 *
 * 1. **Creazione**: pipe(pipe_fds) crea la "cassetta della posta"
 * 2. **I processi FIGLI** scrivono su pipe_fds[1]
 * 3. **Il processo PADRE** legge da pipe_fds[0]
 * 4. **I messaggi** sono struct msg (coordinate, tipo oggetto, ecc.)
 *
 * ESEMPIO PRATICO:
 * ===============
 *
 * Processo coccodrillo vuole dire "Sono in posizione 50,20":
 *
 * struct msg message = {
 *     .id = CROC_ID,     // Sono un coccodrillo
 *     .x = 50,           // Mia posizione X
 *     .y = 20,           // Mia posizione Y
 *     .pid = 1234        // Mio ID processo
 * };
 *
 * write(pipe_fds[1], &message, sizeof(message)); // Metto nella cassetta
 *
 * Processo padre riceve:
 * read(pipe_fds[0], &received_msg, sizeof(received_msg)); // Prendo dalla cassetta
 *
 * PERCHÉ È IMPORTANTE?
 * ===================
 *
 * 1. **Isolamento processi**: Ogni processo lavora da solo
 * 2. **Sincronizzazione**: Il padre controlla tutto
 * 3. **Sicurezza**: I processi non si interferiscono
 * 4. **Scalabilità**: Posso aggiungere più processi facilmente
 *
 * SENZA PIPE:
 * ==========
 * - I processi non potrebbero comunicare
 * - Non sapremmo dove sono gli oggetti
 * - Non potremmo disegnare lo schermo
 * - Il gioco non funzionerebbe!
 */

// VARIABILI GLOBALI
int vite;                    // Vite rimanenti
int pipe_fds[2];            // Le due estremità della pipe (0=read, 1=write)
int flussi[N_FLUSSI];       // Direzione di ogni corsia (0=destra, 1=sinistra)
int tanen[5] = {0};         // Stato delle 5 tane (0=aperta, 1=chiusa)
int score;                  // Punteggio totale
struct point frogxy;        // Posizione e stato della rana

/*
 * LA GRANDE MATRICE DEL GIOCO - SPIEGAZIONE ULTRA-DETTAGLIATA
 * ==========================================================
 *
 * IMMAGINA: Hai un foglio di carta quadrettata ENORME dove segni
 * ogni cosa che succede nel tuo gioco. Ogni quadretto può contenere
 * al massimo UNA informazione alla volta.
 *
 * DIMENSIONI: [DIM_X+CROC_X*2][DIM_Y] = [187][42]
 * ================================================
 *
 * Perché 187 colonne? (Calcolo passo-passo)
 * ----------------------------------------
 * - DIM_X = 155 → La finestra visibile è 155 caratteri
 * - CROC_X = 16 → Un coccodrillo è largo 16 caratteri
 * - CROC_X*2 = 32 → Margini per coccodrilli che entrano/escono
 * - TOTALE = 187 → Spazio sufficiente per tutti gli oggetti
 *
 * Perché 42 righe?
 * ---------------
 * - DIM_Y = 42 → L'altezza totale della finestra di gioco
 * - Include: tane, riva, fiume (8 corsie), marciapiede
 *
 * OGNI CELLA CONTIENE UNA STRUCT msg:
 * ==================================
 *
 * struct msg {
 *     int id;          // "COSA c'è qui?" (FROG_ID, CROC_ID, ecc.)
 *     int x, y;        // Coordinate attuali (ridondanti, ma utili)
 *     char ch[4];      // "COME appare?" (il carattere/sprite)
 *     pid_t pid;       // "CHI lo controlla?" (ID del processo)
 *     int x_speed;     // Velocità orizzontale
 *     int y_speed;     // Velocità verticale (normalmente 0)
 *     bool first;      // È il primo messaggio di questo oggetto?
 * };
 *
 * ESEMPIO PRATICO - Come appare nella matrice:
 * ===========================================
 *
 * Immaginiamo che nella posizione [50][20] ci sia un coccodrillo:
 *
 * game_matrix[50][20] = {
 *     .id = CROC_ID,           // È un coccodrillo (valore 2)
 *     .x = 50,                 // Coordinata X corrente
 *     .y = 20,                 // Coordinata Y corrente
 *     .ch = "▄",               // Appare come un semicerchio
 *     .pid = 1234,             // Controllato dal processo 1234
 *     .x_speed = 1,            // Si muove a destra (1 pixel per frame)
 *     .y_speed = 0,            // Non si muove in verticale
 *     .first = false           // Non è il primo messaggio
 * };
 *
 * COME SI USA LA MATRICE:
 * ======================
 *
 * 1. **Per le collisioni**:
 *    - Controllo se game_matrix[x][y].id == FROG_ID
 *    - Controllo se game_matrix[x+1][y].id == CROC_ID
 *    - Distanza = game_matrix[croc_x][croc_y].x - frog_x
 *
 * 2. **Per il disegno**:
 *    - Leggo ogni cella da [CROC_X] a [DIM_X+CROC_X]
 *    - Disegno il carattere game_matrix[i][j].ch
 *    - Uso il PID per identificare l'oggetto
 *
 * 3. **Per gli aggiornamenti**:
 *    - Quando un oggetto si muove: cancello vecchia posizione
 *    - Scrivo nella nuova posizione
 *    - Aggiorno x, y, x_speed se necessario
 *
 * PERCHÉ È COSÌ IMPORTANTE?
 * ========================
 *
 * 1. **Unica fonte di verità**: Tutto quello che esiste nel gioco
 *    è rappresentato in questa matrice. Non ci sono altre strutture
 *    dati che tengono traccia delle posizioni.
 *
 * 2. **Sincronizzazione processi**: Ogni processo (rana, coccodrillo)
 *    invia la sua posizione qui, e il processo padre la usa per
 *    disegnare e controllare collisioni.
 *
 * 3. **Collisioni precise**: Posso controllare esattamente se due
 *    oggetti occupano la stessa cella o celle adiacenti.
 *
 * 4. **Disegno efficiente**: Invece di ridisegnare tutto ogni volta,
 *    leggo solo le celle che contengono oggetti.
 *
 * ESEMPIO DI UTILIZZO NEL GIOCO:
 * =============================
 *
 * Quando premi freccia destra:
 * 1. Processo rana calcola nuova posizione
 * 2. Processo rana invia messaggio al padre
 * 3. Processo padre riceve messaggio
 * 4. Processo padre chiama delete_old() per cancellare vecchie celle
 * 5. Processo padre chiama update_matrix_frog() per scrivere nuove celle
 * 6. Processo padre chiama draw_matrix() per disegnare
 *
 * È come un database centrale dove tutti gli oggetti "firmano il registro"
 * ogni volta che cambiano posizione!
 */

struct msg game_matrix[DIM_X+2*CROC_X][DIM_Y];  // LA GRANDE MATRICE DEL GIOCO!
struct game partita;        // Stato della partita corrente

/*
 * SPRITE - COME APPARIRANNO I PERSONAGGI
 * ======================================
 *
 * Gli sprite sono array di stringhe che formano l'aspetto dei personaggi.
 * Ogni elemento dell'array è una riga del disegno.
 */

// Sprite della rana (4 colonne x 3 righe)
char frog_sprite[FROG_Y*FROG_X][FROG_X] = {"▙", "█", "█" ,"▟", " ", "█", "█", " ", "█","▀", "▀", "█"};

// Sprite dei coccodrilli (2 versioni per direzione)
char croc_sprite[CROC_Y*CROC_X][CROC_X] = {
   "▄", "▄", "▄", "▄", "▮", "▃", "▃", "▃", "▃", "▁", " "," "," "," "," "," ",
   "▚", "▚", "▚", "▚", "▚", "█", "█", "█", "█", "█", "█", "█", "▅", "▄", "▃", "▁",
   "▀", "▀", "▀", "▀", "▀", "▐", "▛", "▀", "▀", "▐", "▛", "▀", "▀", "▀", "█", "◤"
};

char croc_sprite1[CROC_Y*CROC_X][CROC_X]={
   " "," "," "," "," "," ", "▁","▃","▃","▃","▃","▮","▄","▄","▄","▄",
   "▁","▃","▄","▅","█","█","█","█","█","█","█","▞","▞","▞","▞","▞",
   "◥","█","▀","▀","▀","▜","▋","▀","▀","▜","▋","▀","▀","▀","▀","▀"
};

/*
 * FUNZIONE MAIN - IL PUNTO DI PARTENZA
 * ====================================
 *
 * Questa è la prima funzione che viene chiamata quando avvii il programma.
 * È come la pagina iniziale di un libro.
 */

int main() {
   // PASSO 1: Prepara tutto l'ambiente di gioco
   // - Configura ncurses (grafica testuale)
   // - Imposta colori e caratteri speciali
   // - Prepara il sistema per input da tastiera
   game_init();

   // PASSO 2: Avvia il ciclo di vita del gioco
   // Questo gestisce: menu → gioco → game over → menu...
   game_polling();

   return 0;  // Programma terminato con successo
}

/*
 * INIZIALIZZAZIONE DEL GIOCO
 * ==========================
 *
 * Questa funzione prepara tutto l'ambiente di gioco.
 * È come preparare la cucina prima di cucinare.
 */

void game_init() {
   // Configura ncurses con le impostazioni che abbiamo definito
   setlocale(LC_ALL, "");        // Supporto per caratteri UTF-8
   initscr();                    // Inizializza la modalità schermo
   noecho();                     // Non mostrare i tasti premuti
   curs_set(CURS);               // Imposta visibilità cursore
   nodelay(stdscr, ND);          // Modalità input (bloccante/non bloccante)
   keypad(stdscr, KP);           // Abilita tasti speciali (frecce)
   cbreak();                     // Input carattere per carattere
   start_color();                // Abilita il sistema di colori

   // Definisci i colori personalizzati (al di fuori della tavolozza standard)
   init_color(VERDE_RANA, 100, 600, 0);      // Verde chiaro
   init_color(ROSSO_ARGINE, 320, 67, 141);   // Rosso scuro

   // Crea le COPPIE di colori (testo + sfondo)
   init_pair(COL_VITE, COLOR_RED, COLOR_BLACK);
   init_pair(COL_TIME, COLOR_BLACK, COLOR_RED);
   init_pair(COL_RIVA, COLOR_BLUE, ROSSO_ARGINE);
   init_pair(COL_MARCIAPIEDE, COLOR_BLACK, ROSSO_ARGINE);
   init_pair(COL_FIUME, COLOR_YELLOW, COLOR_BLUE);
   init_pair(COL_RANA_FIUME, VERDE_RANA, COLOR_BLUE);
   init_pair(COL_RANA_RIVA, VERDE_RANA, ROSSO_ARGINE);
   init_pair(COL_RANA_MARC, VERDE_RANA, ROSSO_ARGINE);
   init_pair(COL_COCCODRILLI, COLOR_BLACK, COLOR_BLUE);
   init_pair(COL_TANE, COLOR_BLACK, COLOR_YELLOW);
   init_pair(COL_RANA_CROC, VERDE_RANA, COLOR_BLACK);
   init_pair(COL_BULL_CROC, COLOR_YELLOW, COLOR_BLUE);
   init_pair(COL_BULL_RANA, COLOR_RED, COLOR_BLUE);
}

/*
 * LOOP PRINCIPALE DEL GIOCO - IL "CUORE" DEL PROGRAMMA
 * ===================================================
 *
 * QUESTA È LA FUNZIONE CHE CONTROLLA L'INTERA VITA DEL GIOCO!
 * ===========================================================
 *
 * Immagina: Il gioco è come un ristorante.
 * Questa funzione è il maître d'hôtel che accoglie i clienti,
 * li serve, e quando finiscono li saluta e aspetta i prossimi.
 *
 * IL CICLO DI VITA DEL GIOCO:
 * ==========================
 *
 * 1. **Benvenuto** → Il ristorante è aperto
 *    - Controlla che ci sia posto (schermo abbastanza grande)
 *    - Prepara il tavolo (finestra di gioco)
 *
 * 2. **Servizio** → Il cliente mangia (gioca)
 *    - Porta il menu (schermo di gioco)
 *    - Serve i piatti (manche di gioco)
 *    - Prende le ordinazioni (input giocatore)
 *
 * 3. **Conto e arrivederci** → Il cliente paga e va via
 *    - Mostra il conto (risultato partita)
 *    - Saluta e aspetta il prossimo cliente
 *    - Se il ristorante chiude, fine
 *
 * VARIABILE wanna_play:
 * ===================
 * - true = Il ristorante è aperto, aspettiamo clienti
 * - false = Il cliente ha finito, aspettiamo di sapere se vuole tornare
 *
 * PERCHÉ UN while(true) INFINITO?
 * ==============================
 * - Il gioco deve continuare finché l'utente non decide di smettere
 * - Ogni partita è come un cliente diverso
 * - Quando una partita finisce, il ciclo ricomincia per il prossimo "cliente"
 */

void game_polling() {
   /*
    * FASE 1: PREPARAZIONE - "Apriamo il ristorante"
    * =============================================
    *
    * Immagina di aprire un ristorante ogni giorno:
    * - Controlli che il locale sia abbastanza grande
    * - Prepari i tavoli (finestre)
    * - Aspetti i clienti
    */

   bool wanna_play = true;  // Il ristorante è aperto!

   // Continua finché qualcuno non chiude il ristorante
   while (wanna_play) {
     /*
      * PREPARAZIONE PER IL PROSSIMO CLIENTE
      * ===================================
      *
      * Ogni partita è come un nuovo cliente:
      * - Prima assumiamo che voglia giocare
      * - Durante il gioco potrebbe cambiare idea
      */
     wanna_play = false;  // Resetta per il prossimo giro

     /*
      * CONTROLLO LOCALE - "È abbastanza grande?"
      * ========================================
      *
      * Prima di far entrare un cliente, controlla che ci sia posto.
      * Se lo schermo è troppo piccolo, non possiamo servire bene.
      */
     screen_size_loop();  // Controlla dimensioni schermo

     /*
      * PREPARAZIONE TAVOLO - "Sistemiamo il tavolo"
      * ===========================================
      *
      * Crea la finestra di gioco dove il cliente "mangerà" (giocherà).
      * È come apparecchiare una bella tavola.
      */
     create_game_win();   // Crea finestra centrata

     /*
      * SERVIZIO AL TAVOLO - "Serviamo il cliente"
      * =========================================
      *
      * Finalmente possiamo servire il cliente!
      * Questa è la parte principale: far giocare la partita.
      */
     run();  // Gioca la partita completa

     /*
      * NOTA IMPORTANTE:
      * ===============
      *
      * Quando run() finisce, il ciclo ricomincia dal while(wanna_play).
      * Se durante il gioco il giocatore ha deciso di smettere,
      * wanna_play sarà false e il ciclo finisce.
      *
      * È come: "Cliente servito, tavolo libero per il prossimo!"
      */
   }

   /*
    * CHIUSURA RISTORANTE
    * ==================
    *
    * Se siamo usciti dal while, significa che il ristorante chiude.
    * Il gioco finisce qui.
    */
}

/*
 * CONTROLLO DIMENSIONI SCHERMO
 * =============================
 *
 * Assicurati che la finestra del terminale sia abbastanza grande
 * per il nostro gioco, altrimenti mostra un messaggio di avviso
 */

void screen_size_loop() {
   // Continua a controllare finché lo schermo non è abbastanza grande
   while (!is_screen_size_ok()) {
          // Mostra messaggio di avviso al centro dello schermo
          mvprintw(get_screen_size().y/2, get_screen_size().x/2 - 10,
                   AVVISO_DIM_SCHERMO);
          refresh();  // Aggiorna lo schermo per mostrare il messaggio
       }
}

/*
 * VERIFICA DIMENSIONI SCHERMO
 * ===========================
 *
 * Controlla se le dimensioni attuali del terminale sono sufficienti
 */

bool is_screen_size_ok() {
   bool flag = true;  // Inizialmente assumiamo che vada bene

   // Se lo schermo è troppo piccolo in qualsiasi direzione
   if (get_screen_size().x <= DIM_X || get_screen_size().y <= DIM_Y) {
       flag = false;  // Non va bene!
   }

   return flag;
}

/*
 * OTTIENI DIMENSIONI SCHERMO
 * ==========================
 *
 * Chiede al sistema le dimensioni attuali del terminale
 */

struct point get_screen_size() {
   struct point maxxy;  // Struttura per memorizzare le dimensioni

   // Chiedi a ncurses le dimensioni massime
   getmaxyx(stdscr, maxxy.y, maxxy.x);

   return maxxy;
}

/*
 * CREA FINESTRA DI GIOCO
 * ======================
 *
 * Crea la finestra principale dove si svolgerà il gioco
 */

void create_game_win() {
   // Crea una finestra di dimensioni DIM_Y x DIM_X
   // posizionata al centro dello schermo
   gamewin = newwin(DIM_Y, DIM_X,
                   generate_win_start().y,
                   generate_win_start().x);

   // Disegna un bordo attorno alla finestra
   box(gamewin, 0, 0);
}

/*
 * CALCOLA POSIZIONE FINESTRA
 * ==========================
 *
 * Calcola dove posizionare la finestra di gioco per centrarla
 */

struct point generate_win_start() {
   struct point upper_left_corner;

   // Ottieni dimensioni schermo
   upper_left_corner = get_screen_size();

   // Calcola posizione per centrare la finestra
   upper_left_corner.y = (upper_left_corner.y - DIM_Y) / 2;
   upper_left_corner.x = (upper_left_corner.x - DIM_X) / 2;

   return upper_left_corner;
}

/*
 * INIZIALIZZAZIONE DEI PROCESSI
 * =============================
 *
 * Ogni "oggetto" del gioco (rana, coccodrilli, proiettili)
 * diventa un processo separato che "vive" per conto suo.
 */

void init_processes() {
   // PASSO 1: Crea il processo per la rana
   // La rana ascolterà i comandi da tastiera
   generate_frog();

   // PASSO 2: Crea il processo "fabbrica di coccodrilli"
   // Questo processo creerà altri processi per i coccodrilli
   generate_croc_father();
}

/*
 * INIZIALIZZAZIONE DEI FLUSSI
 * ===========================
 *
 * Ogni corsia del fiume può andare in direzioni diverse.
 * Questa funzione decide casualmente la direzione di ogni corsia.
 */

void init_flussi() {
   // Usa il tempo corrente come seme per la casualità
   srand(time(NULL));

   // Scegli una direzione base (0=destra, 1=sinistra)
   int r = rand() % 2;

   // Per ogni corsia del fiume
   for (size_t i = 0; i < N_FLUSSI; i++) {
     if (r == 0) {
        // Pattern: destra, sinistra, destra, sinistra...
        flussi[i] = (i % 2 == 0) ? (0) : (1);
     }
     else {
        // Pattern opposto: sinistra, destra, sinistra, destra...
        flussi[i] = (i % 2 == 0) ? (1) : (0);
     }
   }
}

/*
 * RICEZIONE DATI DAI PROCESSI
 * ===========================
 *
 * Questa funzione ascolta i messaggi che arrivano dalla pipe.
 * Ogni processo (rana, coccodrillo, proiettile) invia la sua posizione.
 */

void get_data() {
   struct msg temp;  // Buffer temporaneo per il messaggio

   // Leggi un messaggio dalla pipe
   read(pipe_fds[0], &temp, sizeof(temp));

   // Elabora il messaggio ricevuto
   update(temp);
}

/*
 * ELABORAZIONE MESSAGGI
 * =====================
 *
 * Questa funzione decide cosa fare con ogni messaggio ricevuto.
 * È come un centralino che smista le chiamate.
 */

void update(struct msg temp) {
   // In base al tipo di oggetto che ha inviato il messaggio...
   switch(temp.id) {

   // CASO 1: Messaggio dalla rana
   case FROG_ID:
        // Aggiorna la posizione della rana nella matrice
        update_frog(temp.x, temp.y, temp.pid);
        update_matrix_frog(true);  // Ricalcola la matrice per la rana
        break;

   // CASO 2: Messaggio da un coccodrillo
      case CROC_ID:
         // Aggiorna la matrice con la nuova posizione del coccodrillo
         update_matrix_croc(temp);
         break;

   // CASO 3: Messaggio da un proiettile della rana
      case BULL_ID:
         // Gestisci il movimento del proiettile
         update_matrix_bull(temp);
         break;

   // CASO 4: Messaggio da un proiettile dei coccodrilli
      case BULL_CROC_ID:
         // Gestisci collisione con il proiettile nemico
         update_matrix_croc_bull(temp);
         break;

   // Se non riconosci il tipo, non fare nulla
      default:
        break;
   }
}

/*
 * AGGIORNAMENTO POSIZIONE RANA
 * ============================
 *
 * Quando la rana si muove, dobbiamo aggiornare la sua posizione
 * e controllare se è ancora viva.
 */

void update_frog(int x, int y, pid_t pid) {
   // Sposta la rana della quantità richiesta
   frogxy.x += x;
   frogxy.y += y;
   frogxy.pid = pid;

   // Segna che la posizione è cambiata (per il disegno)
   if (x != 0 || y != 0) {
       frogxy.change = true;
   } else {
       frogxy.change = false;
   }
}

/*
 * AGGIORNAMENTO MATRICE RANA
 * ==========================
 *
 * Dopo aver mosso la rana, dobbiamo aggiornare la grande matrice
 * che tiene traccia di dove si trova ogni oggetto.
 */

void update_matrix_frog(bool need) {
   // PASSO 1: Cancella la vecchia posizione della rana
   delete_old(frogxy.pid, true);

   // PASSO 2: Inserisci la rana nella nuova posizione
   for(size_t i = 0; i < FROG_X; i++) {      // Per ogni colonna della rana
      for(size_t j = 0; j < FROG_Y; j++) {   // Per ogni riga della rana
         // Se non c'è già un proiettile in quella posizione
         if (game_matrix[frogxy.x+i][frogxy.y+j].id != BULL_ID) {
            // Inserisci la rana nella matrice
            game_matrix[frogxy.x+i][frogxy.y+j].id = FROG_ID;
            game_matrix[frogxy.x+i][frogxy.y+j].pid = frogxy.pid;
            // Usa lo sprite appropriato per questa parte della rana
            strcpy(game_matrix[frogxy.x+i][frogxy.y+j].ch,
                   frog_sprite[i+FROG_X*j]);
         }
      }
   }

   // PASSO 3: Controlla se la rana è morta
   if ((frog_on_water() && need) || frog_is_out_of_bounds()) {
      // Uccidi il processo della rana
      kill_process(frogxy.pid);
      // Segna la manche come persa
      partita.manche_on = false;
      partita.loss = true;
   }
}

/*
 * CANCELLAZIONE POSIZIONI VECCHIE
 * ===============================
 *
 * Quando un oggetto si muove, dobbiamo cancellare la sua vecchia posizione
 * dalla matrice, altrimenti rimarrebbe "fantasma" nella posizione precedente.
 */

void delete_old(pid_t pid, bool frog) {
   // Scandisci tutta la matrice
   for(size_t i = 0; i < DIM_X + CROC_X*2; i++) {
      for(size_t j = 0; j < DIM_Y; j++) {
         // Se trovi l'oggetto che si è mosso
         if (pid == game_matrix[i][j].pid) {
            // Se è la rana E la rana è su un coccodrillo
            if (frog && frogxy.on_croc) {
               // Sostituisci la rana con il coccodrillo sottostante
               game_matrix[i][j].id = CROC_ID;
               game_matrix[i][j].pid = -1;
            }
            // Se è un proiettile sulla rana (collisione)
            else if (game_matrix[i][j].id == BULL_ID) {
               // Mantieni il coccodrillo sottostante
               game_matrix[i][j].id = CROC_ID;
               game_matrix[i][j].pid = -1;
            }
            else {
               // Cancella completamente la posizione
               game_matrix[i][j].pid = -1;
               game_matrix[i][j].id = -1;
            }
         }
      }
   }
}

/*
 * CONTROLLO COLLISIONI - RANA IN ACQUA
 * ====================================
 *
 * Controlla se la rana è in acqua senza essere su un coccodrillo.
 * Se sì, la rana annega!
 */

bool frog_on_water() {
   // Controlla sempre se la rana è su un coccodrillo
   bool is_on = is_frog_on_croc();

   // Se la rana è nel fiume E non è su un coccodrillo
   if (frogxy.y < MARCIAPIEDESY && frogxy.y > FIUMESY && !is_on) {
      return true;  // Rana in acqua = morte!
   }

   return false;
}

/*
 * CONTROLLO RANA SU COCCODRILLO - SPIEGAZIONE ULTRA-DETTAGLIATA
 * ============================================================
 *
 * QUESTA È LA FUNZIONE PIÙ COMPLESSA DEL GIOCO!
 * ==============================================
 *
 * Immagina: La rana salta su un coccodrillo che nuota.
 * Il coccodrillo è largo 16 caratteri, la rana è larga 4.
 * La rana deve essere COMPLETAMENTE sostenuta dal coccodrillo,
 * altrimenti cade in acqua e affoga.
 *
 * Il controllo è a TRE LIVELLI:
 * ============================
 * 1. **croc_on_both()** → La rana è completamente sui coccodrilli?
 * 2. **croc_on_right()** → Solo la parte destra è su un coccodrillo?
 * 3. **croc_on_left()** → Solo la parte sinistra è su un coccodrillo?
 * 4. **frog_is_drowning()** → Anche se è su un coccodrillo, annega?
 *
 * LOGICA PASSO-PASSO:
 * ==================
 *
 * 1. **Controllo completo (entrambe le parti)**:
 *    - La rana deve avere coccodrillo sotto TUTTI i suoi 4 caratteri
 *    - Sia sotto frogxy.x che sotto frogxy.x+3 deve esserci un coccodrillo
 *    - Per ogni riga della rana (3 righe totali)
 *
 * 2. **Controllo parziale destro**:
 *    - La parte sinistra della rana (frogxy.x) deve essere su coccodrillo
 *    - La parte destra deve avere un coccodrillo abbastanza grande
 *    - È come stare sul bordo di una zattera
 *
 * 3. **Controllo parziale sinistro**:
 *    - Simmetrico al controllo destro, ma per la parte sinistra
 *
 * 4. **Controllo annegamento**:
 *    - Anche se la rana è "tecnicamente" su un coccodrillo,
 *      potrebbe esserci un buco dove cade l'acqua
 *    - Controlla se c'è acqua sotto le zampe della rana
 */

bool is_frog_on_croc() {
   /*
    * ESPRESSIONE COMPLESSA SPIEGATA:
    * ==============================
    *
    * (croc_on_both() || croc_on_right() || croc_on_left())
    * && (!frog_is_drowning())
    *
    * SIGNIFICA:
    * "La rana è su un coccodrillo in almeno uno dei tre modi
    *  E NON sta annegando"
    */

   if ((croc_on_both() || croc_on_right() || croc_on_left())
       && (!frog_is_drowning())) {

       /*
        * SUCCESSO! La rana è al sicuro
        * =============================
        *
        * Segno che la rana è su un coccodrillo e memorizzo
        * quale coccodrillo la sta trasportando (per il movimento)
        */
       frogxy.on_croc = true;
       return true;
   }
   else {
       /*
        * FALLIMENTO! La rana è in pericolo
        * =================================
        *
        * La rana potrebbe essere:
        * - Completamente in acqua
        * - Su un coccodrillo ma in una posizione precaria
        * - Su un coccodrillo che ha un buco
        */
       return false;
   }
}

/*
 * CONTROLLO ANNEgAMENTO
 * =====================
 *
 * Anche se la rana è su un coccodrillo, potrebbe comunque annegare
 * se non è completamente coperta dal coccodrillo.
 */

bool frog_is_drowning() {
   // Controlla le posizioni immediatamente a sinistra e destra della rana
   if (game_matrix[frogxy.x-1][frogxy.y].id != CROC_ID &&
       game_matrix[frogxy.x+FROG_X][frogxy.y].id != CROC_ID) {
      return true;  // Nessun coccodrillo sotto i piedi = annegamento!
   }
   return false;
}

/*
 * CONTROLLO COMPLETO - ENTRAMBE LE PARTI SU COCCODRILLO
 * ====================================================
 *
 * La rana è completamente su un coccodrillo se TUTTE le sue
 * parti inferiori sono sopra parti di coccodrillo.
 */

bool croc_on_both() {
   // Memorizza il PID del coccodrillo che sta trasportando la rana
   frogxy.croc_pid = game_matrix[frogxy.x-1][frogxy.y].pid;

   // Controlla ogni riga della rana
   for (size_t i = frogxy.y; i < frogxy.y+FROG_Y; i++) {
      // Ogni riga deve avere coccodrillo sia a sinistra che a destra
      if ((game_matrix[frogxy.x-1][i].id != CROC_ID &&
           game_matrix[frogxy.x-1][i].id != BULL_ID) ||
          (game_matrix[frogxy.x+FROG_X][i].id != CROC_ID &&
           game_matrix[frogxy.x+FROG_X][i].id != BULL_ID)) {
         return false;  // Trovato un buco!
        }
      }

   // Tutto OK, la rana è salva
   frogxy.croc_pid = game_matrix[frogxy.x-1][frogxy.y].pid;
   return true;
}

/*
 * CONTROLLO PARZIALE - SOLO PARTE DESTRA SU COCCODRILLO
 * ====================================================
 *
 * La rana può essere su un coccodrillo anche se solo metà del
 * suo corpo è coperta, ma deve esserci un coccodrillo abbastanza grande.
 */

bool croc_on_right() {
   // Controlla ogni riga della rana
   for (size_t i = frogxy.y; i < frogxy.y+FROG_Y; i++) {
      // La parte sinistra deve essere su coccodrillo
      if (game_matrix[frogxy.x-1][i].id != CROC_ID &&
          game_matrix[frogxy.x-1][i].id != BULL_ID) {
         return false;  // La parte sinistra non è coperta!
        }
    }

   // Controlla se c'è un coccodrillo abbastanza grande
   if ((game_matrix[frogxy.x+FROG_X-CROC_X][frogxy.y].id == CROC_ID ||
        game_matrix[frogxy.x+FROG_X-CROC_X][frogxy.y].id == BULL_ID) &&
       (game_matrix[frogxy.x-CROC_X+1][frogxy.y].id != CROC_ID &&
        game_matrix[frogxy.x-CROC_X+1][frogxy.y].id != BULL_ID)) {

       // OK, c'è un coccodrillo abbastanza grande
       frogxy.croc_pid = game_matrix[frogxy.x-1][frogxy.y].pid;
       return true;
   }

   return false;
}

/*
 * CONTROLLO PARZIALE - SOLO PARTE SINISTRA SU COCCODRILLO
 * =======================================================
 *
 * Simmetrico al controllo destro, ma per la parte sinistra.
 */

bool croc_on_left() {
   // Controlla ogni riga della rana
   for (size_t i = frogxy.y; i < frogxy.y+FROG_Y; i++) {
         // La parte destra deve essere su coccodrillo
         if (game_matrix[frogxy.x+FROG_X][i].id != CROC_ID &&
             game_matrix[frogxy.x+FROG_X][i].id != BULL_ID) {
           return false;  // La parte destra non è coperta!
           }
       }

   // Controlla se c'è un coccodrillo abbastanza grande
   if ((game_matrix[frogxy.x+CROC_X-1][frogxy.y].id == CROC_ID ||
        game_matrix[frogxy.x+CROC_X-1][frogxy.y].id == BULL_ID ) &&
       (game_matrix[frogxy.x+CROC_X+FROG_X-1][frogxy.y].id != CROC_ID &&
        game_matrix[frogxy.x+CROC_X+FROG_X-1][frogxy.y].id != BULL_ID)) {

       // OK, c'è un coccodrillo abbastanza grande
       frogxy.croc_pid = game_matrix[frogxy.x+FROG_X][frogxy.y].pid;
       return true;
   }

   return false;
}

/*
 * CONTROLLO COCCODRILLO FUORI SCHERMO
 * ===================================
 *
 * Quando un coccodrillo esce completamente dallo schermo,
 * deve essere eliminato.
 */

bool croc_is_out_of_bounds(struct msg temp) {
   // Se il coccodrillo è andato troppo a destra OPPURE troppo a sinistra
   if ((temp.x > DIM_X-1+CROC_X && temp.x_speed > 0) ||
       (temp.x <= 0 && temp.x_speed < 0)) {
      return true;   // Fuori schermo, deve morire
   }
   return false;
}

/*
 * CONTROLLO RANA FUORI SCHERMO
 * ============================
 *
 * La rana non deve poter uscire dai bordi del campo.
 */

bool frog_is_out_of_bounds() {
   // La rana deve stare entro i margini sicuri
   if (frogxy.x < CROC_X+1 || frogxy.x > CROC_X+DIM_X-FROG_X-1) {
      return true;  // Fuori dai margini!
   }
   return false;
}

/*
 * CONTROLLO VITTORIA - RANA IN TANA
 * ================================
 *
 * Se la rana arriva in una tana vuota, vince quella manche.
 */

bool check_frog_win() {
   // Controlla ognuna delle 5 tane
   for (size_t i = 0; i < 5; i++) {
      // Se la rana è nella zona di questa tana
      if ((frogxy.x >= TANESX+12*(i+1)+TANE_LARGE*i &&
           frogxy.x < TANESX+12*(i+1)+TANE_LARGE*(i+1)) &&
          frogxy.y < RIVASY) {

          // Se la tana è ancora vuota
          if (tanen[i] == 0) {
              partita.manche_on = false;  // Fine manche
              partita.loss = false;      // Vittoria!
              tanen[i] = 1;              // Chiudi la tana
          }
          else {
              // Tana già chiusa = morte
              partita.manche_on = false;
              partita.loss = true;
          }
      }
   }
   return false;  // Non è necessario il return, ma per completezza
}

/*
 * CONTROLLO PROIETTILE FUORI SCHERMO
 * =================================
 *
 * I proiettili devono morire quando escono dallo schermo.
 */

bool bullet_is_out_of_bounds(int x) {
   // I proiettili hanno margini più stretti
   if ((x > DIM_X+CROC_X) || (x < CROC_X)) {
       return true;  // Fuori schermo
   }
   return false;
}

/*
 * CONTROLLO COLLISIONE PROIETTILE-RANA
 * ===================================
 *
 * Se un proiettile nemico colpisce la rana, la rana muore.
 */

bool frog_collision(int x, int y) {
   // Se nella posizione del proiettile c'è la rana
   if (game_matrix[x][y].id == FROG_ID) {
      return true;  // Collisione fatale!
   }
   return false;
}

/*
 * AGGIORNAMENTO MATRICE PROIETTILI COCCODRILLI
 * ===========================================
 *
 * Gestisce il movimento e le collisioni dei proiettili sparati dai coccodrilli.
 * È una funzione molto importante per il gameplay.
 */

void update_matrix_croc_bull(struct msg temp) {
   // PASSO 1: Cancella la vecchia posizione del proiettile
   delete_old(temp.pid, false);

   // PASSO 2: Controlla se il proiettile è uscito dallo schermo
   if (bullet_is_out_of_bounds(temp.x)) {
      // Se è fuori, uccidi il processo del proiettile
      kill_process(temp.pid);
   } else {
      // PASSO 3: Controlla collisioni
      // Se nella nuova posizione c'è già un proiettile della rana
      if (game_matrix[temp.x][temp.y].id == BULL_ID && game_matrix[temp.x][temp.y].pid > 0) {
         // Collisione! Distruggi entrambi i proiettili
         kill_process(temp.pid);                    // Uccidi proiettile coccodrillo
         kill_process(game_matrix[temp.x][temp.y].pid); // Uccidi proiettile rana
         delete_old(game_matrix[temp.x][temp.y].pid, false); // Cancella dalla matrice
      }
      // Se il proiettile colpisce la rana
      else if (frog_collision(temp.x, temp.y)) {
         // Game over! Uccidi la rana
         kill_process(frogxy.pid);
         partita.manche_on = false;  // Fine manche
         partita.loss = true;       // Sconfitta
      }
      // Se tutto OK, inserisci il proiettile nella matrice
      else {
         // Inserisci il proiettile nella nuova posizione
         game_matrix[temp.x][temp.y].id = BULL_CROC_ID;  // Tipo: proiettile coccodrillo
         game_matrix[temp.x][temp.y].pid = temp.pid;     // PID del processo

         // Scegli il simbolo giusto in base alla direzione
         wattron(gamewin, COLOR_PAIR(COL_TANE));  // Colore per proiettili
         if (temp.x_speed > 0) {
            strcpy(game_matrix[temp.x][temp.y].ch, RIGHT_BULL); // Freccia destra
         } else {
            strcpy(game_matrix[temp.x][temp.y].ch, LEFT_BULL);  // Freccia sinistra
         }
         wattroff(gamewin, COLOR_PAIR(COL_TANE));
      }
   }
}

/*
 * AGGIORNAMENTO MATRICE PROIETTILI RANA
 * ====================================
 *
 * Gestisce i proiettili sparati dalla rana (quando premi TAB).
 * Questa funzione è un po' più complessa perché deve gestire
 * il primo messaggio e i successivi in modo diverso.
 */

void update_matrix_bull(struct msg temp) {
   // Se è il primo messaggio del proiettile
   if (temp.first) {
      // Calcola la posizione di partenza del proiettile
      // Se x_speed > 0 (destra): spara da destra della rana
      // Se x_speed < 0 (sinistra): spara da sinistra della rana
      if (temp.x_speed > 0) {
         game_matrix[frogxy.x+FROG_X][temp.y].id = BULL_ID;
         game_matrix[frogxy.x+FROG_X][temp.y].pid = temp.pid;
         strcpy(game_matrix[frogxy.x+FROG_X][temp.y].ch, SYMB_BULL);
      } else {
         game_matrix[frogxy.x-1][temp.y].id = BULL_ID;
         game_matrix[frogxy.x-1][temp.y].pid = temp.pid;
         strcpy(game_matrix[frogxy.x-1][temp.y].ch, SYMB_BULL);
      }
   } else {
      // Non è il primo messaggio - il proiettile si sta muovendo
      for (int i = 0; i < CROC_X*2+DIM_X; i++) {
         // Trova la posizione attuale del proiettile
         if (game_matrix[i][temp.y].pid == temp.pid) {
            // Cancella la posizione vecchia
            delete_old(temp.pid, false);

            // Controlla se è uscito dai bordi
            if (bullet_is_out_of_bounds(i+temp.x_speed)) {
               // Se è fuori, uccidi il processo
               kill_process(temp.pid);
            } else {
               // Se è ancora dentro, inserisci nella nuova posizione
               game_matrix[i+temp.x_speed][temp.y].id = BULL_ID;
               game_matrix[i+temp.x_speed][temp.y].pid = temp.pid;
               strcpy(game_matrix[i+temp.x_speed][temp.y].ch, SYMB_BULL);
            }
            // Esci dal ciclo (abbiamo trovato e gestito il proiettile)
            i = CROC_X*2+DIM_X;
         }
      }
   }
}

/*
 * CANCELLAZIONE POSIZIONI VECCHIE - VERSIONE COMPLETA
 * ==================================================
 *
 * Questa è la versione completa della funzione che cancella
 * le posizioni vecchie dalla matrice di gioco.
 */

void delete_old(pid_t pid, bool frog) {
   // Scandisci tutta la matrice di gioco
   for(size_t i = 0; i < DIM_X + CROC_X*2; i++) {
      for(size_t j = 0; j < DIM_Y; j++) {
         // Se trovi la cella che appartiene al processo che si sta muovendo
         if (pid == game_matrix[i][j].pid) {
            // Caso speciale: se è la rana E la rana è su un coccodrillo
            if (frog && frogxy.on_croc) {
               // Invece di cancellare, sostituisci con il coccodrillo
               game_matrix[i][j].id = CROC_ID;
               game_matrix[i][j].pid = -1;
            }
            // Se c'è un proiettile nella stessa posizione
            else if (game_matrix[i][j].id == BULL_ID) {
               // Mantieni il coccodrillo sottostante
               game_matrix[i][j].id = CROC_ID;
               game_matrix[i][j].pid = -1;
            }
            else {
               // Caso normale: cancella completamente la cella
               game_matrix[i][j].pid = -1;
               game_matrix[i][j].id = -1;
            }
         }
      }
   }
}

/*
 * AGGIORNAMENTO MATRICE COCCODRILLI - VERSIONE COMPLETA
 * ====================================================
 *
 * Questa è la versione completa che gestisce tutti gli aspetti
 * del movimento dei coccodrilli.
 */

void update_matrix_croc(struct msg temp) {
   // PASSO 1: Cancella la posizione vecchia del coccodrillo
   delete_old(temp.pid, false);

   // PASSO 2: Controlla se il coccodrillo è uscito dallo schermo
   if (croc_is_out_of_bounds(temp)) {
      kill_process(temp.pid);  // Uccidi il processo
   } else {
      // PASSO 3: Inserisci il coccodrillo nella nuova posizione
      for(size_t i = 0; i < CROC_X; i++) {      // Per ogni colonna del coccodrillo
         for(size_t j = 0; j < CROC_Y; j++) {   // Per ogni riga del coccodrillo
            // Non sovrascrivere proiettili esistenti
            if (game_matrix[temp.x+i][temp.y+j].id != BULL_ID) {
               // Inserisci il coccodrillo
               game_matrix[temp.x+i][temp.y+j].id = CROC_ID;
               game_matrix[temp.x+i][temp.y+j].pid = temp.pid;

               // Scegli lo sprite in base alla direzione
               if (temp.x_speed > 0) {
                  // Si muove a destra: usa sprite destra
                  strcpy(game_matrix[temp.x+i][temp.y+j].ch, croc_sprite1[i+CROC_X*j]);
               } else {
                  // Si muove a sinistra: usa sprite sinistra
                  strcpy(game_matrix[temp.x+i][temp.y+j].ch, croc_sprite[i+CROC_X*j]);
               }
            }
         }
      }
   }

   // PASSO 4: Controlla sempre lo stato della rana
   if ((frog_on_water()) || frog_is_out_of_bounds()) {
      kill_process(frogxy.pid);
      partita.manche_on = false;
      partita.loss = true;
   }

   // PASSO 5: Aggiorna la posizione della rana se è su questo coccodrillo
   is_frog_on_croc();
   if (temp.pid == frogxy.croc_pid) {
      update_frog(temp.x_speed, 0, frogxy.pid);
      update_matrix_frog(true);
   }

   // PASSO 6: Verifica che la rana sia ancora sul coccodrillo
   if (!is_frog_on_croc()) {
      frogxy.on_croc = false;
      frogxy.croc_pid = -1;
   }
}

/*
 * PROCESSO COCCODRILLO - SPIEGAZIONE DETTAGLIATA
 * ===============================================
 *
 * IMMAGINA: Hai un robot coccodrillo che deve nuotare nel fiume.
 * Questo robot è completamente autonomo e deve:
 * 1. Sapere dove si trova
 * 2. Muoversi da solo
 * 3. Dire al "capo" (processo padre) dove si trova
 * 4. Ogni tanto sparare un proiettile
 * 5. Continuare all'infinito
 *
 * Questo processo è come un lavoratore in una fabbrica che:
 * - Ha il suo compito specifico (nuotare nella sua corsia)
 * - Rapporta regolarmente al capo (invia posizione)
 * - Prende decisioni autonome (quando sparare)
 * - Non sa cosa fanno gli altri lavoratori
 */

void croc(int flusso) {
   /*
    * PASSO 1: PREPARAZIONE - "Impostare la scena"
    * ===========================================
    *
    * Il coccodrillo deve sapere:
    * - In quale corsia si trova (flusso)
    * - Dove deve iniziare (fuori dallo schermo)
    * - In che direzione deve andare
    */

   // Creo una "busta" per spedire messaggi al processo padre
   struct msg m;

   // CALCOLO DELLA POSIZIONE Y (verticale)
   // Ogni corsia è alta FLUX_Y pixel (normalmente 3 pixel)
   // flusso=1 → prima corsia (la più bassa)
   // flusso=2 → seconda corsia, più in alto di FLUX_Y, ecc.
   m.y = MARCIAPIEDESY - flusso * FLUX_Y;

   // CALCOLO DELLA POSIZIONE X (orizzontale)
   // I coccodrilli iniziano FUORI dallo schermo
   // flussi[flusso-1] è 0 per destra, 1 per sinistra
   // Se va a destra: inizia a sinistra dello schermo (-CROC_X)
   // Se va a sinistra: inizia a destra dello schermo (+DIM_X)
   m.x = flussi[flusso-1] * (DIM_X + CROC_X);

   // Dico al mondo che io sono un coccodrillo
   m.id = CROC_ID;

   // Ogni processo ha un numero identificativo unico (PID)
   m.pid = getpid();

   // VELOCITÀ: Determina la direzione
   // Se flussi[flusso-1] è 0 → va a destra (+CROC_SPEED)
   // Se flussi[flusso-1] è 1 → va a sinistra (-CROC_SPEED)
   m.x_speed = (flussi[flusso-1] == 0) ? (CROC_SPEED) : (-CROC_SPEED);

   // Ogni coccodrillo ha il suo generatore di numeri casuali
   // Usa il PID come seme per avere sequenze diverse
   srand(m.pid);

   /*
    * PASSO 2: CICLO DI VITA - "Il lavoro quotidiano"
    * ===============================================
    *
    * Questo ciclo while(true) è il "cuore" del coccodrillo.
    * È come un nuotatore che:
    * 1. Dice dove si trova
    * 2. Pensa se sparare
    * 3. Nuota un po'
    * 4. Riposa
    * 5. Riparte da capo
    */

   while(true) {
      /*
       * 2A: RAPPORTO POSIZIONE - "Dico al capo dove sono"
       * ================================================
       *
       * Il coccodrillo deve sempre far sapere al processo padre
       * dove si trova attualmente. È come un GPS che invia
       * la posizione ogni secondo.
       */
      write(pipe_fds[1], &m, sizeof(m));

      /*
       * 2B: DECISIONE DI SPARARE - "Tirare a sorte"
       * ===========================================
       *
       * Il coccodrillo decide se sparare in modo casuale.
       * È come lanciare una moneta: se esce testa, spari.
       *
       * BULL_CHANCE è 100, quindi:
       * - rand() % 100 genera un numero da 0 a 99
       * - Se è 0 (1 possibilità su 100), SPARA!
       * - Altrimenti, continua a nuotare
       */
      if (rand() % BULL_CHANCE == 0) {  // 1 su 100 possibilità
         // Creo un NUOVO processo per il proiettile
         // Questo proiettile avrà vita propria
         generate_process(BULL_CROC_ID, -1, m.x, m.y, m.x_speed);
      }

      /*
       * 2C: MUOVIMENTO - "Nuota un po'"
       * ===============================
       *
       * Il coccodrillo si sposta nella sua direzione
       * m.x_speed è positivo per destra, negativo per sinistra
       */
      m.x += m.x_speed;

      /*
       * 2D: RIPOSO - "Prendi fiato"
       * ===========================
       *
       * Il coccodrillo si ferma per un po' prima del prossimo movimento.
       * CROC_SLEEP è 90000 microsecondi (0.09 secondi)
       * Questo determina quanto velocemente nuota il coccodrillo.
       */
      usleep(CROC_SLEEP);
   }

   /*
    * NOTA IMPORTANTE:
    * ===============
    *
    * Questo ciclo NON HA FINE! Il coccodrillo continuerà
    * a nuotare per sempre, o finché il processo padre
    * non lo "uccide" con kill_process().
    *
    * È come un nuotatore che fa vasche all'infinito
    * nella piscina, finché l'allenatore non lo ferma.
    */
}

/*
 * PROCESSO PROIETTILE COCCODRILLO
 * ==============================
 *
 * Ogni proiettile sparato da un coccodrillo diventa un processo separato.
 */

void bullet_croc(int x, int y, int speed) {
   // Prepara il messaggio del proiettile
   struct msg m;

   // Imposta direzione e posizione di partenza
   if (speed > 0) {
      m.x_speed = 1;        // Si muove a destra
      m.x = x + CROC_X;     // Parte dalla bocca destra del coccodrillo
   } else {
      m.x_speed = -1;       // Si muove a sinistra
      m.x = x;              // Parte dalla bocca sinistra
   }

   m.pid = getpid();       // PID del processo proiettile
   m.y = y + 1;           // Un po' sotto il coccodrillo
   m.id = BULL_CROC_ID;   // Tipo: proiettile coccodrillo

   // Ciclo di vita del proiettile
   while (true) {
      // Invia posizione al processo padre
      write(pipe_fds[1], &m, sizeof(m));

      // Muovi il proiettile
      m.x += m.x_speed;

      // Pausa più breve dei coccodrilli (i proiettili sono più veloci)
      usleep(BULL_SLEEP/2);
   }
}

/*
 * FABBRICA DI COCCODRILLI
 * ======================
 *
 * Questo processo crea continuamente nuovi coccodrilli in corsie diverse.
 * È come una fabbrica che produce coccodrilli.
 */

void croc_creator() {
   int rand_flux;         // Corsia casuale scelta
   int prev_rand_flux = -1; // Ultima corsia usata

   while(true) {
      // Genera un seme casuale diverso ogni volta
      srand(time(NULL));

      // Scegli una corsia casuale (1-8)
      rand_flux = rand() % 8 + 1;

      // Evita di creare due coccodrilli nella stessa corsia di fila
      if (rand_flux != prev_rand_flux) {
         // Crea un nuovo processo coccodrillo
         generate_process(CROC_ID, rand_flux, -1, -1, 0);
      }

      // Ricorda l'ultima corsia usata
      prev_rand_flux = rand_flux;

      // Aspetta prima di creare il prossimo
      usleep(CREATOR_SLEEP);
   }
}

/*
 * PROCESSO RANA - IL GIOCATORE
 * ===========================
 *
 * Questo è il processo più importante: gestisce l'input del giocatore
 * e muove la rana in base ai tasti premuti.
 */

void frog() {
   // Variabili per gestire l'input
   struct msg m;          // Messaggio da inviare al padre
   pid_t pid;             // PID di questo processo
   int input;             // Tasto premuto
   bool new_input = false; // Flag per nuovo input

   // Inizializza il messaggio
   m.id = FROG_ID;        // Tipo: rana
   m.pid = getpid();      // PID del processo rana
   frogxy.y = FROG_START_Y; // Posizione iniziale
   m.x = 0; m.y = 0;      // Nessun movimento iniziale

   while(true) {
      // Leggi un carattere dalla tastiera
      input = (int)getch();

      // Controlla se c'è dell'input
      if (input != ERR) {
         new_input = true;
      } else {
         input = false;
         new_input = false;
      }

      // Gestisci l'input
      switch (input) {
         case KEY_UP:
            m.x = 0;           // Nessun movimento orizzontale
            m.y = -FROG_Y;     // Movimento verso l'alto
            frogxy.y += m.y;   // Aggiorna posizione locale
            break;

         case KEY_DOWN:
            m.x = 0;
            m.y = FROG_Y;      // Movimento verso il basso
            frogxy.y += m.y;
            break;

         case KEY_LEFT:
            m.x = -FROG_X;     // Movimento verso sinistra
            m.y = 0;
            break;

         case KEY_RIGHT:
            m.x = FROG_X;      // Movimento verso destra
            m.y = 0;
            break;

         case TAB:  // Tasto TAB per sparare proiettili
            // Crea due proiettili: uno verso sinistra, uno verso destra
            generate_process(BULL_ID, -1, frogxy.y, -1, 0);  // Sinistra
            generate_process(BULL_ID, -1, frogxy.y, 1, 0);   // Destra
            usleep(FROG_BULL_SLEEP);  // Pausa dopo aver sparato
            break;

         default:
            break;
      }

      // Invia il messaggio al processo padre solo se c'è movimento
      if ((new_input) && (m.x != 0 || m.y != 0) && (input != TAB)) {
         write(pipe_fds[1], &m, sizeof(m));
      } else {
         // Invia comunque un messaggio (anche se senza movimento)
         m.x = 0; m.y = 0;
         write(pipe_fds[1], &m, sizeof(m));
      }

      // Pausa prima del prossimo controllo input
      usleep(FROG_SLEEPS_TONIGHT);
   }
}

/*
 * PROCESSO PROIETTILE RANA
 * =======================
 *
 * Gestisce i proiettili sparati dalla rana quando premi TAB.
 */

void bullet(int y, int speed) {
   // Prepara il messaggio del proiettile
   struct msg m;
   m.y = y + 1;           // Posizione Y (un po' sotto la rana)
   m.id = BULL_ID;        // Tipo: proiettile rana
   m.pid = getpid();      // PID del processo
   m.x_speed = (speed >= 0) ? (BULL_SPEED) : (-BULL_SPEED); // Direzione
   m.x = m.x_speed;       // Posizione X iniziale
   m.first = true;        // Flag per il primo messaggio

   while(true) {
      // Invia posizione al processo padre
      write(pipe_fds[1], &m, sizeof(m));

      // Dopo il primo messaggio, disabilita il flag
      m.first = false;

      // Muovi il proiettile
      usleep(BULL_SLEEP);
   }
}

/*
 * TERMINAZIONE PROCESSI - VERSIONE COMPLETA
 * ========================================
 *
 * Funzioni per terminare i processi quando necessario.
 */

void kill_all() {
   // Scandisci tutta la matrice e uccidi tutti i processi attivi
   for (size_t i = 0; i < CROC_X*2 + DIM_X; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
         kill_process(game_matrix[i][j].pid);
      }
   }
}

void kill_process(pid_t pid) {
   if (pid > 0) {
      // Invia segnale di terminazione
      kill(pid, SIGKILL);
      // Aspetta che il processo termini (senza bloccare)
      waitpid(pid, NULL, WNOHANG);
   }
}

/*
 * INIZIALIZZAZIONE PIPE
 * ====================
 *
 * Crea la "tubatura" per la comunicazione tra processi.
 */

void init_pipe() {
   if (pipe(pipe_fds) == -1) {
      perror("Pipe call");
      exit(1);
   }
}

/*
 * INTERFACCIA GIOCO - VITE E TEMPO
 * ===============================
 *
 * Funzioni per disegnare l'interfaccia utente.
 */

void ready_field() {
   lifes();       // Disegna le vite
   time_bar();    // Disegna la barra del tempo
}

void lifes() {
   // Attiva il colore per le vite
   wattron(gamewin, COLOR_PAIR(COL_VITE));

   // Disegna un cuore per ogni vita
   for (int i = 0; i < vite; i++) {
      mvwprintw(gamewin, VITE_Y, VITE_X + i*2, SYMB_VITE);
   }

   wattroff(gamewin, COLOR_PAIR(COL_VITE));
}

void time_bar() {
   // Attiva il colore per la barra del tempo
   wattron(gamewin, COLOR_PAIR(COL_TIME));

   // Disegna la barra piena
   for (int i = 0; i < END_TIME_X-2; i++) {
      mvwprintw(gamewin, TIME_Y, START_TIME_X + i, " ");
   }

   wattroff(gamewin, COLOR_PAIR(COL_TIME));
}

void time_bar_progress(long int start) {
   // Calcola il tempo passato
   long int time_passed = gettime(start);

   // Disegna la barra che si svuota (da destra a sinistra)
   for (int i = 0; i < round(time_passed * 2.5); i++) {
      mvwprintw(gamewin, TIME_Y, END_TIME_X - i, " ");
   }
}

long int gettime(long int start) {
   struct timespec end;
   long int time_passed;

   // Ottieni il tempo attuale
   clock_gettime(CLOCK_REALTIME, &end);

   // Calcola differenza
   time_passed = end.tv_sec - start;

   return time_passed;
}

/*
 * SISTEMA DI DISEGNO - GRAFICA
 * ===========================
 *
 * Funzioni che gestiscono tutto il disegno del gioco.
 */

void graphics() {
   background();    // Disegna lo sfondo
   draw_matrix();   // Disegna tutti gli oggetti
   box(gamewin, 0, 0); // Disegna il bordo della finestra
}

void draw_matrix() {
   // Scandisci la matrice di gioco e disegna ogni oggetto
   for (size_t i = CROC_X; i < DIM_X + CROC_X; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
         switch(game_matrix[i][j].id) {
            case FROG_ID:           // Disegna la rana
               draw_frog(i, j);
               break;

            case CROC_ID:           // Disegna un coccodrillo
               if (game_matrix[i][j].pid > -1) {
                  wattron(gamewin, COLOR_PAIR(COL_COCCODRILLI));
                  mvwprintw(gamewin, j, i - CROC_X - 1, "%s", game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_COCCODRILLI));
               }
               break;

            case BULL_ID:           // Disegna proiettile rana
               wattron(gamewin, COLOR_PAIR(COL_BULL_RANA));
               mvwprintw(gamewin, j, i - CROC_X - 1, "%s", game_matrix[i][j].ch);
               wattroff(gamewin, COLOR_PAIR(COL_BULL_RANA));
               break;

            case BULL_CROC_ID:      // Disegna proiettile coccodrillo
               wattron(gamewin, COLOR_PAIR(COL_BULL_CROC));
               mvwprintw(gamewin, j, i - CROC_X - 1, "%s", game_matrix[i][j].ch);
               wattroff(gamewin, COLOR_PAIR(COL_BULL_CROC));
               break;

            default:
               break;
         }
      }
   }

   // Controlla sempre lo stato della rana
   if ((frog_on_water()) || frog_is_out_of_bounds()) {
      kill_process(frogxy.pid);
      partita.manche_on = false;
      partita.loss = true;
   }
}

void draw_frog(int x, int y) {
   int color;

   // Scegli il colore in base alla zona
   if (y >= MARCIAPIEDESY && y < MARCIAPIEDESY + MARCIAPIEDE_Y) {
      color = COL_RANA_MARC;     // Marciapiede
   } else if (y > FIUMESY && y < FIUMESY + FIUME_Y) {
      color = COL_RANA_FIUME;    // Fiume
   } else if (y >= RIVASY && y < RIVASY + RIVA_Y) {
      color = COL_RANA_RIVA;     // Riva
   }

   // Se la rana è su un coccodrillo, usa il colore speciale
   if (frogxy.on_croc) {
      color = COL_RANA_CROC;
   }

   // Disegna la rana con il colore scelto
   wattron(gamewin, COLOR_PAIR(color));
   mvwprintw(gamewin, y, x - CROC_X - 1, "%s", game_matrix[x][y].ch);
   wattroff(gamewin, COLOR_PAIR(color));
}

/*
 * DISEGNO SFONDO - ELEMENTI STATICI
 * ================================
 *
 * Funzioni che disegnano le parti fisse del gioco.
 */

void background() {
   tane();        // Disegna le tane
   riva();        // Disegna la riva
   fiume();       // Disegna il fiume
   marciapiede(); // Disegna il marciapiede
}

void draw_tana(int x) {
   // Disegna una singola tana
   wattron(gamewin, COLOR_PAIR(COL_TANE));
   for(int i = 0; i < TANEY; i++) {
      for(int j = 0; j < TANE_LARGE; j++) {
         mvwprintw(gamewin, TANESY + i, x + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_TANE));
}

void tane() {
   // Disegna lo sfondo delle tane
   wattron(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
   for(int i = 0; i < TANEY; i++) {
      for(int j = 0; j < TANEX; j++) {
         mvwprintw(gamewin, TANESY + i, TANESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));

   // Disegna le tane vere e proprie (solo se non conquistate)
   for (size_t i = 0; i < 5; i++) {
      if (tanen[i] == 0) {  // Tana non conquistata
         draw_tana(TANESX + TANE_LARGE * i + 12 * (i + 1));
      }
   }
}

void riva() {
   // Disegna la zona della riva
   wattron(gamewin, COLOR_PAIR(COL_RIVA));
   for(int i = 0; i < RIVA_Y; i++) {
      for(int j = 0; j < RIVA_X; j++) {
         mvwprintw(gamewin, RIVASY + i, RIVASX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_RIVA));
}

void fiume() {
   // Disegna la zona del fiume
   wattron(gamewin, COLOR_PAIR(COL_FIUME));
   for(int i = 0; i < FIUME_Y; i++) {
      for(int j = 0; j < FIUME_X; j++) {
         mvwprintw(gamewin, FIUMESY + i, FIUMESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_FIUME));
}

void marciapiede() {
   // Disegna la zona del marciapiede
   wattron(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
   for(int i = 0; i < MARCIAPIEDE_Y; i++) {
      for(int j = 0; j < MARCIAPIEDE_X; j++) {
         mvwprintw(gamewin, MARCIAPIEDESY + i, MARCIAPIEDESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
}
