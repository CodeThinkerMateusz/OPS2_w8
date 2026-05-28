#include "l8_common.h"

#define SPELL_TYPES 3
const char* spell_names[SPELL_TYPES] = {"Divination", "Summon Elemental", "Fireball"};
#define BOARD_SIZE 8
#define BACKLOG 16

#define MAX_QUEUE 10
#define THREAD_COUNT 3
#define FAMILIAR_DELAY 100

#define MAX_CLIENTS 2
#define MAX_NAME_LENGTH 14

#define MSG_SIZE 16
// MSG_SIZE = 16, bo zadanie mówi że KAŻDY datagram ma dokładnie 16 bajtów.
// Jeśli dostaniemy coś innego -> błąd.

typedef struct __attribute__((__packed__)) {
    char type;  // 1 bajt: typ wiadomości - 'l', 'c', lub 'q'
    char padding; // 1 bajt: padding - ignorujemy, ale musi być w strukturze żeby zachować właściwe offsety
    char body[14]; // 14 bajtów: ciało - różne znaczenie zależnie od typu
} message_t;
// __packed__ mówimy kompilatorowi żeby nie dodawał wyrównania między polami.
// Bez tego sizeof(message_t) mogłoby być np. 20 zamiast 16.


typedef struct __attribute__((__packed__)) {
    uint16_t spell;  // numer zaklęcia (0, 1, 2)
    uint16_t x;      // współrzędna X
    uint16_t y;      // współrzędna Y
} cast_body_t;

typedef struct {
    uint16_t spell;
    uint16_t x;
    uint16_t y;
    char name[MAX_NAME_LENGTH + 1];
    // Dodajemy imię - familiar musi wiedzieć kto rzuca
    // żeby wydrukować "[Cast] <name> casts ..."
} cast_cmd_t;
// To jest jeden element kolejki - komenda cast którą wątek familiar ma wykonać.
// Osobna struktura bo kolejka nie potrzebuje całego message_t, tylko te 3 wartości.

typedef struct {
    cast_cmd_t commands[MAX_QUEUE];
    // Tablica - tu przechowujemy komendy (max 10).
    
    int head;   // indeks gdzie czytamy (wątki familiar)
    int tail;   // indeks gdzie piszemy (wątek główny)
    int count;  // ile elementów jest teraz w kolejce
    
    pthread_mutex_t mutex;  // mutex - tylko jeden wątek na raz dotyka kolejki
    sem_t available;        // semafor - ile elementów jest do pobrania
    // Familiar czeka na semaforze jeśli kolejka pusta - bez busywaiting!
} fifo_queue_t;



void usage(char* name)
{
    printf("%s <in_port>\n", name);
    printf("  in_port - port that accepts messages\n");
    exit(EXIT_FAILURE);
}

#define STARTING_PEBBLES 10
// Każdy gracz zaczyna z 10 kamyczkami

const int spell_cost[SPELL_TYPES] = {1, 3, 4};
// Koszt zaklęć: Divination=1, Summon Elemental=3, Fireball=4
// Indeks odpowiada numerowi zaklęcia

typedef struct {
    char name[MAX_NAME_LENGTH + 1];
    // Imię gracza - +1 na null terminator
    
    struct sockaddr_in addr;
    // Adres IP i port gracza - używamy do identyfikacji
    // kto wysłał wiadomość
    
    int pebbles;
    // Ile kamyczków ma gracz
    
    bool logged_in;
    // Czy gracz jest już zalogowany
} player_t;


fifo_queue_t queue;
// Globalna kolejka - widoczna dla wszystkich wątków

void queue_init(fifo_queue_t* q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    // head i tail zaczynają od 0, kolejka pusta
    
    if (pthread_mutex_init(&q->mutex, NULL) != 0)
        ERR("pthread_mutex_init");
    // mutex chroni kolejkę przed jednoczesnym dostępem z wielu wątków
    
    if (sem_init(&q->available, 0, 0) != 0)
        ERR("sem_init");
    // semafor zaczyna od 0 - brak elementów
    // familiar będzie czekał na sem_wait() dopóki nie ma nic w kolejce
}

void queue_push(fifo_queue_t* q, cast_cmd_t cmd)
{
    pthread_mutex_lock(&q->mutex);
    // Blokujemy mutex - tylko my dotykamy kolejki
    
    if (q->count >= MAX_QUEUE)
    {
        // Kolejka pełna - wyrzucamy komendę
        fprintf(stderr, "ERROR: kolejka pełna, komenda odrzucona\n");
        pthread_mutex_unlock(&q->mutex);
        return;
    }
    
    q->commands[q->tail] = cmd;
    // Wpisujemy komendę na pozycję tail
    
    q->tail = (q->tail + 1) % MAX_QUEUE;
    // Przesuwamy tail do przodu (cyklicznie - po MAX_QUEUE wracamy do 0)
    
    q->count++;
    
    pthread_mutex_unlock(&q->mutex);
    // Zwalniamy mutex
    
    sem_post(&q->available);
    // Informujemy semafor że jest nowy element - familiar może się obudzić
}

