
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h> 
#include <sys/fcntl.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

//Da fare:
// sistemare lo sfondo del proiettile RANA del marciapiede
// rendere distinguibili i proiettili dells rana e dei coccodrilli (cambiare i colori)

//MACRO DI INIZIALIZZAZIONE
#define CURS 0 //visibilità cursore
#define ND 0 //valore di nodelay
#define KP 1 //keypad

//DIMENSIONI SCHERMO & FRIENDS
#define DIM_X 155 //larghezza campo di gioco
#define DIM_Y 42 //altezza campo di gioco
#define VITE_X 1 //coord x delle vite
#define VITE_Y DIM_Y-2 //coord y delle vite
#define START_TIME_X 2
#define END_TIME_X DIM_X-3
#define TIME_Y 1
#define TANEX DIM_X-2
#define TANEY 3
#define RIVA_X DIM_X-2
#define RIVA_Y 4
#define FIUME_X DIM_X-2
#define FIUME_Y 24
#define MARCIAPIEDE_X DIM_X-2
#define MARCIAPIEDE_Y 4
#define TANESX 1
#define TANESY 3
#define RIVASX 1
#define RIVASY TANESY+TANEY
#define FIUMESX 1
#define FIUMESY RIVASY+RIVA_Y
#define MARCIAPIEDESX 1
#define MARCIAPIEDESY FIUMESY+FIUME_Y
#define FLUX_Y FIUME_Y/8
#define TANE_LARGE 16

//COLORI
#define COL_VITE 1
#define COL_TIME 2
#define COL_RIVA 3
#define COL_MARCIAPIEDE 4
#define COL_FIUME 5
#define COL_RANA 6
#define COL_RANA_RIVA 7
#define COL_RANA_FIUME 8
#define COL_RANA_MARC 9
#define COL_COCCODRILLI 10
#define COL_TANE 11
#define COL_RANA_CROC 12
#define COL_BULL_CROC 13
#define COL_BULL_RANA 14

//COLORI PERSONALIZZATI
#define VERDE_RANA 100
#define ROSSO_ARGINE 101

//ID
#define FROG_ID 0
#define FCROC_ID 1
#define CROC_ID 2
#define BULL_ID 3
#define BULL_CROC_ID 4

//macro testo
#define AVVISO_DIM_SCHERMO "INGRANDIRE LO SCHERMO" //stampato quando la dimensione dello schermo è inferiore al minimo 

//dimensioni entità
#define FROG_X 4
#define FROG_Y 3
#define CROC_X 16
#define CROC_Y FROG_Y
#define BULL_X 3 
#define BULL_Y 2
#define TANA_X 6
#define TANA_Y 4

//MISCELLANOUS
#define VITE 5
#define SYMB_VITE "♥ "
#define SYMB_BULL "☣"
#define RIGHT_BULL "⁍"
#define LEFT_BULL "⁌"
#define TIMEL 60
#define TAB 32
#define FROG_START_X DIM_X/2
#define FROG_START_Y MARCIAPIEDESY
#define N_FLUSSI 8
#define CROC_SPEED 1
#define BULL_SPEED 1
#define BULL_CHANCE 100

//sleep vari
#define FROG_SLEEPS_TONIGHT 30000
#define MAIN_SLEEP 1
#define CREATOR_SLEEP 100000
#define CROC_SLEEP 90000
#define BULL_SLEEP 60000
#define FROG_BULL_SLEEP 500000


//finestra di gioco
WINDOW *gamewin;
struct point {
   int x;
   int y;
   pid_t pid;
   bool change;
   bool on_croc;
   pid_t croc_pid; 
};

//struttura dei messaggi che passo alla pipe
struct msg {
    int id;
    int x;    
    int y;
    char ch[4];
    pid_t pid; 
    int x_speed;
    int y_speed; 
    bool first; 
};

//struttura della partita (cioè ha i due campi che servono per valutare una manche)
struct game {
   bool manche_on;
   bool loss;
};


//firme delle funzioni
struct point get_screen_size();
struct point generate_win_start();
bool is_screen_size_ok();
void screen_size_loop();
void create_game_win();
void game_init();
void lifes();
void time_bar();
void tane();
void riva();
void fiume();
void marciapiede();
void draw_frog(int, int);
void background();
void draw_matrix();
void graphics();
long int gettime(long int);
void time_bar_progress(long int);
void ready_field();
void ready_matrix();
void ready_frog();
void bullet(int, int);
void frog();
void kill_process(pid_t);
void kill_all();
void bullet_croc(int, int, int);
void croc(int);
bool frog_is_drowning();
bool croc_on_both();
bool croc_on_right();
bool croc_on_left();
bool check_croc_for_frog(int, int);
bool frog_on_water();
bool frog_is_out_of_bounds();
bool croc_is_out_of_bounds(struct msg);
bool check_frog_win();
bool is_frog_on_croc();
bool frog_collision(int, int);
bool bullet_is_out_of_bounds(int);
void update_frog(int, int, pid_t);
void delete_old(pid_t, bool);
void update_matrix_croc_bull(struct msg);
void update_matrix_bull(struct msg);
void update_matrix_croc(struct msg);
void update_matrix_frog(bool);
void update(struct msg);
void init_flussi();
void get_data();
void croc_creator();
void generate_process(int, int, int, int, int);
void init_processes();
void generate_frog();
void generate_croc_father();
void init_pipe();
void manche();
void run();
void game_polling();

