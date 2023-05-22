#include "common.h"

int sock;

void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

void close_sources() {
    close(sock);
}

void interrupt_handler(int sign) {
    close_sources();
    exit(0);
}

int main(int argc, char *argv[]) {
    (void)signal(SIGINT, interrupt_handler);
                  
    struct sockaddr_in server_address;
    unsigned short server_port;       
    char *serverIP;                   

    if (argc != 3) {    // Test for correct number of arguments
       fprintf(stderr, "Usage: %s <Server IP> <Server Port>\n", argv[0]);
       exit(1);
    }

    serverIP = argv[1];
    server_port = atoi(argv[2]);

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        DieWithError("socket() failed");
    }
        
    // Construct the server address structure
    memset(&server_address, 0, sizeof(server_address));     // clear structure memory
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(serverIP);   // server IP address
    server_address.sin_port = htons(server_port);           // server port

    // establish the connection to the server
    if (connect(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        DieWithError("connect() failed");
    }

    char buffer[100];
    for (;;) {
        int len = recv(sock, &buffer, 100, 0);
        printf("%s", buffer);
    }

    close(sock);
    exit(0);
}