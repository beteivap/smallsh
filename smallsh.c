/*
Author: Paldin Bet Eivaz
Description: Implements a shell with a subset of features of well-known shells, such as bash.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_CMD_SIZE 2049     // max num of cmd line chars
#define MAX_ARGS 512          // max num of cmd line args
#define MAX_BG_PROCESSES 100  // max num of running background processes tracked

int exitStatus = 0;   // holds value set by normally terminating process
int sigNum = -5;      // holds terminating signal number
bool fgMode = false;  // if true -> processes can't run in background

/* stores the different elements included in a command */
struct cmdLine 
{
    char *cmd;             // cmd to run
    char *args[MAX_ARGS];  // cmd args
    int argCount;          // num args in cmd
    char *inputFile;       // file for input redirection
    char *outputFile;      // file for output redirection
    bool bgMode;           // if true -> run cmd in background 
};

/* initializes a cmdLine struct and returns it */
struct cmdLine initCmd() 
{
    struct cmdLine cmd;

    for(int i = 0; i < MAX_ARGS; i++) cmd.args[i] = NULL;
    cmd.argCount = 1;  // cmd itself is counted as an arg
    cmd.inputFile = NULL;
    cmd.outputFile = NULL;
    cmd.bgMode = false;

    return cmd;
}

/* parses and extracts each of the different elements included in the
 * inputCmd and stores them inside a cmdLine struct and returns it */
struct cmdLine parseCmd(char *inputCmd)
{
    // initialize cmdLine struct values
    struct cmdLine currCmd = initCmd();

    // get length of inputCmd so can iterate over each char
    int cmdLen = strlen(inputCmd);

    // if true -> redirect operator seen in cmd
    bool redirSeen = false;

    currCmd.cmd = &inputCmd[0];     // first element is cmd to run
    currCmd.args[0] = &inputCmd[0]; // store cmd as first arg for exec()

    // iterate over each character in the inputCmd
    for(int i = 0; i < cmdLen; i++) {

        // each element of cmd is delimited by a space, therefore
        // can get pointers to just the first char of each element
        // and replace spaces with the null terminator
        if(inputCmd[i] == ' ') {
            inputCmd[i] = '\0';

            // check if the last element in the cmd is an '&'
            // if it is then the cmd should be run in background
            if((i + 1 == cmdLen - 1) && (inputCmd[i+1] == '&')) {
                currCmd.bgMode = true;
            }

            // if we haven't seen a redirect operator then add element as an argument
            else if((inputCmd[i+1] != '<') && (inputCmd[i+1] != '>') && (!redirSeen)) {
                currCmd.args[currCmd.argCount] = &inputCmd[i+1];
                currCmd.argCount++;
            }
        }

        // if the element in the cmd is '<' then element
        // that follows must be file for input redirection
        else if(inputCmd[i] == '<') {
            currCmd.inputFile = &inputCmd[i+2];
            redirSeen = true;
        }

        // if the element in the cmd is '>' then element
        // that follows must be file for output redirection
        else if(inputCmd[i] == '>') {
            currCmd.outputFile = &inputCmd[i+2];
            redirSeen = true;
        }

    }

    return currCmd;
}

/* replaces the first occurrence of var with exp inside the src string by
 * shifting the characters after var to make room for the replacement 
 * code adapted from: http://tiny.cc/auowtz */
int varExp(char *src, char *var, char *exp)
{
    // pointer to the variable we want to expand
    char *varP = strstr(src, var);

    // if var does not exist in src string
    if(varP == NULL) return 0;

    // pointer to string we need to shift
    // to make room for var expansion
    char *strToShift = varP + strlen(var);

    // size of string we need to shift
    size_t sizeOfStr = strlen(varP) - strlen(var) + 1;

    // pointer to where we need to shift string to
    char *offset = varP + strlen(exp);

    // shift string to make room for var expansion
    memmove(offset, strToShift, sizeOfStr);

    // replace var with exp in the src string
    memmove(varP, exp, strlen(exp));

    return 1;
}

