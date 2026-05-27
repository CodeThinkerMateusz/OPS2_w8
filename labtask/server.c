#include "l8_common.h"

#define BACKLOG 3
#define MAXBUF 576
#define MAXADDR 5

typedef struct{
    struct sockaddr_in addr;
    int active;
    int32_t last_packet_id;
}ClientInfo;

typedef struct{
    int32_t packet_id;
    int32_t is_last;   // 0 = kolejna część, 1 = ostatnia część
    char data[568];   // 576 - 4 - 4 = 568 bajtów na dane
}Packet;



int get_client_index(struct sockaddr_in * sender_addr, ClientInfo * clients){
    //zwracanie istniejacego klienta 
    for(int i = 0; i < MAXADDR;i++){
        if(clients[i].active && clients[i].addr.sin_addr.s_addr == sender_addr->sin_addr.s_addr && clients[i].addr.sin_port == sender_addr->sin_port){
            return i;
        }
    }
    //nowy klient index
    for(int i = 0;i<MAXADDR;i++){
        if(!clients[i].active){
            clients[i].active = 1;
            clients[i].addr = *sender_addr;
            return i;
        }
    }
    return -1;
}


void server_work(int fd){
    ClientInfo clients[MAXADDR] = {0};

    while(1)
    {
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        Packet pack;

        ssize_t n = recvfrom(fd, &pack, sizeof(Packet), 0, (struct sockaddr *)&sender_addr, &addr_len);
        if(n<0) continue;

        int index = get_client_index(&sender_addr, clients);
        if(index == -1){
            fprintf(stderr, "Serwer pełny, ignoruję klienta\n");
            continue;
        }


        // przetwarzanie  danych klienta  
        int32_t id = ntohl(pack.packet_id);
        int32_t last = ntohl(pack.is_last);

        if(id == clients[index].last_packet_id +1){
            // To jest pakiet, którego oczekujemy
            clients[index].last_packet_id = id;
            printf("Klient %d wysłał pakiet %d (ostatni: %d)\n", index, id, last);

            sendto(fd, &pack.packet_id, sizeof(int32_t), 0, (struct sockaddr *)&sender_addr, addr_len);
            //ostatni packet zwalniamy miejsce  dla kolejnego klienta 
            if(last == 1){
                clients[index].active = 0;
                clients[index].last_packet_id = 0;
            }
        }

        else if(id <= clients[index].last_packet_id){
            sendto(fd, &pack.packet_id, sizeof(int32_t), 0, (struct sockaddr *)&sender_addr, addr_len);
            printf("Klient %d wysłał pakiet %d (ostatni: %d)\n", index, id, last);
            continue;
        }

    }

}


void usage(char *name) { fprintf(stderr, "USAGE: %s port\n", name); exit(EXIT_FAILURE);}

int main(int argc, char **argv){
    if(argc != 2){
        usage(argv[0]);
    }
    int fd;
    
    fd = bind_udp_socket(atoi(argv[1]));
    

    printf("Serwer działa na porcie %d\n", atoi(argv[1]));

    server_work(fd);

    return EXIT_SUCCESS;

}


