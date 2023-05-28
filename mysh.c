#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <poll.h>

#define MAX_NUMBER_OF_ARGS 1000
#define MAX_LEN_OF_ARG 100
#define MAX_PIPE_OUTPUT 10000
#define MAX_TRAPS 100
#define MAX_TRAP_COMM_LENGTH 100

/*
    Global vars...
*/
int globaltrapcount = 0;
int* globaltrapids;
char** globaltraps;

char shellname[100] = { 0 };
int laststatus = 0;
char procfspath[1000] = "/proc";
bool inpipe = false;

#define NUM_OF_BUILTINS 31
char global_builtins[31][20] = {
    "help",
    "status",
    "exit",
    "name",
    "print",
    "echo",
    "pid",
    "ppid",
    "dirwhere",
    "dirbase",
    "dirchange",
    "dirmake",
    "dirremove",
    "dirlist",
    "linklist",
    "linkhard",
    "linksoft",
    "linkread",
    "unlink",
    "rename",
    "remove",
    "cpcat",
    "sysinfo",
    "shellinfo",
    "proc",
    "pids",
    "pinfo",
    "waitone",
    "waitall",
    "pipes",
    "trap"
};


bool isBuiltin(char* input) {
    for(int i = 0; i < NUM_OF_BUILTINS; i++) {
        if(strcmp(input, global_builtins[i]) == 0)
            return true;
    }
    return false;
}

/*
    Splits the given string to arguments, returns number of args
*/
int split(char* input, char*** output) {
    int n; int lineindex = 0; int charindex = 0; bool multiple_spaces = false; bool quote_opened = false;
    char** items;

    if(strlen(input) == 0) {
        output = NULL;
        return 0;
    }
    items = malloc(MAX_NUMBER_OF_ARGS*sizeof(char*));
    for(int i = 0; i < MAX_NUMBER_OF_ARGS; i++) {
        items[i] = calloc(MAX_LEN_OF_ARG+1, sizeof(char));
    }

    for(int i = 0; i < strlen(input); i++) {
        if(input[i] == ' ' && multiple_spaces == false) {
            multiple_spaces = true;
            if(strlen(items[lineindex]) == 0) {
                charindex = 0;
            } else {
                lineindex++;
                charindex = 0;
            }
        } else if(input[i] == '"') {
            multiple_spaces = false;
            if(strlen(items[lineindex]) == 0) {
                charindex = 0;
            } else {
                lineindex++;
                charindex = 0;
            }

            if(i < strlen(input)-1)
                i++;
            while(input[i] != '"' && i < strlen(input)) {
                items[lineindex][charindex] = input[i];
                charindex++;
                i++;
            }
        } else {
            if(input[i] != ' ') {
                multiple_spaces = false;
                items[lineindex][charindex] = input[i];
                charindex++;
            }
        }
    }
    if(multiple_spaces == false)
        lineindex++;

    //Remove new line
    if(items[lineindex-1][strlen(items[lineindex-1])-1] == '\n')
        items[lineindex-1][strlen(items[lineindex-1])-1] = '\0';
    
    //copy to output
    *output = malloc(lineindex*sizeof(char*));
    for(int i = 0; i < lineindex; i++) {
        (*output)[i] = calloc(strlen(items[i])+1, sizeof(char));
        strcpy((*output)[i], items[i]);
    }
    
    
    for(int i = 0; i < MAX_NUMBER_OF_ARGS; i++) {
        free(items[i]);
    }
    free(items);

    return lineindex;
}

/*
    Clears all zombie processes
*/
void clearZombies() {
    while(1) {
        int out = waitpid(-1, NULL, WNOHANG);
        if(out <= 0)
            return;
    }
}
void clearZombiessig(int signum) {
    while(1) {
        int out = waitpid(-1, NULL, WNOHANG);
        if(out <= 0)
            return;
    }
}


//////////////////////////////////////
//    PREPROSTI VGRAJENI UKAZI      //
//////////////////////////////////////
int helpcom(char** output) {
    char helpmsg[] = "This is the help menu\n";
    *output = calloc(strlen(helpmsg)+1, sizeof(char));
    strcpy(*output, helpmsg);
    return 0;
}

int statuscom(char** output) {
    char tmp[10] = { 0 };
    sprintf(tmp, "%d", laststatus);
    *output = calloc(strlen(tmp)+1, sizeof(char));
    strcpy(*output, tmp);
    (*output)[strlen(tmp)] = '\n';
    return 0;
}

int printcom(int argc, char** args, char** output) {
    int len = 0;
    for(int i = 1; i < argc; i++) {
        len += strlen(args[i]);
        if(i != argc-1)
            len++;
    }
    len+= 2;

    *output = calloc(len+1, sizeof(char));
    for(int i = 1; i < argc; i++) {
        strcat(*output, args[i]);
        if(i != argc-1)
            strcat(*output, " ");
    }
    return 0;
}

int pidcom(char** output) {
    char tmp[10] = { 0 };
    sprintf(tmp, "%d", getpid());
    *output = calloc(strlen(tmp)+1, sizeof(char));
    strcpy(*output, tmp);
    (*output)[strlen(tmp)] = '\n';
    return 0;
}

int ppidcom(char** output) {
    char tmp[10] = { 0 };
    sprintf(tmp, "%d", getppid());
    *output = calloc(strlen(tmp)+1, sizeof(char));
    strcpy(*output, tmp);
    (*output)[strlen(tmp)] = '\n';
    return 0;  
}

//////////////////////////////////////////////
//    Vgrajeni ukazi za delo z imeniki      //
//////////////////////////////////////////////
int dirchangecom(char* dest) {
    char dest2[100] = { 0 };
    if(dest == NULL) {
        strcpy(dest2, "/");
    } else {
        strcpy(dest2, dest);
    }
    
    if(chdir(dest2) < 0) {
        int err = errno;
        printf("dirchange: %s\n", strerror(err));
        fflush(stdout);
        return err;
    }

    return 0;
}