//variabili globali
int vite;
int pipe_fds[2];
int flussi[N_FLUSSI];
int tanen[5] = {0};
int score;
//struttura rana (il processo manda solo posizioni relative)
struct point frogxy;
//matrice di gioco
struct msg game_matrix[DIM_X+2*CROC_X][DIM_Y];
struct game partita;

//sprite

char frog_sprite[FROG_Y*FROG_X][FROG_X] = {"▙", "█", "█" ,"▟", " ", "█", "█", " ", "█","▀", "▀", "█"};
char croc_sprite[CROC_Y*CROC_X][CROC_X] = {"▄", "▄", "▄", "▄", "▮", "▃", "▃", "▃", "▃", "▁", " "," "," "," "," "," ",
   "▚", "▚", "▚", "▚", "▚", "█", "█", "█", "█", "█", "█", "█", "▅", "▄", "▃", "▁",
   "▀", "▀", "▀", "▀", "▀", "▐", "▛", "▀", "▀", "▐", "▛", "▀", "▀", "▀", "█", "◤"};
char croc_sprite1[CROC_Y*CROC_X][CROC_X]={" "," "," "," "," "," ", "▁","▃","▃","▃","▃","▮","▄","▄","▄","▄",
   "▁","▃","▄","▅","█","█","█","█","█","█","█","▞","▞","▞","▞","▞",
   "◥","█","▀","▀","▀","▜","▋","▀","▀","▜","▋","▀","▀","▀","▀","▀"};
//◢
/*
char tana_aperta[TANA_Y*TANA_X][TANA_X]={"◢","█","█","█","█","◣",
    "█"," "," "," "," ","█",
    "█"," "," "," "," ","█",
    "█"," "," "," "," ","█",
    "█"," "," "," "," ","█"}

*/

//char tana_chiusa

int main() {
   //funzione che si occupa di inizializzare la schermata e i colori
   game_init();
   //funzione che contiene la logica di gioco
   game_polling();
}

//inizializzazione di schermo e colori
void game_init() {
   //inizializzo lo standard screen. Abbiamo scelto di settare nodelay a false ma non è necessario.
   setlocale(LC_ALL, ""); initscr(); noecho(); curs_set(CURS); nodelay(stdscr, ND); keypad(stdscr, KP); cbreak(); start_color();
   //inizializzo i colori
   init_color(VERDE_RANA, 100, 600,0); //tonalità di verde chiaro per la rana. Verde_rana
   init_color(ROSSO_ARGINE,320,67,141); //tonalità rosso scuro pe l'argine superiore e inferiore. Rosso_argine
   init_pair(COL_VITE, COLOR_RED, COLOR_BLACK);//colore delle vite
   init_pair(COL_TIME, COLOR_BLACK, COLOR_RED);//colore della barra del tempo
   init_pair(COL_RIVA, COLOR_BLUE, ROSSO_ARGINE);//colore dell'argine superiore
   init_pair(COL_MARCIAPIEDE, COLOR_BLACK, ROSSO_ARGINE);//colore del marciapiede
   init_pair(COL_FIUME, COLOR_YELLOW, COLOR_BLUE);//colore del fiume
   init_pair(COL_RANA_FIUME, VERDE_RANA, COLOR_BLUE);//colore della rana quando annega
   init_pair(COL_RANA_RIVA, VERDE_RANA, ROSSO_ARGINE);//colore della rana sull'argine superiore
   init_pair(COL_RANA_MARC, VERDE_RANA, ROSSO_ARGINE);//colore della rana sul marciapiede
   init_pair(COL_COCCODRILLI,COLOR_BLACK, COLOR_BLUE);//colore dei coccodrilli
   init_pair(COL_TANE, COLOR_BLACK, COLOR_YELLOW);//colore della zona delle tane
   init_pair(COL_RANA_CROC, VERDE_RANA, COLOR_BLACK);//colore della rana quando sta su un coccodrillo
   init_pair(COL_BULL_CROC, COLOR_YELLOW,COLOR_BLUE);//colori proiettili dei coccodrilli
   init_pair(COL_BULL_RANA, COLOR_RED,COLOR_BLUE);//colori proiettili dei coccodrilli
}

//funzione che contiene il loop principale del gioco
void game_polling() {
   bool wanna_play = true;
//il loop continua finché il giocatore non si stanca
   while (wanna_play) {
     wanna_play = false;
//controllo che le dimensioni schermo vadano bene
     screen_size_loop();
//IMPORTANTE!!! Il motivo per cui non inizializzo la finestra di gioco in game_init() è che
//adesso ho la sicurezza che le coordinate usate per generare la finestra con generate_win_start() sono adatte allo schermo
//grazie al while precedente
//creiamo la finestra di gioco
     create_game_win();
   //loop di gioco
     run();
     }
   }

//funzione che si occupa del loop secondario (manche) del gioco
void run() {
   vite = VITE;tanen[0]=0; tanen[1] = 0; tanen[2] = 0;tanen[3] = 0;tanen[4] = 0;tanen[5] = 0;score = 0;
   while(true) {
//disegno barra del tempo, vite ecc...
      ready_field();
      wrefresh(gamewin);
//inzia la manche
      manche();
//pulisco lo schermo
      wclear(gamewin);
//se la manche è stata persa decremento le vite
      if (partita.loss) {
         vite -= 1;
      }
//ridisegno il box
      box(gamewin, 0, 0);
   }
   
}  
 