/* prints the exit status or terminating signal of the last terminated process */
void printStatus() 
{
    // if process terminated normally, exitStatus will be nonnegative
    if(exitStatus >= 0) {
        printf("exit value %d\n", exitStatus);
        fflush(stdout);
    }
    // if process terminated abnormally, sigNum will be nonnegative
    else if(sigNum >= 0 ){
        printf("terminated by signal %d\n", sigNum);
        fflush(stdout);
    }
}

/* gets the exit status or terminating signal of the last terminated process */
void setStatus(int childExitMethod) 
{
    // if process terminated normally, get exit status
    if (WIFEXITED(childExitMethod)) {
        exitStatus = WEXITSTATUS(childExitMethod);
        sigNum = -5;
    }
    // if process terminated abnormally, get terminating signal number
    else {
        sigNum = WTERMSIG(childExitMethod);
        exitStatus = -5;
    }
}

/* changes the working directory of the calling process */
void changeDir(struct cmdLine currCmd) 
{
    // if path of directory included as argument, change to it
    // otherwise change to the directory specified in HOME env var
    if(currCmd.args[1]) chdir(currCmd.args[1]);
    else chdir(getenv("HOME"));
}

/* adds background process pid to the array bgProcesses */
void addBgProcess(int bgProcesses[], int bgPid) {
    // place pid in first empty location found in array
    for(int i = 0; i < MAX_BG_PROCESSES; i++) {
        if(!bgProcesses[i]) {
            bgProcesses[i] = bgPid;
            break;
        }
    }
}

/* waits for each background process with pid listed in bgProcesses */
void waitBg(int bgProcesses[]) {
    int childExitMethod;
    
    // iterate over entire bgProcesses array
    for(int i = 0; i < MAX_BG_PROCESSES; i++) {
        
        // currently running bg process pids will be > 0
        if(bgProcesses[i] > 0) {
            
            // check if bg process has terminated, if not continue
            pid_t pidBg = waitpid(bgProcesses[i], &childExitMethod, WNOHANG);
            
            // if bg process has terminated, remove pid from array
            // and print the exit status or terminating signal
            if(pidBg > 0) {
                bgProcesses[i] = 0;
                printf("background pid %d is done: ", pidBg);
                fflush(stdout);
                
                if(WIFEXITED(childExitMethod)) {
                    printf("exit value %d\n", WEXITSTATUS(childExitMethod));
                    fflush(stdout);
                }
                else {
                    printf("terminated by signal %d\n", WTERMSIG(childExitMethod));
                    fflush(stdout);
                }
            }
        }
    }   
}

/* kills all currently running background processes */
void killBgProcesses(int bgProcesses[]) {
    for(int i = 0; i < MAX_BG_PROCESSES; i++) {
        if(bgProcesses[i] > 0) kill(bgProcesses[i], SIGTERM);
    }
}

/* redirects stdin and stdout to the input and output files specified in the cmd
 * code adapted from CS344 Module 5 exploration */
void redir(struct cmdLine currCmd) 
{
    // if file included for stdin redirection
    if(currCmd.inputFile) {
        // get file descriptor of input file
        int sourceFD = open(currCmd.inputFile, O_RDONLY);
        // if redirect fails exit
        if(sourceFD == -1) {
            printf("cannot open %s for input\n", currCmd.inputFile);
            fflush(stdout);
            exitStatus = 1;
            exit(1);
        }
        // redirect stdin to input file
        else dup2(sourceFD, 0);      
    }
    // if background cmd and file not included for stdin
    // redirection then redirect stdin to /dev/null
    else if(currCmd.bgMode) {
        int sourceFD = open("/dev/null", O_RDONLY);
        dup2(sourceFD, 0);
    }

    // if file included for stdout redirection
    if(currCmd.outputFile) {
        // get file descriptor of output file
        int targetFD = open(currCmd.outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        // if redirect fails exit
        if(targetFD == -1) {
            printf("cannot open %s for input\n", currCmd.outputFile);
            fflush(stdout);
            exitStatus = 1;
            exit(1);
        }
        // redirect stdout to output file
        else dup2(targetFD, 1);
    }
    // if background cmd and file not included for stdout
    // redirection then redirect stdout to /dev/null
    else if(currCmd.bgMode) {
        int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(targetFD, 1);
    }
}

/* executes a program using the args included in currCmd */
void execute(struct cmdLine currCmd)
{
    execvp(*currCmd.args, currCmd.args);
    // if exec() fails
    printf("%s: No such file or directory\n", *currCmd.args);
    fflush(stdout);
    exitStatus = 1;
    exit(1);
}

// handler for SIGTSTP signal
// toggles foreground only mode on/off
void catchSIGTSTP(int signo)
{
    // if foreground only mode is off, turn it on
    if(!fgMode) {
        char *msg = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg, 49);
        fgMode = true;
    }
    // if foreground only mode is on, turn it off
    else {
        char *msg = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, msg, 29);
        fgMode = false;
    }
}