int dirwherecom(char** output) {
    char buffer[1000] = { 0 };
    if(getcwd(buffer, sizeof(buffer)) == NULL) {
        int err = errno;
        printf("dirwhere: %s\n", strerror(err));
        fflush(stdout);
        return err;
    }
    *output = calloc(strlen(buffer)+2, sizeof(char));
    strcpy(*output, buffer);
    (*output)[strlen(buffer)] = '\n';
    (*output)[strlen(buffer)+1] = '\0';
    return 0;
}

int dirbasecom(char** output) {
    char* dir;
    dirwherecom(&dir);
    char* bn = basename(dir);
    *output = calloc(strlen(bn)+1, sizeof(char));
    strcpy(*output, bn);
    (*output)[strlen(bn)] = '\0';
    free(dir);
    return 0;
}

int dirmakecom(char* dest) {
    if(mkdir(dest, 0777) < 0) {
        int err = errno;
        printf("dirmake: %s\n", strerror(err));
        fflush(stdout);
        return err;
    }
    return 0;
}

int dirremovecom(char* target) {
    if(rmdir(target) < 0) {
        int err = errno;
        printf("dirremove: %s\n", strerror(err));
        fflush(stdout);
        return err; 
    }
    return 0;
}

int dirlistcom(char* target, char** output) {
    char* path;
    if(target == NULL) {
        dirwherecom(&path);
        path[strlen(path)-1] = '\0'; // \n
    } else {
        path = calloc(101, sizeof(char));
        strcpy(path, target);
        path[100] = '\0';
    }
    
    DIR *dir = opendir(path);
    if(dir == NULL) {
        int err = errno;
        printf("dirlist: %s\n", strerror(err));
        fflush(stdout);
        free(path);
        return err; 
    }

    struct dirent *entry = readdir(dir);
    char line[10000] = { 0 };
    // printf("%s", entry->d_name);
    strcat(line, entry->d_name);
    entry = readdir(dir);
    while(entry != NULL) {
        // printf("  %s", entry->d_name);
        strcat(line, "  ");
        strcat(line, entry->d_name);
        entry = readdir(dir);
    }
    // printf("\n");
    strcat(line, "\n");

    if(closedir(dir) < 0) {
        int err = errno;
        printf("dirlist: %s\n", strerror(err));
        fflush(stdout);
        free(path);
        return err;   
    }

    *output = calloc(strlen(line)+1, sizeof(char));
    strcpy(*output, line);
    (*output)[strlen(line)] = '\0';

    free(path);
    return 0;
}

//////////////////////////////////////////////
//    Vgrajeni ukazi za delo z datotekami   //
//////////////////////////////////////////////

int linkhardcom(char* target, char* name) {
    if(link(target, name) < 0) {
        int err = errno;
        printf("linkhard: %s\n", strerror(err));
        fflush(stdout);
        return err; 
    }
    return 0;
}

int linksoftcom(char* target, char* name) {
    if(symlink(target, name) < 0) {
        int err = errno;
        printf("linksoft: %s\n", strerror(err));
        fflush(stdout);
        return err; 
    }
    return 0;
}

int linkreadcom(char* target, char** output) {
    char buffer[100] = { 0 };
    if(readlink(target, buffer, sizeof(buffer)) < 0) {
        int err = errno;
        printf("linkread: %s\n", strerror(err));
        fflush(stdout);
        return err; 
    }
    
    *output = calloc(strlen(buffer)+2, sizeof(char));
    strcpy(*output, buffer);
    (*output)[strlen(buffer)] = '\n';
    (*output)[strlen(buffer)+1] = '\0';
    return 0;
}

int linklistcom(char* target, char** output) {
    char* path;
    dirwherecom(&path);
    path[strlen(path)-1] = '\0'; // \n
    
    DIR *dir = opendir(path);
    if(dir == NULL) {
        int err = errno;
        printf("linklist: %s\n", strerror(err));
        fflush(stdout);
        free(path);
        return err; 
    }

    struct dirent *entry = readdir(dir);
    while(entry != NULL && strcmp(entry->d_name, target) != 0) {
        entry = readdir(dir);
    }

    if(closedir(dir) < 0) {
        int err = errno;
        printf("linklist: %s\n", strerror(err));
        fflush(stdout);
        free(path);
        return err;   
    }
    if(entry == NULL) {
        return -1;
        free(path);
    }

    DIR *dir2 = opendir(path);
    if(dir2 == NULL) {
        int err = errno;
        printf("linklist: %s\n", strerror(err));
        fflush(stdout);
        free(path);
        return err; 
    }

    //go through it the 2nd time
    char line[10000] = { 0 };
    struct dirent *entry2 = readdir(dir2);
    while(entry2 != NULL) {
        if(entry2->d_ino == entry->d_ino) {
            //printf("%s\n", entry2->d_name);
            if(strlen(line) > 0)
                strcat(line, "  ");
            strcat(line, entry2->d_name);
        }
        entry2 = readdir(dir2);
    }
    strcat(line, "\n");

    if(closedir(dir2) < 0) {
        int err = errno;
        printf("linklist: %s\n", strerror(err));
        fflush(stdout);
        free(path);
        return err;   
    }

    free(path);

    *output = calloc(strlen(line)+1, sizeof(char));
    strcpy(*output, line);
    (*output)[strlen(line)] = '\0';

    return 0;
}

int unlinkcom(char* target) {
    if(unlink(target) < 0) {
        int err = errno;
        printf("unlink: %s\n", strerror(err));
        fflush(stdout);
        return err;  
    }
    return 0;
}