//funzione che si occupa del ciclo di manche
void manche() {
   partita.manche_on = true; partita.loss = false; struct timespec start; 
//misuro il tempo di inizio
    clock_gettime(CLOCK_REALTIME, &start);
//disegno lo sfondo
    background();
//inizializziamo i flussi
    init_flussi();
//inizializzo la pipe
    init_pipe();
//inizializzo i processi
    init_processes();
//sono in lettura
    close(pipe_fds[1]);
//preparo la rana
    ready_frog();
//preapro la matrice settandola a -1
    ready_matrix();
    graphics();
    wrefresh(gamewin);
//disegnamo le vite adesso e non in graphics perché graphics si occupa di grafiche d
//continua finché non capita un evento di fine manche (es. la rana viene assassinata)
   while(partita.manche_on) {
      time_bar_progress(start.tv_sec);
      //graphics();
      //process_data();      
      wrefresh(gamewin);
//ricevo i dati dalle pipes
      get_data();
//processo i dati
      graphics();
      fflush(stdout);;
      //controllo se il tempo è finito
      
      if (gettime(start.tv_sec) >= TIMEL) {
         partita.manche_on = false;
         partita.loss = true;
         
      }
      
      usleep(MAIN_SLEEP);
      
   }
   
   kill_all();
}

   
//inizializza la schermata di gioco
void create_game_win() {
   gamewin = newwin(DIM_Y, DIM_X, generate_win_start().y, generate_win_start().x);
   box(gamewin, 0, 0);
}

//funzione che inizializza i processi
void init_processes() {
   generate_frog();
   generate_croc_father();
}

//funzione che inizializza i flussi
void init_flussi() {
   srand(time(NULL));
   int r = rand() % 2;
   for (size_t i = 0; i < N_FLUSSI; i++) {
     if (r == 0) {
     flussi[i] = (i % 2 == 0) ? (0) : (1);
     }
     else {flussi[i] = (i % 2 == 0) ? (1) : (0);}
   }
}

//funzione che riceve e processa i dati
void get_data() {
   struct msg temp; 
   read(pipe_fds[0], &temp, sizeof(temp));
   update(temp);
}

//funzione che aggiorna i dati
void update(struct msg temp) {
   switch(temp.id) {

//aggiorno i dati relativi all'oggetto rana
      case FROG_ID:
        
        update_frog(temp.x, temp.y, temp.pid);
        update_matrix_frog(true);
        break;
        
      case CROC_ID:
         update_matrix_croc(temp);
         break;
         
      case BULL_ID:
         update_matrix_bull(temp);
         break;
         
      case BULL_CROC_ID:
         update_matrix_croc_bull(temp);
         break;
      
      default:
        break;
   }

}

//funzione che aggiorna la matrice relativamente ai coccodrilli
void update_matrix_croc(struct msg temp) {
   delete_old(temp.pid, false);
//controllo se il coccodrillo è uscito dal campo di gioco
   if (croc_is_out_of_bounds(temp)) {kill_process(temp.pid);}
   //if ((frog_on_water()) || frog_is_out_of_bounds()) {kill_process(frogxy.pid); partita.manche_on = false; partita.loss = true;}
   else {
      for(size_t i = 0; i < CROC_X; i++) {
         for(size_t j = 0; j < CROC_Y; j++) {
           if (/*game_matrix[temp.x+i][temp.y+j].id != FROG_ID &&*/ game_matrix[temp.x+i][temp.y+j].id != BULL_ID) {
               //if (game_matrix[i][j].id == BULL_ID) {beep();}
               game_matrix[temp.x+i][temp.y+j].id = CROC_ID;
               game_matrix[temp.x+i][temp.y+j].pid = temp.pid;
               if (temp.x_speed > 0) {
                  strcpy(game_matrix[temp.x+i][temp.y+j].ch, croc_sprite1[i+CROC_X*j]);
               }
               else {strcpy(game_matrix[temp.x+i][temp.y+j].ch, croc_sprite[i+CROC_X*j]);}
               }
         }
      } 
      
   } 
   if ((frog_on_water()) || frog_is_out_of_bounds()) {
   kill_process(frogxy.pid); partita.manche_on = false; partita.loss = true;}
   is_frog_on_croc();
   if (temp.pid == frogxy.croc_pid) {update_frog(temp.x_speed, 0, frogxy.pid); update_matrix_frog(true); }
   if (!is_frog_on_croc()) {frogxy.on_croc = false; frogxy.croc_pid = -1;}
   
}

