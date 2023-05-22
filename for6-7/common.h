#include <stdio.h>      
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <stdlib.h>     
#include <string.h>    
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

enum ProgramStatus
{
    IN_PROCESS,         // программист пишет программу
    WAIT_FOR_REVIEW,    // программа передана на проверку
    REVIEW,             // программа проверяется
    FAIL,               // программа написана неправильно
    SUCCESS,            // программа написана правильно
    FIX,                // программист исправляет свою работу
};

struct EventMessage {
    int program_id;
    enum ProgramStatus status;
};

struct Program {
    int reviewer_id;
    enum ProgramStatus status;
};