int renamecom(char* oldpath, char* newpath) {
    if(rename(oldpath, newpath) < 0) {
        int err = errno;
        printf("rename: %s\n", strerror(err));
        fflush(stdout);
        return err;  
    }
    return 0;
}

int removecom(char* target) {
    if(remove(target) < 0) {
        int err = errno;
        printf("remove: %s\n", strerror(err));
        fflush(stdout);
        return err;  
    }
    return 0;
}

int cpcatcom(char* src, char* dest, char** output) {
    //open the file
    int desk = open(src, O_RDONLY);
    if(desk < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        return err;  
    }

    //get the size
    struct stat fs;
    if(stat(src, &fs) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        return err;
    }
    long size = fs.st_size;

    //allocate memory and read the file into it
    char* content = calloc(size+1, sizeof(char));
    if(read(desk, content, size) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;  
    }

    //close the file
    if(close(desk) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;  
    }

    if(dest == NULL) {
        *output = calloc(size+1, sizeof(char));
        strcpy(*output, content);
        (*output)[size] = '\0';
        free(content);
        return 0;
    }

    //create the dest file
    desk = creat(dest, O_WRONLY);
    if(desk < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;    
    }

    //change permissions
    if(chmod(dest, 0777) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err; 
    }

    //write to it
    if(write(desk, content, size) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;
    }

    //close the file
    if(close(desk) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;  
    }

    free(content);
    return 0;
}

int cpcatcompipe(char* src, char* dest, char** output ) {
    int size = strlen(src);

    char* content = calloc(size+1, sizeof(char));
    strcpy(content, src);

    if(dest == NULL) {
        *output = calloc(size+1, sizeof(char));
        strcpy(*output, content);
        (*output)[size] = '\0';
        free(content);
        return 0;
    }

    //create the dest file
    int desk = creat(dest, O_WRONLY);
    if(desk < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;    
    }

    //change permissions
    if(chmod(dest, 0777) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err; 
    }

    //write to it
    if(write(desk, content, size) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;
    }

    //close the file
    if(close(desk) < 0) {
        int err = errno;
        printf("cpcat: %s\n", strerror(err));
        fflush(stdout);
        free(content);
        return err;  
    }

    free(content);
    return 0;
}

//////////////////////////////////////////////
//    Vgrajeni ukazi za info o sys          //
//////////////////////////////////////////////

int sysinfocom(char** output) {
    struct utsname data;
    if(uname(&data) < 0) {
        int err = errno;
        printf("sysinfo: %s\n", strerror(err));
        fflush(stdout);
        return err;     
    }

    char buffer[10000] = { 0 };
    strcat(buffer, "Sysname: ");
    strcat(buffer, data.sysname);
    strcat(buffer, "\nNodename: ");
    strcat(buffer, data.nodename);
    strcat(buffer, "\nRelease: ");
    strcat(buffer, data.release);
    strcat(buffer, "\nVersion: ");
    strcat(buffer, data.version);
    strcat(buffer, "\nMachine: ");
    strcat(buffer, data.machine);

    *output = calloc(strlen(buffer)+2, sizeof(char));
    strcpy(*output, buffer);
    (*output)[strlen(buffer)] = '\n';
    (*output)[strlen(buffer)+1] = '\0';

    return 0;
}

int shellinfocom(char** output) {
    char uid[10] = { 0 };
    sprintf(uid, "%d", getuid());
    char euid[10] = { 0 };
    sprintf(euid, "%d", geteuid());
    char gid[10] = { 0 };
    sprintf(gid, "%d", getgid());
    char egid[10] = { 0 };
    sprintf(egid, "%d", getegid());

    char buffer[1000] = { 0 };
    strcat(buffer, "Uid: ");
    strcat(buffer, uid);
    strcat(buffer, "\nEUid: ");
    strcat(buffer, euid);
    strcat(buffer, "\nGid: ");
    strcat(buffer, gid);
    strcat(buffer, "\nEGid: ");
    strcat(buffer, egid);

    *output = calloc(strlen(buffer)+2, sizeof(char));
    strcpy(*output, buffer);
    (*output)[strlen(buffer)] = '\n';
    (*output)[strlen(buffer)+1] = '\0';

    return 0;
}

//////////////////////////////////////////////
//    Vgrajeni ukazi za delo s procesi      //
//////////////////////////////////////////////

int proccom(char* path) {
    //check if the path exists
    if(access(path, F_OK|R_OK) < 0) {
        return 1;
    }
    strcpy(procfspath, path);
    return 0;
}

int pidscom(char** output) {
    if(strlen(procfspath) == 0)
        return -1;
    
    char* procdirlist;
    dirlistcom(procfspath, &procdirlist);
    procdirlist[strlen(procdirlist)-1] = '\0'; //remove \n
    //split it
    char** entries = NULL;
    int n = split(procdirlist, &entries);
    free(procdirlist);

    char* buffer = calloc(11*MAX_NUMBER_OF_ARGS, sizeof(char));
    //check which ones are numerical
    for(int i = 0; i < n; i++) {
        bool numerical = true;
        for(int j = 0; j < strlen(entries[i]); j++) {
            if(entries[i][j] < 48 || entries[i][j] > 57) {
                numerical = false;
                break;
            }
        }
        if(numerical == true) {
            strcat(buffer, entries[i]);
            strcat(buffer, "\n");
        }
    }
    buffer[(11*MAX_NUMBER_OF_ARGS)-1] = '\0';

    *output = calloc(strlen(buffer)+1, sizeof(char));
    strcpy(*output, buffer);
    (*output)[strlen(buffer)] = '\0';
    free(buffer);


    if(entries != NULL) {
        for(int i = 0; i < n; i++) {
            free(entries[i]);
        }
        free(entries);
    }

    return 0;
}

