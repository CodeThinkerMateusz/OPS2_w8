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

void usage(char* name)
{
    printf("%s <port>\n", name);
    printf("  port - port that accepts messages\n");
    exit(EXIT_FAILURE);
}

int work_with_data(char* buf){
// Zwraca 1 jeśli wiadomość poprawna, 0 jeśli błędna
// (żeby main wiedział czy zwiększać msg_count)
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
        printf("[REG] Welcome to the Chamber, <%s>! \n", name);
    }
    else if (strcmp(type, "SEND") == 0)
    {
        char* priority = strtok_r(NULL, ";", &save_ptr);
        char* content = strtok_r(NULL, ";", &save_ptr);

        char *recipients[MAX_RECIPIENTS];
        int count = 0;
        char* token  = strtok_r(NULL, ";", &save_ptr);
        while(token != NULL && count < MAX_RECIPIENTS){
            recipients[count] = token;
            count++;
            token = strtok_r(NULL, ";", &save_ptr);
        }
        printf("[SEND] Parcel to ");
        for(int i = 0; i < count;i++){
            if(i < count -1) printf("<%s>, ",recipients[i]);
            else printf("<%s>", recipients[i]);
        }
        printf("(priority<%s>): %s\n", priority, content);
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
    (void)port;

    int sockfd = bind_inet_socket(port, SOCK_DGRAM, 0);
    printf("Listening on port %d\n", port);

    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    char buf[MAX_MSG_LEN];

    int msg_count =0;

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