#include "l8_common.h"


#define MAX_MSG_LEN         256
#define MAX_NAME_LEN        32
#define MAX_CONTENTS_LEN    64
#define MAX_WIZARDS         10
#define MAX_RECIPIENTS      5
#define COURIER_COUNT       3
#define COURIER_DELAY_MS    500
#define MAX_DISPATCH_QUEUE  20
#define MAX_TOTAL_PARCELS   30
#define STARTING_PEBBLES    15
#define JUDGE_INTERVAL_S    2
#define MAX_MESSAGES        20

typedef struct{
    char data[MAX_MSG_LEN];
}job_t;

typedef struct{
    job_t job[MAX_DISPATCH_QUEUE];
    int head;   // indeks gdzie czytamy (wątki familiar)
    int tail;   // indeks gdzie piszemy (wątek główny)
    int count;  // ile elementów jest teraz w kolejce

    pthread_mutex_t mutex;  
    pthread_cond_t not_empty;
}queue_t;

queue_t queue;





void queue_init(queue_t* q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    // head i tail zaczynają od 0, kolejka pusta
    
    if (pthread_mutex_init(&q->mutex, NULL) != 0)
        ERR("pthread_mutex_init");
    // mutex chroni kolejkę przed jednoczesnym dostępem z wielu wątków
    
    if(pthread_cond_init(&q->not_empty, NULL) != 0)\
        ERR("cond init");
}


void queue_push(queue_t* q, job_t job)
{
    pthread_mutex_lock(&q->mutex);
    // Blokujemy mutex - tylko my dotykamy kolejki
    
    if (q->count >= MAX_DISPATCH_QUEUE)
    {
        // Kolejka pełna - wyrzucamy komendę
        fprintf(stderr, "ERROR: kolejka pełna, komenda odrzucona\n");
        pthread_mutex_unlock(&q->mutex);
        return;
    }
    
    q->job[q->tail] = job;
    // Wpisujemy komendę na pozycję tail
    
    q->tail = (q->tail + 1) % MAX_DISPATCH_QUEUE;
    // Przesuwamy tail do przodu (cyklicznie - po MAX_QUEUE wracamy do 0)
    
    q->count++;
    
    pthread_mutex_unlock(&q->mutex);
    // Zwalniamy mutex
    
    pthread_cond_signal(&q->not_empty);
}


job_t queue_pop(queue_t* q)
{
    pthread_mutex_lock(&q->mutex);

    while(q->count == 0){
        pthread_cond_wait(&q->not_empty, &q->mutex); 
    }
    
    job_t cmd = q->job[q->head];
    // Pobieramy najstarszy element (head)
    q->head = (q->head + 1) % MAX_DISPATCH_QUEUE;
    // Przesuwamy head do przodu (cyklicznie)
    q->count--;

    pthread_mutex_unlock(&q->mutex);
    
    return cmd;
}

void queue_destroy(queue_t* q)
{
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    // Sprzątamy zasoby na końcu programu
}


void* courier_thread(void* arg)
{
    queue_t* q = (queue_t*)arg;

    while(1){
        job_t job = queue_pop(q);
        ms_sleep(COURIER_DELAY_MS);
        job.data[strcspn(job.data, "\n")] = '\0';
        char* save_ptr;
        char* type = strtok_r(job.data, ";", &save_ptr);

        if(type == NULL){
            fprintf(stderr, "[ERROR] Malformed message.\n");
            return NULL;
        }
        if (strcmp(type, "SEND") == 0)
        {
            char* priority = strtok_r(NULL, ";", &save_ptr);
            if(priority == NULL){
                fprintf(stderr, "[ERROR] Priority is empty.\n");
                return NULL;
            }
            if(atoi(priority) <1 || atoi(priority) > 5){
                fprintf(stderr, "[ERROR] Priotity invalid number.\n");
                return NULL;
            }
            char* content = strtok_r(NULL, ";", &save_ptr);
            if(content == NULL){
                fprintf(stderr, "[ERROR] content is empty \n");
                return NULL;
            }
            if(strlen(content) > MAX_CONTENTS_LEN){
                fprintf(stderr, "[ERROR] Invalid content name: '%s'.\n", content);
                return NULL;
            }

            char *recipients[MAX_RECIPIENTS];
            int count = 0;
            char* token  = strtok_r(NULL, ";", &save_ptr);
            while(token != NULL && count < MAX_RECIPIENTS){
                recipients[count] = token;
                count++;
                token = strtok_r(NULL, ";", &save_ptr);
            }
            if(count == 0){
                fprintf(stderr, "[ERROR] No recipients .\n");
                return NULL;
            }

            for(int i = 0; i < count; i++){
                printf("[Courier] Delivered to %s: %s (priority %s)\n", recipients[i], content, priority);
            }


        }
    }

    return NULL;
}


void usage(char* name)
{
    printf("%s <port>\n", name);
    printf("  port - port that accepts messages\n");
    exit(EXIT_FAILURE);
}