int pinfocom(char** output) {
    int status = 0;

    char* pidsstring;
    pidscom(&pidsstring);
    //replace \n with ' '
    for(int i = 0; i < strlen(pidsstring); i++) {
        if(pidsstring[i] == '\n')
            pidsstring[i] = ' ';
    }

    char** processes = NULL;
    int n = split(pidsstring, &processes);
    free(pidsstring);

    char* outputbuffer = calloc(n*100, sizeof(char));
    sprintf(outputbuffer, "%5s %5s %6s %s\n", "PID", "PPID", "STANJE", "IME");

    //go through all process stat files
    for(int i = 0; i < n; i++) {
        char processpath[1000] = { 0 };
        strcat(processpath, procfspath);
        strcat(processpath, "/");
        strcat(processpath, processes[i]);
        strcat(processpath, "/stat");
        
        //open the file
        int desk = open(processpath, O_RDONLY);
        if(desk < 0) {
            int err = errno;
            printf("pinfo: %s\n", strerror(err));
            fflush(stdout);
            status = err;
            goto END; 
        }

        int size = 1000;
        //read the contents
        char* content = calloc(size+1, sizeof(char));
        if(read(desk, content, size) < 0) {
            int err = errno;
            printf("pinfo: %s\n", strerror(err));
            fflush(stdout);
            free(content);
            status = err;
            goto END;
        }

        // //change ( and ) for " so that we can use the split function
        // for(int j = 0; j < strlen(content); j++) {
        //     if(content[j] == '(' || content[j] == ')')
        //         content[j] = '"';
        // }
        if(content[strlen(content)-1] == '\n')
            content[strlen(content)-1] = '\0';

        //extract data
        char pid[100] = { 0 };
        char pname[200] = { 0 };
        char state[200] = { 0 };
        char ppid[100] = { 0 };
        int index = 0;

        int j = 0;
        for(; content[j] != ' '; j++) {
            pid[index] = content[j];
            index++;
        }
        j+=2;
        pid[index] = '\0';
        index = 0;
        
        for(; content[j] != ')'; j++) {
            pname[index] = content[j];
            index++;
        }
        j+=2;
        pname[index] = '\0';
        index = 0;

        for(; content[j] != ' '; j++) {
            state[index] = content[j];
            index++;
        }
        j++;
        state[index] = '\0';
        index = 0;

        for(; content[j] != ' '; j++) {
            ppid[index] = content[j];
            index++;
        }

        char line[300] = { 0 };
        sprintf(line, "%5s %5s %6s %s\n", pid, ppid, state, pname);
        strcat(outputbuffer, line);

        free(content);
        //close the file
        if(close(desk) < 0) {
            int err = errno;
            printf("pinfo: %s\n", strerror(err));
            fflush(stdout);
            status = err;
            goto END; 
        }
    }

    *output = calloc(strlen(outputbuffer)+1, sizeof(char));
    strcpy(*output, outputbuffer);
    (*output)[strlen(outputbuffer)] = '\0';
    free(outputbuffer);

    END:
    if(processes != NULL) {
        for(int i = 0; i < n; i++)
            free(processes[i]);
        free(processes);
    }
    return status;
}

int waitonecom(char* pidstr) {
    if(pidstr != NULL) {
        int pid = atoi(pidstr);
        int status;
        if(waitpid(pid, &status, 0) < 0) {
            int err = errno;
            printf("waitone: %s\n", strerror(err));
            fflush(stdout);
            return err;
        } 
    } else {
        int status;
        if(wait(&status) < 0) {
            int err = errno;
            printf("waitone: %s\n", strerror(err));
            fflush(stdout);
            return err;
        }
    }
    return 0;
}

int waitallcom() {
    int status;
    while(wait(&status) > 0) {}
    return 0;
}

//////////////////////////////////////////////
//    Vgrajeni ukazi za delo s traps        //
//////////////////////////////////////////////

int evaluate(int argc, char** args);

void traphandler(int signum) {
    char* command = NULL;
    for(int i = 0; i < globaltrapcount; i++) {
        if(globaltrapids[i] == signum)
            command = globaltraps[i];
    }

    if(command == NULL)
        return;

    char** tokens;
    int n = split(command, &tokens);
    if(tokens[0][0] == '#' || strcmp(tokens[0], "\n") == 0) {
        return; 
    }

    //check if it should be run in the background
    bool run_background = false;
    int forkpid = -1;
    if(strcmp(tokens[n-1], "&") == 0) {
        run_background = true;
        n--;
        forkpid = fork();
        if(forkpid < 0)
            laststatus = forkpid;
    }
                                                                //child
    if(run_background == false || (run_background == true && forkpid == 0)) {
        //evaluate
        laststatus = evaluate(n, tokens);
    } else {
        laststatus = 0;
    }
    
    //free up tokens mem
    for(int i = 0; i < n; i++) {
        free(tokens[i]);
    }
    free(tokens);

    //child ran in the background
    if(run_background == true && forkpid == 0)
        exit(laststatus);
}

int printtrapscom(char** output) {
    if(globaltrapcount == 0)
        return 0;
    
    char* tmpoutput = calloc(MAX_TRAPS*MAX_TRAP_COMM_LENGTH, sizeof(char));
    for(int i = 0; i < globaltrapcount; i++) {
        char line[200];
        sprintf(line, "%d: %s", globaltrapids[i], globaltraps[i]);
        if(i > 0)
            strcat(tmpoutput, "\n");
        strcat(tmpoutput, line);
    }
    strcat(tmpoutput, "\n");

    *output = calloc(strlen(tmpoutput) + 1, sizeof(char));
    strcpy(*output, tmpoutput);
    free(tmpoutput);

    return 0;
}