cast_cmd_t queue_pop(fifo_queue_t* q)
{
    sem_wait(&q->available);
    // Czekamy aż będzie coś w kolejce - to jest blokowanie bez busywaiting!
    // Jeśli semafor == 0, wątek zasypia i czeka
    
    pthread_mutex_lock(&q->mutex);
    
    cast_cmd_t cmd = q->commands[q->head];
    // Pobieramy najstarszy element (head)
    
    q->head = (q->head + 1) % MAX_QUEUE;
    // Przesuwamy head do przodu (cyklicznie)
    
    q->count--;
    
    pthread_mutex_unlock(&q->mutex);
    
    return cmd;
}

void queue_destroy(fifo_queue_t* q)
{
    pthread_mutex_destroy(&q->mutex);
    sem_destroy(&q->available);
    // Sprzątamy zasoby na końcu programu
}


void* familiar_thread(void* arg)
{
    // arg to wskaźnik do naszej kolejki - rzutujemy z void*
    fifo_queue_t* q = (fifo_queue_t*)arg;
    
    while (1)
    {
        // Pobieramy komendę z kolejki - jeśli pusta, czekamy (sem_wait w środku)
        cast_cmd_t cmd = queue_pop(q);
        
        // Czekamy FAMILIAR_DELAY ms - symulujemy czas rzucania zaklęcia
        ms_sleep(FAMILIAR_DELAY);
        // ms_sleep jest już w l8_common.h - nie musisz jej pisać
        
        // Drukujemy wynik - tak samo jak w Stage 1
        printf("[Cast] %s casts %s onto %d,%d\n",cmd.name, spell_names[cmd.spell], cmd.x, cmd.y);
    }
    
    return NULL;
    // Wątek nigdy tu nie dotrze (while(1)), ale kompilator tego wymaga
}