int main() 
{
    char inputCmd[MAX_CMD_SIZE];  // holds cmd inputted by user
    int bgProcesses[MAX_BG_PROCESSES] = {0};  // holds pids of bg processes
    
    // get pid of smallsh
    char pid[7];
    sprintf(pid, "%d", getpid());

    struct sigaction SIGTSTP_action = {{0}}, SIGINT_action = {{0}};

    // setup handler for SIGTSTP (^Z) signal
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    
    // ignore SIGINT (^C) signal
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    while(1) {
        // get cmd from user
        printf(": ");
        fflush(stdout);
        fgets(inputCmd, MAX_CMD_SIZE, stdin);
        
        // wait for terminated bg processes
        waitBg(bgProcesses);
        
        // ignore blank lines or comments
        if((inputCmd[0] == '\n') || (inputCmd[0] == '#')) continue;

        // replace newline added to end of cmd by fgets with null terminator
        if ((strlen(inputCmd) > 0) && (inputCmd[strlen(inputCmd) - 1] == '\n')) {
            inputCmd[strlen(inputCmd) - 1] = '\0';
        }

        // replace all instances of $$ in inputCmd with pid of smallsh
        while(varExp(inputCmd, "$$", pid));

        // parse the input command and store elements in struct
        struct cmdLine currCmd = parseCmd(inputCmd);

        // if cmd is exit, kill all bg processes and exit shell
        if(!strcmp(currCmd.cmd, "exit")) {
            killBgProcesses(bgProcesses);
            exit(0);
        }
        // if cmd is cd, change to specified directory
        else if(!strcmp(currCmd.cmd, "cd")) {
            changeDir(currCmd);
            continue;
        }
        // if cmd is status, print exit status or 
        // terminating signal of last foreground process
        else if(!strcmp(currCmd.cmd, "status")) {
            printStatus();
            continue;
        }
        
        // if cmd is not one of the above built-in cmds
        // then fork a child process and exec() the cmd
        int childExitMethod;
        pid_t spawnPid = fork();

        switch(spawnPid) {
            // if fork fails exit shell
            case -1:
                perror("fork() failed!\n");
                exit(1);
                break;
            // child process
            case 0:
                // update SIGTSTP handler so that the child processes
                // ignore the signal instead of calling catchSIGTSTP handler
                SIGTSTP_action.sa_handler = SIG_IGN;
                sigaction(SIGTSTP, &SIGTSTP_action, NULL);

                // for child processes running in foreground reset
                // the handler for the SIGINT signal to default
                if(!currCmd.bgMode) {
                    SIGINT_action.sa_handler = SIG_DFL;
                    sigaction(SIGINT, &SIGINT_action, NULL);
                }

                // redirect input/ouput
                redir(currCmd);

                // execute the non built-in cmd
                execute(currCmd);
                break;
            // parent process
            default:
                // if child is to be run in foreground
                // then wait until child terminates
                if(!currCmd.bgMode || fgMode) {
                    spawnPid = waitpid(spawnPid, &childExitMethod, 0);
                    // get exit status of child
                    setStatus(childExitMethod);
                    // if child terminated abnormally, print terminating signal
                    if(sigNum >= 0) printStatus();
                }
                // if child is to be run in background
                // then don't wait until child terminates
                else {
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);
                    // add background process pid to array
                    addBgProcess(bgProcesses, spawnPid);
                }
                // wait for terminated bg processes
                // before prompting for next cmd
                waitBg(bgProcesses);
                break;
        }
    }
}