//funzione che aggiorna la matrice relativamente ai proiettili
void update_matrix_bull(struct msg temp) {
   if (temp.first) {
      //for (int i = frogxy.x+abs(temp.x); i < frogxy.x + abs(temp.x) + BULL_X; i++) {
      /*
         for (int j = temp.y-1; j < temp.y + BULL_Y; j++) {
            game_matrix[i][j].id = BULL_ID;
            game_matrix[i][j].pid = temp.pid;
            strcpy(game_matrix[i][j].ch, SYMB_BULL);
             
         }
         */
         if (temp.x_speed > 0) {
            game_matrix[frogxy.x+FROG_X][temp.y].id = BULL_ID;
            game_matrix[frogxy.x+FROG_X][temp.y].pid = temp.pid;
            strcpy(game_matrix[frogxy.x+FROG_X][temp.y].ch, SYMB_BULL);
            }
         else {
            game_matrix[frogxy.x-1][temp.y].id = BULL_ID;
            game_matrix[frogxy.x-1][temp.y].pid = temp.pid;
            strcpy(game_matrix[frogxy.x-1][temp.y].ch, SYMB_BULL);
         
         }
      //}
   }
   
   else {
   
      for (int i = 0; i < CROC_X*2+DIM_X; i++) {
          if (game_matrix[i][temp.y].pid == temp.pid) {
             //beep();
             delete_old(temp.pid, false);
             if (bullet_is_out_of_bounds(i+temp.x_speed)) {kill_process(temp.pid);}
             else {
             /*
             for (int j = i+abs(temp.x)+1; j <  i+abs(temp.x)+1+ BULL_X; j++) {
               for (int h = temp.y; h < temp.y + BULL_Y; h++) {
                  game_matrix[j][h].pid = temp.pid;
                  game_matrix[j][h].id = BULL_ID;
                  strcpy(game_matrix[j][h].ch, SYMB_BULL);
                }
       }*/   
            game_matrix[i+temp.x_speed][temp.y].id = BULL_ID;
            game_matrix[i+temp.x_speed][temp.y].pid = temp.pid;
            strcpy(game_matrix[i+temp.x_speed][temp.y].ch, SYMB_BULL);
            i = CROC_X*2+DIM_X;
            }
     }
   }
   
   
   
 }
 
}

//funzione che aggiorna la matrice relativamente ai proiettili coccodrillo
void update_matrix_croc_bull(struct msg temp) {
   delete_old(temp.pid, false);
   if (bullet_is_out_of_bounds(temp.x)) {kill_process(temp.pid);}
   else {
   if (game_matrix[temp.x][temp.y].id == BULL_ID && game_matrix[temp.x][temp.y].pid > 0) {
      kill_process(temp.pid);
      kill_process(game_matrix[temp.x][temp.y].pid);
      delete_old(game_matrix[temp.x][temp.y].pid, false);
   }
   else if (frog_collision(temp.x, temp.y)) {
      kill_process(frogxy.pid);
      partita.manche_on = false;
      partita.loss = true;
      
   }
   else{
   game_matrix[temp.x][temp.y].id = BULL_CROC_ID;
   game_matrix[temp.x][temp.y].pid = temp.pid;

   wattron(gamewin, COLOR_PAIR(COL_TANE));
   if (temp.x_speed > 0) {strcpy(game_matrix[temp.x][temp.y].ch, RIGHT_BULL);}
   else {strcpy(game_matrix[temp.x][temp.y].ch, LEFT_BULL);}
   wattroff(gamewin, COLOR_PAIR(COL_TANE));
   }
   }
}

//funzione che controlla se il proiettile ha colpito la rana
bool frog_collision(int x, int y) {
   if (game_matrix[x][y].id == FROG_ID) {
      return true;
   }
   return false;
}


//funzione che si occupa di aggiornare i dati relativi alla rana
void update_frog(int x, int y, pid_t pid) {
   frogxy.x += x;
   frogxy.y += y;
   frogxy.pid = pid;
   
   //controlliamo se la rana ha cambiato posizione
   if (x != 0 || y != 0) {frogxy.change = true;}
   else {frogxy.change = false;}
}

//funzione che si occupa di aggiornare la matrice delle posizioni
//se need è false allora l'update sta arrivando da update_matrix_croc e non è necessario controllare se la nuova posizione della rana è lecita (DEPRECATED)
void update_matrix_frog(bool need) {
   delete_old(frogxy.pid, true);
   //check_frog_win();
   for(size_t i = 0; i < FROG_X; i++) {
      for(size_t j = 0; j < FROG_Y; j++) {
         if (game_matrix[frogxy.x+i][frogxy.y+j].id != BULL_ID) {
            game_matrix[frogxy.x+i][frogxy.y+j].id = FROG_ID;
            game_matrix[frogxy.x+i][frogxy.y+j].pid = frogxy.pid;
            strcpy(game_matrix[frogxy.x+i][frogxy.y+j].ch, frog_sprite[i+FROG_X*j]);
         }
      }
   }
   if ((frog_on_water() && need) || frog_is_out_of_bounds()) {
   kill_process(frogxy.pid); partita.manche_on = false; partita.loss = true;}
   
}

//funzione che cancella le vecchie entità
void delete_old(pid_t pid, bool frog) {
   for(size_t i = 0; i < DIM_X + CROC_X*2; i++) {
      for(size_t j = 0; j < DIM_Y; j++) {
         if (pid == game_matrix[i][j].pid) {
            if (frog && frogxy.on_croc /*&& game_matrix[i][j].id != BULL_ID*/) {
               game_matrix[i][j].id = CROC_ID; 
               game_matrix[i][j].pid = -1; 
            }
            //else if (game_matrix[i][j].id == BULL_ID) {
            //}
            else if (game_matrix[i][j].id == BULL_ID) {game_matrix[i][j].id = CROC_ID; 
               game_matrix[i][j].pid = -1; }
            else{
               game_matrix[i][j].pid = -1;
               game_matrix[i][j].id = -1; 
               
            }
            
         }
      }
   }  

}