int deltrapcom(char* strtrapid) {
    int trapid = atoi(strtrapid);
    signal(trapid, SIG_DFL);

    int index = -1;
    for(int i = 0; i < globaltrapcount; i++) {
        if(globaltrapids[i] == trapid) {
            index = i;
            break;
        }
    }
    if(index == -1)
        return -1;

    //create new trap array
    char** tmptraparr = malloc(MAX_TRAPS*sizeof(char*));
    for(int i = 0; i < MAX_TRAPS; i++)
        tmptraparr[i] = calloc(MAX_TRAP_COMM_LENGTH, sizeof(char));
    
    int* tmptrapids = calloc(MAX_TRAPS, sizeof(int));

    //go through the old trap array
    int counter = 0;
    for(int i = 0; i < globaltrapcount; i++) {
        if(i != index) {
            strcpy(tmptraparr[counter], globaltraps[i]);
            tmptrapids[counter] = globaltrapids[i];
            counter++;
        }
    }

    //free up the old one
    for(int i = 0; i < MAX_TRAPS; i++) {
        free(globaltraps[i]);
    }
    free(globaltraps);
    free(globaltrapids);

    //change global vars
    globaltraps = tmptraparr;
    globaltrapids = tmptrapids;
    globaltrapcount = counter;

    return 0;
}

int addtrapcom(char* strtrapnum, char* command) {
    int trapnum = atoi(strtrapnum);

    globaltrapids[globaltrapcount] = trapnum;
    strcpy(globaltraps[globaltrapcount], command);

    globaltrapcount++;

    signal(trapnum, traphandler);
    return 0;
}


