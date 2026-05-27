#include "l8_common.h"
#include <fcntl.h>

#define MAXBUF 576

// Ta sama struktura co w serwerze!
typedef struct {
    int32_t packet_id;
    int32_t is_last;
    char data[568];
} Packet;





void doclient(int fd, struct sockaddr_in addr, int file){
    Packet pack; 
    int32_t chucknumber = 0;
    ssize_t bytesread;

    while((bytesread = read(file, pack.data, 568) ) > 0 ){
        chucknumber++;

        pack.packet_id = htonl(chucknumber);

        if(bytesread < 568){
            pack.is_last = htonl(1);
        }
        else{
            pack.is_last = htonl(0);
        }
        while(1){
            // wysylamy packiet i czeamy na  odpowiedz  
            sendto(fd, &pack, sizeof(Packet), 0, (struct sockaddr*)&addr, sizeof(addr));

            // czemu na  odpowiedz od  ack 
            int32_t ack_id;
            // serwer odsyla nam sockid 4 bajty 
            recvfrom(fd, &ack_id, sizeof(int32_t), 0, NULL, NULL);

            // czy serwer potwierdza  dokladnie ten packiet
            if(ntohl(ack_id) == (uint32_t)chucknumber){
                printf("Potwierdzono pakiet %d\n", chucknumber);
                break;
            }
        }
    }
}


int main(int argc, char** argv){
    if (argc != 4) {
        fprintf(stderr, "USAGE: %s ip port file\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;

    int file = open(argv[3], O_RDONLY);
    int fd = make_udp_socket();
    addr = make_address(argv[1], argv[2]);

    doclient(fd, addr, file);


    return EXIT_SUCCESS;
}