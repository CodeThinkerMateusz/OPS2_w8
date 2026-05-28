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

int main(int argc, char** argv)
{
    if (argc != 2) usage(argv[0]);

    uint16_t port = atoi(argv[1]);

    // TODO: Stage 1 - socket UDP + petla odbierania
    (void)port;

    



    return EXIT_SUCCESS;
}