int evaluate(int argc, char** args) {
    char* output = NULL; char** input; int inputc = 0; int status; int indesc; int outdesc;
    bool redirected_in = false; bool redirected_out = false; bool run_background = false; char* inpath; char* outpath;

    if(argc == 0) {
        return -1;
    }

    /*
        Check for redirections
    */
    //redirected out
    if(args[argc-1][0] == '>') {
        redirected_out = true;

        outpath = calloc(strlen(args[argc-1])+1, sizeof(char));
        for(int i = 1; i < strlen(args[argc-1]); i++)
            outpath[i-1] = args[argc-1][i];
        outdesc = creat(outpath, O_WRONLY);
        if(outdesc < 0) {
            int err = errno;
            printf("%s\n", strerror(err));
            fflush(stdout);
            return err;
        }
        chmod(outpath, 0777);
        int err = errno;
        if(err < 0) {
            printf("%s\n", strerror(err));
            fflush(stdout);
            return err;
        }

        argc--;
    } else {
        outdesc = 1;
    }
    //redirected in
    if(args[argc-1][0] == '<') {
        redirected_in = true;

        inpath = calloc(strlen(args[argc-1])+1, sizeof(char));
        for(int i = 1; i < strlen(args[argc-1]); i++)
            inpath[i-1] = args[argc-1][i];
        indesc = open(inpath, O_RDONLY);
        if(indesc < 0) {
            int err = errno;
            printf("%s\n", strerror(err));
            fflush(stdout);
            return err;
        }

        // char line[MAX_NUMBER_OF_ARGS*MAX_LEN_OF_ARG];
        // char fromfile[MAX_NUMBER_OF_ARGS*MAX_LEN_OF_ARG];
        struct stat fs;
        stat(inpath, &fs);
        unsigned long filesize = fs.st_size;

        char* fromfile = calloc(filesize+1, sizeof(char));
        read(indesc, fromfile, filesize);
        if(fromfile[strlen(fromfile)-1] == '\n')
            fromfile[strlen(fromfile)-1] = '\0';

        inputc = argc;
        input = malloc(argc*sizeof(char*));
        for(int i = 0; i < argc-1; i++) {
            input[i] = calloc(strlen(args[i])+1, sizeof(char));
            strcpy(input[i], args[i]);
        }
        input[argc-1] = calloc(strlen(fromfile)+1,sizeof(char));
        strcpy(input[argc-1], fromfile);

        free(fromfile);
        argc--;
    } else {
        input = args;
        inputc = argc;
    }



    if(strcmp(input[0], "help") == 0) {
        status = helpcom(&output);
    }
    else if(strcmp(input[0], "status") == 0) {
        status = statuscom(&output);
    }
    else if(strcmp(input[0], "exit") == 0) {
        if(inputc >= 2)
            exit(atoi(input[1]));
    }
    else if(strcmp(input[0], "name") == 0) {
        if(inputc > 1) {
            if(strlen(input[1]) > 8) {
                return 1;
            }
            strcpy(shellname, input[1]);
            return 0;
        } else {
            output = calloc(strlen(shellname)+2, sizeof(char));
            strcpy(output, shellname);
            strcat(output, "\n");
            status = 0;
        }
    }
    else if(strcmp(input[0], "print") == 0) {
        status = printcom(inputc, input, &output);
    }
    else if(strcmp(input[0], "echo") == 0) {
        status = printcom(inputc, input, &output);
        strcat(output, "\n");
    }
    else if(strcmp(input[0], "pid") == 0) {
        status = pidcom(&output);
    }
    else if(strcmp(input[0], "ppid") == 0) {
        status = ppidcom(&output);
    }
    else if(strcmp(input[0], "dirwhere") == 0) {
        status = dirwherecom(&output);
    }
    else if(strcmp(input[0], "dirbase") == 0) {
        status = dirbasecom(&output);
    }
    else if(strcmp(input[0], "dirchange") == 0) {
        if(inputc == 1) {
            status = dirchangecom(NULL);
        } else {
            status = dirchangecom(input[1]);
        }
        return status;
    }
    else if(strcmp(input[0], "dirmake") == 0) {
        if(inputc == 1)
            return -1;
        status = dirmakecom(input[1]);
        return status;
    }
    else if(strcmp(input[0], "dirremove") == 0) {
        if(inputc == 1)
            return -1;
        status = dirremovecom(input[1]);
        return status;
    }
    else if(strcmp(input[0], "dirlist") == 0) {
        if(inputc == 1) {
            status = dirlistcom(NULL, &output);
        } else {
            status = dirlistcom(input[1], &output);
        }
    }
    else if(strcmp(input[0], "linkhard") == 0) {
        if(inputc < 3)
            return -1;
        status = linkhardcom(input[1], input[2]);
        return status;
    }
    else if(strcmp(input[0], "linksoft") == 0) {
        if(inputc < 3)
            return -1;
        status = linksoftcom(input[1], input[2]);
        return status;
    }
    else if(strcmp(input[0], "linkread") == 0) {
        if(inputc == 1)
            return -1;
        status = linkreadcom(input[1], &output);
    }
    else if(strcmp(input[0], "linklist") == 0) {
        if(inputc == 1)
            return -1;
        status = linklistcom(input[1], &output);
    }
    else if(strcmp(input[0], "unlink") == 0) {
        if(inputc == 1)
            return -1;
        status = unlinkcom(input[1]);
        return status;
    }
    else if(strcmp(input[0], "rename") == 0) {
        if(inputc < 3)
            return -1;
        status = renamecom(input[1], input[2]);
        return status;
    }
    else if(strcmp(input[0], "remove") == 0) {
        if(inputc == 1)
            return -1;
        status = removecom(input[1]);
        return status;
    }
    else if(strcmp(input[0], "cpcat") == 0) {
        if(inpipe == true) {
            if (strcmp(input[1], "-") == 0) {
                status = cpcatcompipe(input[3], input[2], &output);
                goto END;
            }
            else {
                //check if the file exists
                int desk = open(input[1], O_RDONLY);
                if(desk < 0) {
                    status = cpcatcompipe(input[1], NULL, &output);
                    goto END;
                }
            }
        }
        
        if(redirected_in == true) {
            //check if the file exists
            int desk = open(input[1], O_RDONLY);
            if(desk < 0) {
                output = calloc(strlen(input[1])+2, sizeof(char));
                strcpy(output, input[1]);
                if(output[strlen(output)-1] != '\n')
                    strcat(output, "\n");
                goto END;
            }
        }

        if(inputc < 2)
            return -1;
        if(inputc == 2)
            status = cpcatcom(input[1], NULL, &output);
        else
            status = cpcatcom(input[1], input[2], &output);
    }
    else if(strcmp(input[0], "sysinfo") == 0) {
        status = sysinfocom(&output);
    }
    else if(strcmp(input[0], "shellinfo") == 0) {
        status = shellinfocom(&output);
    }
    else if(strcmp(input[0], "proc") == 0) {
        if(inputc == 1) {
            output = calloc(strlen(procfspath)+2, sizeof(char));
            strcpy(output, procfspath);
            (output)[strlen(procfspath)] = '\n';
            status = 0;
        }
        else
            status = proccom(input[1]);
    }
    else if(strcmp(input[0], "pids") == 0) {
        status = pidscom(&output);
    }
    else if(strcmp(input[0], "pinfo") == 0) {
        status = pinfocom(&output);
    }
    else if(strcmp(input[0], "waitone") == 0) {
        if(inputc == 1)
            status = waitonecom(NULL);
        else
            status = waitonecom(input[1]);
    }
    else if(strcmp(input[0], "waitall") == 0) {
        status = waitallcom();
    }
    else if(strcmp(input[0], "pipes") == 0) {
        char* nextcmdinput;
        if(redirected_in == true) {
            nextcmdinput = calloc(strlen(input[inputc-1])+1, sizeof(char));
            strcpy(nextcmdinput, input[inputc-1]);
            inputc--;
        } else {
            nextcmdinput = calloc(2, sizeof(char));
        }

        for(int j = 1; j < inputc; j++) {
            fflush(stdout);

            char* command = input[j];

            char** pipeinputtmp;
            int pipeinputctmp = split(command, &pipeinputtmp);

            if(isBuiltin(pipeinputtmp[0]) == true || strlen(nextcmdinput) == 0) {
                //add the previos comm output
                char** pipeinput;
                int pipeinputc;
                if(strlen(nextcmdinput) == 0) {
                    pipeinput = malloc(pipeinputctmp*sizeof(char*));
                    pipeinputc = pipeinputctmp;
                } else {
                    pipeinput = malloc((pipeinputctmp+1)*sizeof(char*));
                    pipeinputc = pipeinputctmp+1;
                }
                for(int i = 0; i < pipeinputctmp; i++) {
                    pipeinput[i] = calloc(strlen(pipeinputtmp[i])+1, sizeof(char));
                    strcpy(pipeinput[i], pipeinputtmp[i]);
                }
                if(strlen(nextcmdinput) != 0) {
                    pipeinput[pipeinputctmp] = calloc(strlen(nextcmdinput)+1, sizeof(char));
                    strcpy(pipeinput[pipeinputctmp], nextcmdinput);
                }

                //clean
                for(int i = 0; i < pipeinputctmp; i++)
                    free(pipeinputtmp[i]);
                free(pipeinputtmp);

                //create pipes
                int pipedesc[2];
                if(pipe(pipedesc) < 0) {
                    int err = errno;
                    printf("pipes: %s", strerror(err));
                    fflush(stdout);
                    status = err;
                    goto PIPESCLEANUP;   
                }

                //redirect stdout to the new pipe
                int original_stdout = dup(1);
                dup2(pipedesc[1], 1);

                //evaluate the command
                inpipe = true;
                status = evaluate(pipeinputc, pipeinput);
                inpipe = false;
                printf("\n");
                fflush(stdout);

                //return our stdout
                dup2(original_stdout, 1);

                //read the output
                char* pipeoutput = calloc(MAX_PIPE_OUTPUT+1, sizeof(char));
                if(read(pipedesc[0], pipeoutput, MAX_PIPE_OUTPUT) < 0) {
                    int err = errno;
                    printf("pipes: %s\n", strerror(err));
                    fflush(stdout);
                    free(pipeoutput);
                    goto PIPESCLEANUP;  
                }
                pipeoutput[strlen(pipeoutput)-1] = '\0';

                //close pipes
                close(pipedesc[0]);
                close(pipedesc[1]);

                if(strlen(pipeoutput) > 1 || pipeoutput[0] != '\n') {
                    free(nextcmdinput);
                    nextcmdinput = calloc(strlen(pipeoutput)+1, sizeof(char));
                    strcpy(nextcmdinput, pipeoutput);
                } else {
                    free(nextcmdinput);
                    nextcmdinput = calloc(2, sizeof(char));
                }

                free(pipeoutput);
                PIPESCLEANUP:
                for(int i = 0; i < pipeinputc; i++) {
                    free(pipeinput[i]);
                }
                free(pipeinput);
            } else {
                //create pipes
                int pipedesc[2];
                int pipedesc2[2];
                if(pipe(pipedesc) < 0) {
                    int err = errno;
                    printf("pipes: %s", strerror(err));
                    fflush(stdout);
                    status = err;
                    return err; 
                }
                if(pipe(pipedesc2) < 0) {
                    int err = errno;
                    printf("pipes: %s", strerror(err));
                    fflush(stdout);
                    status = err;
                    return err; 
                }

                int forkpid = fork();
                if(forkpid < 0) {
                    exit(-1);
                } else if(forkpid == 0) {
                    dup2(pipedesc[0], 0);
                    close(pipedesc[0]);
                    close(pipedesc[1]);

                    dup2(pipedesc2[1], 1);
                    close(pipedesc2[0]);
                    close(pipedesc2[1]);

                    char** execargs = malloc((pipeinputctmp+1)*sizeof(char*));
                    for(int i = 0; i < pipeinputctmp; i++) {
                        execargs[i] = calloc(strlen(pipeinputtmp[i])+1, sizeof(char));
                        strcpy(execargs[i], pipeinputtmp[i]);
                    }
                    execargs[pipeinputctmp] = NULL;

                    //run the external program
                    if(execvp(pipeinputtmp[0], execargs) < 0) {
                        int err = errno;
                        printf("pipes: %s\n", strerror(err));
                        fflush(stdout);
                        status = err;
                        exit(err);
                    }

                    exit(0);
                } else {
                    write(pipedesc[1], nextcmdinput, strlen(nextcmdinput));
                    close(pipedesc[0]);
                    close(pipedesc[1]);

                    if(waitpid(forkpid, &status, 0) < 0) { //wait for the child to finish executing
                        int err = errno;
                        printf("pipes: %s\n", strerror(err));
                        fflush(stdout);
                        return err;
                    }

                    char* pipeoutput = calloc(MAX_PIPE_OUTPUT+1, sizeof(char));
                    read(pipedesc2[0], pipeoutput, MAX_PIPE_OUTPUT);
                    close(pipedesc2[0]);
                    close(pipedesc2[1]);

                    free(nextcmdinput);
                    nextcmdinput = calloc(strlen(pipeoutput)+1, sizeof(char));
                    strcpy(nextcmdinput, pipeoutput);
                    free(pipeoutput);

                    //clean
                    for(int i = 0; i < pipeinputctmp; i++)
                        free(pipeinputtmp[i]);
                    free(pipeinputtmp);
                }
            }
        }
        output = calloc(strlen(nextcmdinput)+1, sizeof(char));
        strcpy(output, nextcmdinput);
        free(nextcmdinput);
    }
    else if(strcmp(input[0], "trap") == 0) {
        if(inputc == 1)
            status = printtrapscom(&output);
        else if(inputc == 2) {
            status = deltrapcom(input[1]);
            printtrapscom(&output);
        }
        else if(inputc == 3)
            status = addtrapcom(input[1], input[2]);
    }
    //external program
    else {
        if(!isBuiltin(input[0]) && redirected_in == true) {
            //simulate pipes
            char** pipeargs = malloc(3*sizeof(char*));
            pipeargs[0] = calloc(6, sizeof(char));
            strcpy(pipeargs[0], "pipes");

            pipeargs[1] = calloc(strlen(inpath)+7, sizeof(char));
            strcat(pipeargs[1], "cpcat ");
            strcat(pipeargs[1], inpath);

            int allargslen = 0;
            for(int i = 0; i < inputc-1; i++)
                allargslen += strlen(input[i]);
            allargslen += inputc-2;

            pipeargs[2] = calloc(allargslen+1, sizeof(char));
            for(int i = 0; i < inputc-1; i++) {
                if(i > 0)
                    strcat(pipeargs[2], " ");
                strcat(pipeargs[2], input[i]);
            }

            //piping
            int original_stdout = dup(1);
            dup2(outdesc, 1);

            status = evaluate(3, pipeargs);

            dup2(original_stdout, 1);

            for(int i = 0; i < 3; i++) {
                free(pipeargs[i]);
            }
            free(pipeargs);

            goto END;
        }

        int forkpid = fork();
        if(forkpid < 0)
            return forkpid;
        else if(forkpid == 0) { //child
            //make a new array with args
            char** execargs = malloc((inputc+1) * sizeof(char*));
            for(int i = 0; i < inputc; i++) {
                execargs[i] = calloc(strlen(input[i])+1, sizeof(char));
                strcpy(execargs[i], input[i]);
            }
            execargs[inputc] = NULL;

            //piping
            dup2(outdesc, 1);

            int pipetmp[2];
            if(pipe(pipetmp) < 0) {
                int err = errno;
                printf("pipes: %s", strerror(err));
                fflush(stdout);
                status = err;
                return err; 
            }
            dup2(pipetmp[1], 0);
            close(pipetmp[0]);
            close(pipetmp[1]);

            //run the external program
            if(execvp(input[0], execargs) < 0) {
                int err = errno;
                printf("%s: %s\n", input[0], strerror(err));
                fflush(stdout);
                status = err;
                goto CLEANUP;
            }

            CLEANUP:
            for(int i = 0; i < inputc; i++)
                free(execargs[i]);
            free(execargs);

            exit(status);

        } else { //parent
            if(waitpid(forkpid, &status, 0) < 0) { //wait for the child to finish executing
                int err = errno;
                printf("%s: %s\n", input[0], strerror(err));
                fflush(stdout);
                return err;
            }
        }
    }

    END:
    if(redirected_in == true) {
        for(int i = 0; i < inputc; i++) {
            free(input[i]);
        }
        free(input);
    }

    if(output != NULL) {
        write(outdesc, output, strlen(output));
        fflush(stdout);
        free(output);
    }
    return status;
}

