#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define MAX_ARGS 512
#define MAX_INPUT 2048
#define MAX_BACKGROUND 100

int exitStatus = 0;
pid_t bgProcess[MAX_BACKGROUND];
int bgCount = 0;


void splitCommand(char* userInput, char** args){

    char* command = strtok(userInput, " ");
    int i = 0;

    while(command != NULL && i < MAX_ARGS - 1){
        args[i] = command;
        ++i;
        command = strtok(NULL, " ");
    }
    args[i] = NULL;

}

void runCd(char** args){
    char* path;
    if(args[1] == NULL){
        path = getenv("HOME");
    } else{
        path = args[1];
    }
    if(chdir(path) != 0){
        fprintf(stderr, "cd: %s", path);
        perror("");
    }
}

void handleSIGCHILD(int signo){
    int status;
    pid_t pid;
    

    while((pid = waitpid(-1, &status, WNOHANG)) > 0){
        int background = 0;

        for(int i = 0; i < bgCount; ++i){
            if(bgProcess[i] == pid){
                background = 1;
                bgProcess[i] = bgProcess[bgCount - 1];
                bgCount--;
                break;
            }  
        }
        
        if(background){
            if(WIFSIGNALED(status)){
                int sigNum = WTERMSIG(status);
                printf("backgroun pid %d is done. terminated by signal %d\n", pid, sigNum);
                fflush(stdout);
            } else if(WIFEXITED(status)){
                printf("background pid %d is done: exit value %d\n", pid, WEXITSTATUS(status));
                fflush(stdout);
            }
        }
        
        
    }
}




void runCommand(char** args){

    char* command;
    char* inputFile = NULL, *outputFile = NULL;
    int i = 0;
    int j = 0;
    int background = 0;
    int childStatus;
    char* _args[MAX_ARGS];


    if(args[0] == NULL || args[0][0] == '#'){
        return;
    }
    
    command = args[0];

    while(args[i] != NULL){
        if(strcmp(args[i], "<") == 0){
            inputFile = args[i + 1];
            i += 2;
        } else if(strcmp(args[i], ">") == 0){
            outputFile = args[i + 1];
            i += 2;
        } else{
            _args[j++] = args[i];
            i++;
        }
    }
    _args[j] = NULL;
    if(strcmp(args[i - 1], "&") == 0){
        background = 1;
        _args[i - 1] = NULL;
    }

    if(strcmp(command, "exit") == 0){
        exit(0);
    } else if(strcmp(command, "cd") == 0){
        runCd(args);
    } else if(strcmp(command, "status") == 0){
        printf("exit status %d\n", exitStatus);
    } else{
        pid_t spawnPid = fork();

        switch(spawnPid){
            case -1:
                perror("fork()\n");
                exitStatus = 1;
                break;
            case 0:
                if(!background){
                    struct sigaction default_a;
                    default_a.sa_handler = SIG_DFL;
                    sigaction(SIGINT, &default_a, NULL);
                }
                if(inputFile){
                    int fileDriver = open(inputFile, O_RDONLY);
                    if(fileDriver == -1){
                        perror("open file");
                        exit(1);
                    }
                    dup2(fileDriver, STDIN_FILENO);
                    close(fileDriver);
                }
                if(outputFile){
                    int fileDriver = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                    if(fileDriver == -1){
                        perror("open file");
                        exit(1);
                    }
                    dup2(fileDriver, STDOUT_FILENO);
                    close(fileDriver);
                }
                execvp(_args[0], _args);
                perror("execvp");
                exit(2);
                break;
            default:
                if(background){
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);

                    if(bgCount < MAX_BACKGROUND){
                        bgProcess[bgCount++] = spawnPid;
                    }
                } else{
                    waitpid(spawnPid, &childStatus, 0);
                }
                
                if(WIFEXITED(childStatus)){
                    exitStatus = WEXITSTATUS(childStatus);
                    
                }
                    
                
                break;
        }
    }

}


int main(){

    char userInput[MAX_INPUT];
    char* arguments[MAX_ARGS];

    struct sigaction SIGCHILD_a;
    SIGCHILD_a.sa_handler = handleSIGCHILD;
    SIGCHILD_a.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &SIGCHILD_a, NULL);

    struct sigaction SIGINT_a;
    SIGINT_a.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_a, NULL);

    while(1){
        printf(": ");
        fflush(stdout);
        fgets(userInput, 2048, stdin);
        userInput[strcspn(userInput, "\n")] = '\0';
        splitCommand(userInput, arguments);
        runCommand(arguments);
        printf("\n");
    }


    return 0;
}