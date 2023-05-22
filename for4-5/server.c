#include "common.h"
#include <pthread.h>

#define CLIENTS_NUMBER 3

int server_socket;

struct Program programs[CLIENTS_NUMBER];     // server stores information about all 3 programs
int programmers_clients[CLIENTS_NUMBER];     // server stores programmer clients sockets
sem_t semaphores[CLIENTS_NUMBER];            // semaphores[i] - number of tasks server haven't notify client about

void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

int createServerSocket(unsigned short port) {
    int sock;
    struct sockaddr_in server_address;

    // create socket
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        DieWithError("socket() failed");
    }

    // construct address
    memset(&server_address, 0, sizeof(server_address)); // clear memory for address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // make any interface available
    server_address.sin_port = htons(port);

    // try to bind socket with constructed address
    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        DieWithError("bind() failed");
    }

    if (listen(sock, CLIENTS_NUMBER) < 0) {
        DieWithError("listen() failed");
    }
    return sock;
}

int acceptTCPConnection(int server_socket) {
    int client_socket;
    struct sockaddr_in client_address;
    unsigned int client_len = sizeof(client_address);

    client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket < 0) {
        DieWithError("accept() failed");
    }

    printf("Handling client %s\n", inet_ntoa(client_address.sin_addr));                                          
    return client_socket;                                                                                                 
}

int get_reviewer_id(int author_id) {
    srand(time(0));
    // выбрать случайного программиста, который будет проверять задачу
    int reviewer_id;
    do {
        reviewer_id = rand() % CLIENTS_NUMBER;
    } while (reviewer_id == author_id);

    return reviewer_id;
}

void handleClientMessage(struct EventMessage *message) {
    // обработка события, о котором сообщил клиент
    int program_id = message->program_id;
    programs[program_id].status = message->status;

    if (message->status == WAIT_FOR_REVIEW) {
        if (programs[program_id].reviewer_id == -1) {
            // назначить ревьюера для задачи, если его еще нет
            programs[program_id].reviewer_id = get_reviewer_id(program_id);
        }
        sem_post(&semaphores[programs[program_id].reviewer_id]);
    }

    if (message->status == SUCCESS || message->status == FAIL) {
        sem_post(&semaphores[program_id]);
    }

    if (message->status == IN_PROCESS) {
        // если клиент начал писать новую программу, то обнулить предыдущего ревьюера
        programs[program_id].reviewer_id = -1;
    }
}

void makeResponseProgrammerMessage(int client_id, struct EventMessage* message) {
    // сначала смотрит, есть ли для программиста задачи на ревью или исправление кода
    for (int i = 0; i < CLIENTS_NUMBER; ++i) {
        if (i == client_id) {
            if (programs[i].status == FAIL) {
                // взять задачу внесения правок в свой код
                message->program_id = i;
                message->status = FAIL;
                return;
            }
        } else {
            // i != client_id
            if (programs[i].reviewer_id == client_id && programs[i].status == WAIT_FOR_REVIEW) {
                message->program_id = i;
                message->status = WAIT_FOR_REVIEW;
                return;
            }
        }
    }

    // если нет других задач и программа прошла проверку - программист новую программу
    if (programs[client_id].status == SUCCESS) {
        message->program_id = client_id;
        message->status = SUCCESS;
    }
}

void handleProgrammerClient(int client_id) {
    int client_socket = programmers_clients[client_id];

    struct EventMessage message;
    int event_message_size = sizeof(struct EventMessage);

    // отправить первое сообщение о начале работы
    message.program_id = client_id;
    message.status = IN_PROCESS;
    int message_size = send(client_socket, &message, event_message_size, 0);
    if (message_size != event_message_size) {
        DieWithError("send() failed");
    }

    for (;;) {
        // получить сообщение от клиента о его жизнедеятельности
        int message_size = recv(client_socket, &message, event_message_size, 0);
        if (message_size != event_message_size) {
            DieWithError("recv() failed");
        }

        if (message.program_id == -1)
        {
            // перед завершением своей работы клиент отправляет сообщение с program_id=-1
            break;
        }

        handleClientMessage(&message);

        // если клиент закончил делать что-то, то сервер должен выдать новое задание клиенту
        if (message.status == WAIT_FOR_REVIEW || message.status == FAIL || message.status == SUCCESS) {
            // сервер ждет, пока для программиста не появится задача
            sem_wait(&semaphores[client_id]);
            makeResponseProgrammerMessage(client_id, &message);

            // отправить сообщение клиенту с новым заданием для него
            message_size = send(client_socket, &message, event_message_size, 0);
            if (message_size != event_message_size) {
                DieWithError("send() failed");
            }
        }
    }

    close(client_socket); // закрываем клиентский сокет
    programmers_clients[client_id] = -1;
}

void *programmerThreadMain(void *thread_args) {
    pthread_detach(pthread_self());
    int client_id = *((int *)thread_args);
    free(thread_args);
    handleProgrammerClient(client_id);
    return (NULL);
}

void close_sources() {
    // завершение работы сервера
    for (int i = 0; i < CLIENTS_NUMBER; ++i) {
        if (programmers_clients[i] >= 0) {
            close(programmers_clients[i]); // закрываем клиентский сокет
        }
        sem_destroy(&semaphores[i]);       // уничтожаем семафоры
    }
    close(server_socket);
}

void interrupt_handler(int sign) {
    close_sources();
    exit(0);
}

int main(int argc, char *argv[]) {
    (void)signal(SIGINT, interrupt_handler);

    unsigned short server_port;
    pthread_t thread_id;
    int *thread_arg;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <SERVER PORT>\n", argv[0]);
        exit(1);
    }
    server_port = atoi(argv[1]);
    server_socket = createServerSocket(server_port);
    for (int i = 0; i < CLIENTS_NUMBER; ++i) {
        int client_socket = acceptTCPConnection(server_socket);
        programmers_clients[i] = client_socket;
        sem_init(&semaphores[i], 1, 0);

        thread_arg = (int *)malloc(sizeof(int));
        if (thread_arg == NULL) {
            DieWithError("malloc() failed");
        }
        *thread_arg = i;

        if (pthread_create(&thread_id, NULL, programmerThreadMain, (void *) thread_arg) != 0) {
            DieWithError("pthread_create() failed");
        }
        printf("with thread %ld\n", (long int)thread_id);
    }

    while (1) {}; // server works

    close_sources();
    return 0;
}