//funzione che controlla se la rana ha chiuso una tana
bool check_frog_win() {
   for (size_t i = 0; i < 5; i++) {
      if ((frogxy.x >= TANESX+12*(i+1)+TANE_LARGE*i && frogxy.x < TANESX+12*(i+1)+TANE_LARGE*(i+1)) && frogxy.y < RIVASY)
      {if (tanen[i] == 0) {partita.manche_on = false; partita.loss = false; tanen[i] = 1;}
      else {partita.manche_on = false; partita.loss = false;}


      }
   }
}

//funzione che controlla che il priettile non sia uscito dallo schermo
bool bullet_is_out_of_bounds(int x) {
   if ((x > DIM_X+CROC_X) || (x < CROC_X)) {return true;}
   return false;
}

//funzione che ritorna true se la rana è sul fiume
bool frog_on_water() {
//IMPORTANTE! è necessaria questa inzializzazione (o metterla come prima condizione nell'if). Se non attuata il compilatore svolge solo una condizione, vede che è falsa e ignora le altre. E noi vogliamo che la funziona venga eseguita.
   bool is_on = is_frog_on_croc();
   if (frogxy.y < MARCIAPIEDESY && frogxy.y > FIUMESY && !is_on) {
      return true;
    }
  
   return false;
}

//funzione che controlla se la rana è sul coccodrillo
bool is_frog_on_croc() {
   if ((croc_on_both() || croc_on_right() || croc_on_left()) && (!frog_is_drowning())) {frogxy.on_croc = true; return true;}
   else {return false;} 
   }

bool frog_is_drowning() {
   if (game_matrix[frogxy.x-1][frogxy.y].id != CROC_ID && game_matrix[frogxy.x+FROG_X][frogxy.y].id != CROC_ID) {
      return true;
   }
   return false;
}

bool croc_on_both() {
   frogxy.croc_pid = game_matrix[frogxy.x-1][frogxy.y].pid;
   for (size_t i = frogxy.y; i < frogxy.y+FROG_Y; i++) {
      if ((game_matrix[frogxy.x-1][i].id != CROC_ID && game_matrix[frogxy.x-1][i].id != BULL_ID) || (game_matrix[frogxy.x+FROG_X][i].id != CROC_ID && game_matrix[frogxy.x+FROG_X][i].id != BULL_ID)) {
         return false;
        }
      }
   frogxy.croc_pid = game_matrix[frogxy.x-1][frogxy.y].pid;
   return true;
}

bool croc_on_right() {
   for (size_t i = frogxy.y; i < frogxy.y+FROG_Y; i++) {
      if (game_matrix[frogxy.x-1][i].id != CROC_ID && game_matrix[frogxy.x-1][i].id != BULL_ID) {
         return false;
        }
    }
   if ((game_matrix[frogxy.x+FROG_X-CROC_X][frogxy.y].id == CROC_ID || game_matrix[frogxy.x+FROG_X-CROC_X][frogxy.y].id == BULL_ID) && (game_matrix[frogxy.x-CROC_X+1][frogxy.y].id != CROC_ID && game_matrix[frogxy.x-CROC_X+1][frogxy.y].id != BULL_ID)) {frogxy.croc_pid = game_matrix[frogxy.x-1][frogxy.y].pid; return true;}
   
   return false;
}

bool croc_on_left() {
   for (size_t i = frogxy.y; i < frogxy.y+FROG_Y; i++) {
         if (game_matrix[frogxy.x+FROG_X][i].id != CROC_ID && game_matrix[frogxy.x+FROG_X][i].id != BULL_ID) {
           // beep();
            return false;
           }
       }
      if ((game_matrix[frogxy.x+CROC_X-1][frogxy.y].id == CROC_ID || game_matrix[frogxy.x+CROC_X-1][frogxy.y].id == BULL_ID ) && (game_matrix[frogxy.x+CROC_X+FROG_X-1][frogxy.y].id != CROC_ID && game_matrix[frogxy.x+CROC_X+FROG_X-1][frogxy.y].id != BULL_ID)) {frogxy.croc_pid = game_matrix[frogxy.x+FROG_X][frogxy.y].pid; return true;}
   //beep();
   return false;
}

//funzione che ritorna true se un coccodrillo è out of bounds
bool croc_is_out_of_bounds(struct msg temp) {
   if ((temp.x > DIM_X-1+CROC_X && temp.x_speed > 0) || (temp.x <= 0 && temp.x_speed < 0)) {
      return true;   
   }
   return false;
}

//funzione che controlla se la rana è out of bounds
bool frog_is_out_of_bounds() {
   if (frogxy.x < CROC_X+1 || frogxy.x > CROC_X+DIM_X-FROG_X-1) {
      return true;
   }
   return false;
}

//funzione che dichiara la posizione di partenza della rana
void ready_frog() {
   frogxy.x = FROG_START_X + CROC_X;
   frogxy.y = FROG_START_Y;
   frogxy.on_croc = false;
   frogxy.croc_pid = -1;
}