int main(int argc, char** argv)
{
    if (argc != 2) usage(argv[0]);
    
    uint16_t port = atoi(argv[1]);
    
    // Trzeci argument (backlog) ignorowany dla UDP, dajemy 0.
    int sockfd = bind_inet_socket(port, SOCK_DGRAM, 0);
    
    printf("listen to port %d\n", port);
    
    message_t msg;
    // Tworzymy zmienną typu message_t - tu będą trafiać odebrane bajty.
    
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    // sender - adres nadawcy (skąd przyszedł datagram).
    // Potrzebujemy tego do recvfrom, nawet jeśli nie odpowiadamy.

    player_t players[MAX_CLIENTS];
    // Dwóch graczy - MAX_CLIENTS = 2
    
    int logged_count = 0;
    // Ile graczy już się zalogowało (czekamy na 2)
    
    // Inicjalizujemy graczy jako niezalogowanych
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        players[i].logged_in = false;
        players[i].pebbles = STARTING_PEBBLES;
    }
    
    // Faza logowania - akceptujemy TYLKO wiadomości typu 'l'
    // dopóki nie zalogują się 2 gracze
    while (logged_count < MAX_CLIENTS)
    {
        ssize_t received = TEMP_FAILURE_RETRY(recvfrom(
            sockfd,
            &msg,
            sizeof(msg),
            0,
            (struct sockaddr*)&sender,
            &sender_len
        ));
        
        if (received < 0) ERR("recvfrom");
        
        // Zły rozmiar - ignorujemy
        if (received != MSG_SIZE)
        {
            fprintf(stderr, "ERROR: nieprawidłowy rozmiar wiadomości\n");
            continue;
        }
        
        // Przed zalogowaniem akceptujemy TYLKO typ 'l'
        if (msg.type != 'l')
        {
            fprintf(stderr, "ERROR: gra jeszcze nie started, tylko login!\n");
            continue;
        }
        
        // Sprawdzamy czy ten gracz już się nie zalogował
        // (ten sam IP i port)
        bool already_logged = false;
        for (int i = 0; i < logged_count; i++)
        {
            if (players[i].addr.sin_addr.s_addr == sender.sin_addr.s_addr &&
                players[i].addr.sin_port == sender.sin_port)
            {
                already_logged = true;
                break;
            }
        }
        // Gracze muszą mieć różny IP i/lub port - bez tego odrzucamy
        if (already_logged)
        {
            fprintf(stderr, "ERROR: gracz już zalogowany\n");
            continue;
        }
        
        // Zapisujemy gracza
        msg.body[13] = '\0';
        strncpy(players[logged_count].name, msg.body, MAX_NAME_LENGTH);
        players[logged_count].name[MAX_NAME_LENGTH] = '\0';
        players[logged_count].addr = sender;
        players[logged_count].logged_in = true;
        players[logged_count].pebbles = STARTING_PEBBLES;
        
        printf("[Login] Welcome, %s\n", players[logged_count].name);
        logged_count++;
    }


    // Inicjalizujemy kolejkę
    queue_init(&queue);
    
    // Tworzymy wątki familiar
    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        if (pthread_create(&threads[i], NULL, familiar_thread, &queue) != 0)
            ERR("pthread_create");
        // Każdy wątek dostaje wskaźnik do kolejki jako argument
    }


    
    while (1)
    // Teraz pętla nie kończy się po 4 wiadomościach - kończy się tylko przez quit
    {
        ssize_t received = TEMP_FAILURE_RETRY(recvfrom(
            sockfd, &msg, sizeof(msg), 0,
            (struct sockaddr*)&sender, &sender_len
        ));
        
        if (received < 0) ERR("recvfrom");
        
        if (received != MSG_SIZE)
        {
            fprintf(stderr, "ERROR: nieprawidłowy rozmiar wiadomości\n");
            continue;
        }
        
        // Sprawdzamy czy nadawca to zalogowany gracz
        int player_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (players[i].addr.sin_addr.s_addr == sender.sin_addr.s_addr &&
                players[i].addr.sin_port == sender.sin_port)
            {
                player_idx = i;
                break;
            }
        }
        
        // Nieznany nadawca - ignorujemy
        if (player_idx == -1)
        {
            fprintf(stderr, "ERROR: wiadomość od nieznanego gracza\n");
            continue;
        }
        
        // Wskaźnik na gracza i jego przeciwnika
        player_t* player   = &players[player_idx];
        player_t* opponent = &players[1 - player_idx];
        // 1 - player_idx: jeśli gracz to 0, przeciwnik to 1 i odwrotnie
        
        switch (msg.type)
        {
            case 'l':
            {
                // Login po zalogowaniu - ignorujemy
                fprintf(stderr, "ERROR: gracz już zalogowany\n");
                break;
            }
            case 'c':
            {
                cast_body_t* cast = (cast_body_t*)msg.body;
                uint16_t spell = ntohs(cast->spell);
                uint16_t x     = ntohs(cast->x);
                uint16_t y     = ntohs(cast->y);
                
                if (spell >= SPELL_TYPES)
                {
                    fprintf(stderr, "ERROR: nieprawidłowe zaklęcie: %d\n", spell);
                    break;
                }
                
                if (x >= BOARD_SIZE || y >= BOARD_SIZE)
                {
                    fprintf(stderr, "ERROR: nieprawidłowe współrzędne: %d,%d\n", x, y);
                    break;
                }
                
                // Sprawdzamy czy gracz ma wystarczająco kamyczków
                if (player->pebbles < spell_cost[spell])
                {
                    printf("[tee hee] Not enough pebbles, %s!\n", player->name);
                    break;
                }
                
                // Odejmujemy kamyczki
                player->pebbles -= spell_cost[spell];
                
                // Wrzucamy do kolejki z imieniem gracza
                cast_cmd_t cmd;
                cmd.spell = spell;
                cmd.x = x;
                cmd.y = y;
                strncpy(cmd.name, player->name, MAX_NAME_LENGTH);
                cmd.name[MAX_NAME_LENGTH] = '\0';
                queue_push(&queue, cmd);
                break;
            }
            case 'q':
            {
                // Poddanie się
                printf("[Quit] %s quit. Goodbye!\n", player->name);
                printf("-= Congratulations, %s, you win! =-\n", opponent->name);
                // Kończymy program
                goto end;
                // goto bo jesteśmy w switch wewnątrz while - break nie wystarczy
            }
            default:
            {
                fprintf(stderr, "ERROR: nieznany typ wiadomości: %c\n", msg.type);
                break;
            }
        }
    }
    end:

    // Czekamy aż wątki skończą (nigdy nie skończą bo while(1),
    // ale to poprawny sposób sprzątania)
    for (int i = 0; i < THREAD_COUNT; i++)
        pthread_join(threads[i], NULL);
    
    queue_destroy(&queue);
    // Sprzątamy mutex i semafor

    // Na koniec zamykamy socket - TEMP_FAILURE_RETRY bo close()
    // może być przerwane przez sygnał.
    if (TEMP_FAILURE_RETRY(close(sockfd)) < 0)
        ERR("close");
        
    return EXIT_SUCCESS;
}
















/// NOTES 


//UDP SERWER 
// int sockfd = bind_inet_socket(port, SOCK_DGRAM, 0);

// struct sockaddr_in sender;
// socklen_t sender_len = sizeof(sender);

// while (1)
// {
//     ssize_t received = TEMP_FAILURE_RETRY(recvfrom(
//         sockfd, &msg, sizeof(msg), 0,
//         (struct sockaddr*)&sender, &sender_len
//     ));
//
//      // pytanie co robimy z wiadomoscia  tu 
// }