void rmcomments(char* in) {
    for(int i = 0; i < strlen(in); i++) {
        if(in[i] == '#') {
            in[i] = '\0';
            return;
        }
    }
}

int main(int argc, char* argv[]) {
    signal(SIGCHLD, clearZombiessig);

    //create traps array
    globaltraps = malloc(MAX_TRAPS * sizeof(char*));
    globaltrapids = malloc(MAX_TRAPS * sizeof(int));
    for(int i = 0; i < MAX_TRAPS; i++)
        globaltraps[i] = calloc(MAX_TRAP_COMM_LENGTH, sizeof(char));

    strcpy(shellname, "mysh");

    //interactive and non-interactive
    if(isatty(0) == 1) {
        printf("%s> ", shellname);
        fflush(stdout);

        while(1) {
            clearZombies();
            char line[100*100] = { 0 };

            //line available
            if(fgets(line, sizeof(line), stdin)) {
                fflush(stdin); //flush
                if(strlen(line) == 0 || line[0] == '#' || (line[0] == '\n' && strlen(line) == 1)) {
                    printf("%s> ", shellname);
                    fflush(stdout);
                    continue;
                }

                rmcomments(line);

                bool justspaces = true;
                for(int i = 0; i < strlen(line); i++) {
                    if(line[i] != ' ') {
                        justspaces = false;
                        break;
                    }
                }
                if(justspaces == true)
                    continue;

                if(line[strlen(line)-1] == '\n')
                    line[strlen(line)-1] = '\0';

                //split to tokens
                char** tokens;
                int n = split(line, &tokens);
                if(tokens[0][0] == '#' || strcmp(tokens[0], "\n") == 0) {
                    printf("%s> ", shellname);
                    fflush(stdout);
                    continue; 
                }

                //check if it should be run in the background
                bool run_background = false;
                int forkpid = -1;
                if(strcmp(tokens[n-1], "&") == 0) {
                    run_background = true;
                    n--;
                    forkpid = fork();
                    if(forkpid < 0)
                        laststatus = forkpid;
                }
                                                                         //child
                if(run_background == false || (run_background == true && forkpid == 0)) {
                    //evaluate
                    laststatus = evaluate(n, tokens);
                } else {
                    laststatus = 0;
                }
                
                //free up tokens mem
                for(int i = 0; i < n; i++) {
                    free(tokens[i]);
                }
                free(tokens);

                //child ran in the background
                if(run_background == true && forkpid == 0)
                    exit(laststatus);

                printf("%s> ", shellname);
                fflush(stdout);
            }
        }
        return 0;
    } else {
        // printf("%s> ", shellname);

        char line[100*100] = { 0 };
        while(fgets(line, sizeof(line), stdin)) {
            if(strlen(line) == 0 || line[0] == '#' || (line[0] == '\n' && strlen(line) == 1)) {
                // printf("%s> ", shellname);
                continue;
            }
            rmcomments(line);

            bool justspaces = true;
            for(int i = 0; i < strlen(line); i++) {
                if(line[i] != ' ') {
                    justspaces = false;
                    break;
                }
            }
            if(justspaces == true)
                continue;

            if(line[strlen(line)-1] == '\n')
                line[strlen(line)-1] = '\0';
            
            //split to tokens
            char** tokens;
            int n = split(line, &tokens);
            if(tokens[0][0] == '#' || strcmp(tokens[0], "\n") == 0) {
                // printf("%s> ", shellname);
                continue; 
            }

            //check if it should be run in the background
            bool run_background = false;
            int forkpid = -1;
            if(strcmp(tokens[n-1], "&") == 0) {
                run_background = true;
                n--;
                forkpid = fork();
                if(forkpid < 0)
                    laststatus = forkpid;
                else if(forkpid == 0) {
                    int pipedesc2[2];
                    if(pipe(pipedesc2) < 0) {
                    }
                    dup2(pipedesc2[1], 0);
                    close(pipedesc2[0]);
                    close(pipedesc2[1]);
                }
            }
                                                                        //child
            if(run_background == false || (run_background == true && forkpid == 0)) {
                //evaluate
                laststatus = evaluate(n, tokens);
                if(forkpid == 0)
                    exit(laststatus);
            } else {
                laststatus = 0;
            }
            
            //free up tokens mem
            for(int i = 0; i < n; i++) {
                free(tokens[i]);
            }
            free(tokens);

            //child ran in the background
            if(run_background == true && forkpid == 0)
                exit(laststatus);
            
            // printf("%s> ", shellname);
        }
    }
    return 0;
}