int work_with_data(char* buf){
// Zwraca 1 jeśli wiadomość poprawna, 0 jeśli błędna
// (żeby main wiedział czy zwiększać msg_count)
    char original[MAX_MSG_LEN];
    strncpy(original, buf, MAX_MSG_LEN);

    char* save_ptr;

    // usuwamy ostanie 
    buf[strcspn(buf, "\n")] = '\0';

    //type wyciagmy 
    char* type = strtok_r(buf, ";", &save_ptr);

    if(type == NULL){
        fprintf(stderr, "[ERROR] Malformed message.\n");
        return 0;
    }

    if (strcmp(type, "REG") == 0)
    {
        char* name = strtok_r(NULL, ";", &save_ptr);
        if(name == NULL) {
            fprintf(stderr, "[ERROR] Malformed REG message.\n");
            return 0;
        }
        if(strlen(name) > MAX_NAME_LEN) {
            fprintf(stderr, "[ERROR] Invalid wizard name: '%s'.\n", name);
            return 0;
        }
        printf("[REG] Welcome to the Chamber, <%s>! \n", name);
    }
    else if (strcmp(type, "SEND") == 0)
    {
        char* priority = strtok_r(NULL, ";", &save_ptr);
        if(priority == NULL){
            fprintf(stderr, "[ERROR] Priority is empty.\n");
            return 0;
        }
        if(atoi(priority) <1 || atoi(priority) > 5){
            fprintf(stderr, "[ERROR] Priotity invalid number.\n");
            return 0;
        }
        char* content = strtok_r(NULL, ";", &save_ptr);
        if(content == NULL){
            fprintf(stderr, "[ERROR] content is empty \n");
            return 0;
        }
        if(strlen(content) > MAX_CONTENTS_LEN){
            fprintf(stderr, "[ERROR] Invalid content name: '%s'.\n", content);
            return 0;
        }

        char *recipients[MAX_RECIPIENTS];
        int count = 0;
        char* token  = strtok_r(NULL, ";", &save_ptr);
        while(token != NULL && count < MAX_RECIPIENTS){
            recipients[count] = token;
            count++;
            token = strtok_r(NULL, ";", &save_ptr);
        }
        if(count == 0){
            fprintf(stderr, "[ERROR] No recipients .\n");
            return 0;
        }
        
        printf("[SEND] Parcel to ");
        for(int i = 0; i < count;i++){
            if(i < count -1) printf("<%s>, ",recipients[i]);
            else printf("<%s>", recipients[i]);
        }
        printf("(priority<%s>): %s\n", priority, content);
        
        job_t job;
        strncpy(job.data, original, MAX_MSG_LEN);
        queue_push(&queue, job);
    }
    else if (strcmp(type, "FETCH") == 0)
    {
        printf("[FETCH] Fetch request received\n");
    }
    else if (strcmp(type, "STATUS") == 0)
    {
        printf("[STATUS] Status request received\n");
    }
    else if (strcmp(type, "QUIT") == 0)
    {
        printf("[QUIT] Goodbye!\n");
    }
    else
    {
        fprintf(stderr, "[ERROR] Unknown message type: '%s'.\n", type);
        return 0;
    }
    
    return 1;
}




int main(int argc, char** argv)
{
    if (argc != 2) usage(argv[0]);

    uint16_t port = atoi(argv[1]);

    // TODO: Stage 1 - socket UDP + petla odbierania

    int sockfd = bind_inet_socket(port, SOCK_DGRAM, 0);
    printf("Listening on port %d\n", port);

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    char buf[MAX_MSG_LEN];

    int msg_count =0;

    queue_init(&queue);

    pthread_t threads[COURIER_COUNT];
    for(int i = 0; i < COURIER_COUNT;i++){
        if(pthread_create(&threads[i], NULL, courier_thread, &queue) != 0)
            ERR("thread create");
    }

    while(msg_count < MAX_MESSAGES){
        memset(buf, 0, sizeof(buf));
        
        ssize_t received = TEMP_FAILURE_RETRY(recvfrom(
            sockfd, &buf, sizeof(buf) -1 , 0,
            (struct sockaddr*)&sender, &sender_len
        ));
        if(received < 0 ) ERR("recvfrom");

        // paraca  ze  stringiem 

        if(work_with_data(buf))
            msg_count++;

        
    }

    for(int i = 0; i < COURIER_COUNT; i++)
        pthread_join(threads[i], NULL);

    queue_destroy(&queue);

    if(TEMP_FAILURE_RETRY(close(sockfd)))
        ERR("close");

    return EXIT_SUCCESS;
}













/// TESTOWANIE 
// pierwszy terminal :
//      ./sop-chamber 12345
//
//
// drugi  terminal:
//      echo "REG;Gandalf" | nc -u localhost 12345
// 
// albo jesli chcemy zeby wychodzil od razu: 
//      echo "REG;Gandalf" | nc -u -w1 localhost 12345
// inne  komendy:
//      echo "SEND;1;Eliksir;Bob;Alice" | nc -u localhost 12345
// 