//fuznione che setta tutte le celle della matrice a -1
void ready_matrix() {
   //preparo la matrice con i valori iniziali corretti
   for (size_t i = 0; i < DIM_X+CROC_X*2; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
         game_matrix[i][j].id = -1;
         game_matrix[i][j].pid = -1; 
      }
   }

   //setto la posizione iniziale della rana nella matrice
   for (size_t i = frogxy.y; i < frogxy.y+FROG_Y; i++) {
      for(size_t j = frogxy.x; j < frogxy.x+FROG_X; j++) {
         game_matrix[j][i].pid = -1;
         game_matrix[j][i].id = FROG_ID;
         strcpy(game_matrix[j][i].ch, frog_sprite[j+FROG_X*i]);
      }
   }
}

//funzione che genera il processo della rana
void generate_frog() {
   generate_process(FROG_ID, 1, -1, -1, 0);
}

//funzione che genera il processo padre dei coccodrilli
void generate_croc_father() {
   generate_process(FCROC_ID, 1, -1, -1, 0);
}

//funzione che genera i processi
void generate_process(int id, int flusso, int x, int y, int speed) {
   pid_t pid;
   
   pid = fork();
   if (pid < 0) {perror("fork call"); _exit(2);}
   if (pid == 0) {
//sono in scrittura
      int fd = open("/dev/null", O_RDONLY);
      dup2(fd, STDOUT_FILENO);
      close(pipe_fds[0]);
      switch(id) {
      
         case FROG_ID:
           //sleep(5);
            frog();
            break;
            
         case FCROC_ID:
            //beep();
            //sleep(7);
            croc_creator();
            break;
            
         case CROC_ID:
            croc(flusso);
            break;
            
         case BULL_ID:
            bullet(x, y);
            break;
            
         case BULL_CROC_ID:
            bullet_croc(x, y, speed);
            break;
            
         default:
            break;
      }
      _exit(2);
   }
   else {} // non che serva, era solo comodo "concettualmente"

}

//funzione che crea il coccodrillo
void croc(int flusso) {
   struct msg m;
   m.y = MARCIAPIEDESY-flusso*FLUX_Y;
   m.x = flussi[flusso-1]*(DIM_X+CROC_X);
   m.id = CROC_ID;
   m.pid = getpid();
   m.x_speed = (flussi[flusso-1] == 0) ? (CROC_SPEED) : (-CROC_SPEED);
   srand(m.pid);
   while(true) {
      write(pipe_fds[1], &m, sizeof(m));
      if (rand() % BULL_CHANCE == 0) {
         generate_process(BULL_CROC_ID, -1, m.x, m.y, m.x_speed);
      }
      m.x += m.x_speed;
      usleep(CROC_SLEEP);
   }
   
}

//funzione che genera i proiettili coccodrillo
void bullet_croc(int x, int y, int speed) {
   struct msg m;
   if (speed > 0) {m.x_speed = 1; m.x = x+CROC_X;}
   else {m.x_speed = -1; m.x = x;}
   m.pid = getpid();
   m.y = y+1;
   m.id = BULL_CROC_ID;
   while (true) {
     write(pipe_fds[1], &m, sizeof(m));
     m.x += m.x_speed;
     usleep(BULL_SLEEP/2);
   }
}

//funzione che genera i coccodrilli
/*
void croc_creator() {
   int rand_flux; int prev_rand_flux = -1; int lasts[4] = {0};
   bool new = true;
   while(true) {*/
   /*
     srand(time(NULL));
   
      rand_flux = rand() % 8 + 1;
//abbiamo reputato non valesse la pena implementare una coda, o comunque un qualche tipo di pointer list, per un solo array.
      if (rand_flux != lasts[0] && rand_flux != lasts[1] && rand_flux != lasts[2] && rand_flux != lasts[3]) {
         generate_process(CROC_ID, rand_flux, -1, -1, 0);
         lasts[3] = lasts[2];
         lasts[2] = lasts[1];
         lasts[1] = lasts[0];
         lasts[0] = rand_flux;
         new = true;}
         
       else {
          new = false;
          while (new != true) {
             rand_flux = rand() % 8 + 1;
             if (rand_flux != lasts[0] && rand_flux != lasts[1] && rand_flux != lasts[2] && rand_flux != lasts[3]) {
                generate_process(CROC_ID, rand_flux, -1, -1, 0);
                lasts[3] = lasts[2];
                lasts[2] = lasts[1];
                lasts[1] = lasts[0];
                lasts[0] = rand_flux;
                new = true;
                }
             // beep();
          
           }
       }
      usleep(CREATOR_SLEEP);
      }
      */
      /*
      srand(time(NULL));
      rand_flux = rand() % 8 + 1;
      if (rand_flux != prev_rand_flux) {
         generate_process(CROC_ID, rand_flux, -1, -1, 0);
         }
       prev_rand_flux = rand_flux;
       usleep(CREATOR_SLEEP);
      }
      
}*/

//funzione che genera i coccodrilli
void croc_creator() {
   int rand_flux; int prev_rand_flux = -1;
   while(true) {
      srand(time(NULL));
      rand_flux = rand() % 8 + 1;
      if (rand_flux != prev_rand_flux) {
         generate_process(CROC_ID, rand_flux, -1, -1, 0);
         }
      prev_rand_flux = rand_flux;
      usleep(CREATOR_SLEEP);
      }
}

