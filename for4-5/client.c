#include "common.h"

int event_message_size = sizeof(struct EventMessage);

int sock;
int client_id;

int write_time;     // время для написания кода
int review_time;    // время для ревью
int fix_time;       // время для фикса багов

void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

int get_time(int avg) {
    // return value in range [avg - 1, avg + 1]
    srand(time(0));
    int diff = rand() % 3 - 1;
    if (avg + diff > 0) {
        return avg + diff;
    }
    return 1;
}

void send_message(struct EventMessage *message) {
    int message_size = send(sock, message, event_message_size, 0);
    if (message_size != event_message_size) {
        DieWithError("send() failed");
    }
}

void write_code() {
    struct EventMessage message;

    // notify server about start
    message.program_id = client_id;
    message.status = IN_PROCESS;
    send_message(&message);
    printf("Programmer %d started write code\n", client_id + 1);

    int sleep_time = get_time(write_time);
    sleep(sleep_time);

    message.status = WAIT_FOR_REVIEW;
    send_message(&message);
    printf("Programmer %d finished write code\n", client_id + 1);
}

void review(int program_id) {
    struct EventMessage message;

    // notify server about start
    message.program_id = program_id;
    message.status = REVIEW;
    send_message(&message);
    printf("Programmer %d started review code for %d\n", client_id + 1, program_id + 1);

    int sleep_time = get_time(review_time);
    sleep(sleep_time);

    // шанс успешного вердикта - 60%, провала - 40%
    srand(time(0));
    int result = rand() % 10;
    if (result < 6) {
        message.status = SUCCESS;
        printf("Programmer %d finished review program %d, status: SUCCESS \n", client_id + 1, program_id + 1);
    } else {
        message.status = FAIL;
        printf("Programmer %d finished review program %d, status: FAIL\n", client_id + 1, program_id + 1);
    }

    send_message(&message);
}

void fix_code() {
    struct EventMessage message;

    // notify server about start
    message.program_id = client_id;
    message.status = FIX;
    send_message(&message);
    printf("Programmer %d started fix code\n", client_id + 1);

    int sleep_time = get_time(write_time);
    sleep(sleep_time);

    message.status = WAIT_FOR_REVIEW;
    send_message(&message);
    printf("Programmer %d finished fix code\n", client_id + 1);
}

void receiveAndHandleServerTask() {
    struct EventMessage message;
    int message_size = recv(sock, &message, event_message_size, 0);
    if (message_size != event_message_size) {
        DieWithError("recv() failed");
    }

    // first response - start coding
    if (message.status == IN_PROCESS) {
        client_id = message.program_id;
        write_code();
        return;
    }

    // review another client's code
    if (message.status == WAIT_FOR_REVIEW) {
        review(message.program_id);
        return;
    }

    if (message.status == FAIL) {
        fix_code();
        return;
    }

    if (message.status == SUCCESS) {
        write_code();
        return;
    }
}

void close_sources() {
    close(sock);
}

void interrupt_handler(int sign) {
    // send message to server about the closure
    struct EventMessage message;
    message.program_id = -1;
    message.status = FAIL;
    send_message(&message);

    close_sources();
    exit(0);
}

int main(int argc, char *argv[]) {
    (void)signal(SIGINT, interrupt_handler);
                  
    struct sockaddr_in server_address;
    unsigned short server_port;       
    char *serverIP;                   

    if (argc != 6) {    // Test for correct number of arguments
       fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Writing code time> <Review time> <Fix time>\n",
               argv[0]);
       exit(1);
    }

    serverIP = argv[1];
    server_port = atoi(argv[2]);
    write_time = atoi(argv[3]);
    review_time = atoi(argv[4]);
    fix_time = atoi(argv[5]);

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

    for (;;) {
        receiveAndHandleServerTask();
    }

    close(sock);
    exit(0);
}