//funzione che controlla la rana
void frog() {
    
    struct msg m; pid_t pid; int input; bool new_input = false;
    
    m.id = FROG_ID;
    m.pid = getpid();
    //IMPORTANTE!!! Chiudo lo stream di output di questo processo; la ragione per cui è necessario è che chiamando getch() si chiama anche refresh()
    //questo da problemi di coerenza (o quantomeno non garantisce una esecuzione corretta) in alcuni casi.
    //int fd = open("/dev/null", O_RDONLY);
    //dup2(fd, STDOUT_FILENO);
    //sfrog.x = FROG_STARTX;
    frogxy.y = FROG_START_Y;
    m.x = 0;
    m.y = 0;
    while(true) {
       input = (int)getch();
       
       //Controllo se c'è dell'input nello stream
       if (input != ERR) {new_input = true; } 
       else {input = false; new_input = false;}
       
        switch (input) {
        
            case KEY_UP:
                m.x = 0;
                m.y = -FROG_Y; // Moving up
                //sfrog.x += m.x;
                frogxy.y += m.y;
                break;
                
            case KEY_DOWN:
                m.x = 0;
                m.y = FROG_Y; // Moving down
                //frogxy.x += m.x;
                frogxy.y += m.y;
                break;
                
            case KEY_LEFT:
                m.x = -FROG_X;
                m.y = 0; // Moving left
                //sfrog.x += m.x;
                //sfrog.y += m.y;
                break;
                
            case KEY_RIGHT:
                m.x = FROG_X;
                m.y = 0; // Moving right
                //sfrog.x += m.x;
                //sfrog.y += m.y;
                break;
                
            case TAB:
                generate_process(BULL_ID, -1, frogxy.y, -1, 0);
                generate_process(BULL_ID, -1, frogxy.y, 1, 0);
                usleep(FROG_BULL_SLEEP);
                break;
               
            default:
                break;
        }
        /*
        if ((new_input) && input != TAB && (m.x != 0 || m.y != 0)) {
           write(pipe_fds[1], &m, sizeof(m));
        }
        
        else {m.x = 0; m.y = 0; write(pipe_fds[1], &m, sizeof(m));}
      usleep(10000);*/
        if ((new_input) && (m.x != 0 || m.y != 0) && (input != TAB)) {
           write(pipe_fds[1], &m, sizeof(m));
        }
        
        else {m.x = 0; m.y = 0; write(pipe_fds[1], &m, sizeof(m));}
        
        usleep(FROG_SLEEPS_TONIGHT);
    }
}

//funzione che genera i proiettili
void bullet(int y, int speed) {
   struct msg m;
   m.y = y+1;
   m.id = BULL_ID;
   m.pid = getpid();
   m.x_speed = (speed >= 0) ? (BULL_SPEED) : (-BULL_SPEED);
   m.x = m.x_speed;
   m.first = true;
   while(true) {
     write(pipe_fds[1], &m, sizeof(m));
   /*
      if (m.first == true) {
         m.x = 0;
         write(pipe_fds[1], &m, sizeof(m));
         m.x = m.x_speed;
      }
      */
      m.first = false;
      usleep(BULL_SLEEP);
   }
}

//Uccide ogni processo presente in gioco (tranne se stesso)
void kill_all() {
   for (size_t i = 0; i < CROC_X*2 + DIM_X; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
         kill_process(game_matrix[i][j].pid);
      }
   }
   //kill_process(fcroc_pid);
}

//Uccide il processo
void kill_process(pid_t pid)  {
   if (pid > 0) {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, WNOHANG);
   }
}


//funzione che inizializza la pipe
void init_pipe() {
   if (pipe(pipe_fds) == -1) {
        perror("Pipe call");
        exit(1);
    }
}

//funzione che ritorna le coordinate da passare a newwin per inizializzare la finestra di gioco
struct point generate_win_start() {
   struct point upper_left_corner;
   
   upper_left_corner = get_screen_size();
   //Voglio che il campo di gioco sia centrato rispetto allo schermo
   upper_left_corner.y = (upper_left_corner.y - DIM_Y) / 2;
   upper_left_corner.x = (upper_left_corner.x - DIM_X) / 2;
   
   return upper_left_corner;
}

//controlla se le dimensioni schermo sono conformi
bool is_screen_size_ok() {
   bool flag = true;
   if (get_screen_size().x <= DIM_X || get_screen_size().y <= DIM_Y) {
       flag = false;
   }
   return flag;
 }
 
//funzione che si assicura la schermata abbia dimensione adatta
void screen_size_loop() {
   while (!is_screen_size_ok()) {
          mvprintw(get_screen_size().y/2, get_screen_size().x/2 - 10, AVVISO_DIM_SCHERMO);
          refresh();
       }
}

//ritorna le dimensioni attuali dello schermo   
struct point get_screen_size() {
   struct point maxxy;
   getmaxyx(stdscr, maxxy.y, maxxy.x);
   return maxxy;
   }

//funzione che disegna vite e barra del tempo   
void ready_field() {
   lifes();
   time_bar();
}

//funzione che disegna le vite
void lifes() {
   wattron(gamewin, COLOR_PAIR(COL_VITE));
   for (int i = 0; i < vite; i++) {
      mvwprintw(gamewin, VITE_Y, VITE_X + i*2, SYMB_VITE);
   }
   wattroff(gamewin, COLOR_PAIR(COL_VITE));
}

//funzione che disegna la barra del tempo piena
void time_bar() {
   wattron(gamewin, COLOR_PAIR(COL_TIME));
   for (int i = 0; i < END_TIME_X-2; i++) {
      mvwprintw(gamewin, TIME_Y, START_TIME_X+i, " ");
   }
   wattroff(gamewin, COLOR_PAIR(COL_TIME));
}

//funzione che disegna il "progresso" della barra del tempo
void time_bar_progress(long int start) {
//è uno spreco (andiamo a printare più volte negli stessi punti) ma il codice è più leggibile
   long int time_passed = gettime(start);
   for (int i = 0; i < round(time_passed*2.5) ; i++) {
      mvwprintw(gamewin, TIME_Y, END_TIME_X-i, " ");
   }
   
}


//funziona che ritorna il tempo passato da quando è stato misurato start
long int gettime(long int start){
   struct timespec end; long int time_passed;
   clock_gettime(CLOCK_REALTIME, &end);
   time_passed = end.tv_sec - start;
   return time_passed;
}

//funzione che disegna le grafiche
void graphics() {
   background();
   draw_matrix();
   box(gamewin, 0, 0);
}

//funzione che disegna ciò che è salvato nella matrice di gioco
void draw_matrix() {

   for (size_t i = CROC_X; i < DIM_X+CROC_X; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
            switch(game_matrix[i][j].id) {
               case FROG_ID:
                  draw_frog(i, j);
                  break;
               case CROC_ID:
                  if (game_matrix[i][j].pid > -1) {
                  wattron(gamewin, COLOR_PAIR(COL_COCCODRILLI));
                  mvwprintw(gamewin, j, i-CROC_X-1, "%s", game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_COCCODRILLI));
                  }
                  break;
               case BULL_ID:
                  wattron(gamewin, COLOR_PAIR(COL_BULL_RANA));
                  mvwprintw(gamewin, j, i-CROC_X-1, "%s", game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_BULL_RANA));
                  break;
               case BULL_CROC_ID:
                  wattron(gamewin, COLOR_PAIR(COL_BULL_CROC));
                  mvwprintw(gamewin, j, i-CROC_X-1, "%s", game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_BULL_CROC));
                  break;

               default:
                  break;
            }
      }
   }
   //is_frog_on_croc();
   if ((frog_on_water()) || frog_is_out_of_bounds()) {
   kill_process(frogxy.pid); partita.manche_on = false; partita.loss = true;}
}

//funzione che disegna la rana
void draw_frog(int x, int y) {
   int color;
   if (y >= MARCIAPIEDESY && y < MARCIAPIEDESY+MARCIAPIEDE_Y){color = COL_RANA_MARC;}
   else if (y > FIUMESY && y < FIUMESY + FIUME_Y){color = COL_RANA_FIUME;}
   else if (y >= RIVASY && y < RIVASY + RIVA_Y){color = COL_RANA_RIVA;}
   if (frogxy.on_croc) {color = COL_RANA_CROC;}
   wattron(gamewin, COLOR_PAIR(color));
   mvwprintw(gamewin, y, x-CROC_X-1, "%s", game_matrix[x][y].ch);
   wattroff(gamewin, COLOR_PAIR(color));

}

//funzione che disegna lo sfondo
void background() {
   tane();
   riva();
   fiume();
   marciapiede();
}

//funzione che disegna una tana
void draw_tana(int x) {
   wattron(gamewin, COLOR_PAIR(COL_TANE));
   for(int i = 0; i < TANEY; i++) {
      for(int j = 0; j < TANE_LARGE; j++) {
         mvwprintw(gamewin, TANESY + i, x + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_TANE));
}

//funzione stampa tane
void tane(){

   wattron(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
   for(int i = 0; i < TANEY; i++) {
      for(int j = 0; j < TANEX; j++) {
         mvwprintw(gamewin, TANESY + i, TANESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
   for (size_t i = 0; i < 5; i++) {
      if (tanen[i] == 0) {
         draw_tana(TANESX+TANE_LARGE*i+12*(i+1));
      }
   }

}

//funzione stampa riva
void riva() {
   wattron(gamewin, COLOR_PAIR(COL_RIVA));
   for(int i = 0; i < RIVA_Y; i++) {
      for(int j = 0; j < RIVA_X; j++) {
         mvwprintw(gamewin, RIVASY + i, RIVASX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_RIVA));
}
//funzione stampa fiume
void fiume() {
   wattron(gamewin, COLOR_PAIR(COL_FIUME));
   for(int i = 0; i < FIUME_Y; i++) {
      for(int j = 0; j < FIUME_X; j++) {
         mvwprintw(gamewin, FIUMESY + i, FIUMESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_FIUME));
}
//funzione stampa marciapiede
void marciapiede(){
   wattron(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
   for(int i = 0; i < MARCIAPIEDE_Y; i++) {
      for(int j = 0; j < MARCIAPIEDE_X; j++) {
         mvwprintw(gamewin, MARCIAPIEDESY + i, MARCIAPIEDESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
}

/*
void tana() {
  wattron(gamewin, COLOR_PAIR(COL_TANE));
   for(int i = 0; i < TANA_Y; i++) {
      for(int j = 0; j < MARCIAPIEDE_X; j++) {
         mvwprintw(gamewin, MARCIAPIEDESY + i, MARCIAPIEDESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_TANE));
}
*/



