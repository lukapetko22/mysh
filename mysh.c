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

#define MAX_NUMBER_OF_ARGS 100
#define MAX_LEN_OF_ARG 1000

/*
    Global vars
*/
char shellname[100] = { 0 };
int laststatus = 0;



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
        items[i] = calloc(MAX_LEN_OF_ARG, sizeof(char));
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
        (*output)[i] = calloc(strlen(items[i]), sizeof(char));
        strcpy((*output)[i], items[i]);
        free(items[i]);
    }
    free(items);

    return lineindex;
}


//////////////////////////////////////
//    PREPROSTI VGRAJENI UKAZI      //
//////////////////////////////////////
int helpcom(char** output) {
    char helpmsg[] = "This is the help menu\n";
    *output = calloc(strlen(helpmsg), sizeof(char));
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

    *output = calloc(len, sizeof(char));
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
        return err;
    }

    return 0;
}

int dirwherecom(char** output) {
    char buffer[1000] = { 0 };
    if(getcwd(buffer, sizeof(buffer)) == NULL) {
        int err = errno;
        printf("dirwhere: %s\n", strerror(err));
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
        return err;
    }
    return 0;
}

int dirremovecom(char* target) {
    if(rmdir(target) < 0) {
        int err = errno;
        printf("dirremove: %s\n", strerror(err));
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
        return err; 
    }
    return 0;
}

int linksoftcom(char* target, char* name) {
    if(symlink(target, name) < 0) {
        int err = errno;
        printf("linksoft: %s\n", strerror(err));
        return err; 
    }
    return 0;
}

int linkreadcom(char* target, char** output) {
    char buffer[100] = { 0 };
    if(readlink(target, buffer, sizeof(buffer)) < 0) {
        int err = errno;
        printf("linkread: %s\n", strerror(err));
        return err; 
    }
    
    *output = calloc(strlen(buffer)+2, sizeof(char));
    strcpy(*output, buffer);
    (*output)[strlen(buffer)] = '\n';
    (*output)[strlen(buffer)+1] = '\0';
    return 0;
}

int unlinkcom(char* target) {
    if(remove(target) < 0) {
        int err = errno;
        printf("unlink: %s\n", strerror(err));
        return err;  
    }
    return 0;
}

int renamecom(char* oldpath, char* newpath) {
    if(rename(oldpath, newpath) < 0) {
        int err = errno;
        printf("rename: %s\n", strerror(err));
        return err;  
    }
    return 0;
}

int removecom(char* target) {
    if(remove(target) < 0) {
        int err = errno;
        printf("remove: %s\n", strerror(err));
        return err;  
    }
    return 0;
}



int evaluate(int argc, char** args) {
    char* output = NULL; char** input; int inputc = 0; int status; int indesc; int outdesc;
    bool redirected_in = false; bool redirected_out = false; bool run_background = false; char* inpath; char* outpath;

    if(argc == 0) {
        return -1;
    }

    /*
        Check for redirections and if we should run it in the background
    */
    //bg
    if(strcmp(args[argc-1], "&") == 0) {
        run_background = true;
        argc--;
    }
    //redirected out
    if(args[argc-1][0] == '>') {
        redirected_out = true;

        outpath = calloc(strlen(args[argc-1]), sizeof(char));
        for(int i = 1; i < strlen(args[argc-1]); i++)
            outpath[i-1] = args[argc-1][i];
        outdesc = creat(outpath, O_WRONLY);
        if(outdesc < 0) {
            int err = errno;
            printf("%s\n", strerror(err));
            return err;
        }
        chmod(outpath, 0777);
        int err = errno;
        if(err < 0) {
            printf("%s\n", strerror(err));
            return err;
        }

        argc--;
    } else {
        outdesc = 1;
    }
    //redirected in
    if(args[argc-1][0] == '<') {
        redirected_in = true;

        inpath = calloc(strlen(args[argc-1]), sizeof(char));
        for(int i = 1; i < strlen(args[argc-1]); i++)
            inpath[i-1] = args[argc-1][i];
        indesc = open(inpath, O_RDONLY);
        if(indesc < 0) {
            int err = errno;
            printf("%s\n", strerror(err));
            return err;
        }

        char line[MAX_NUMBER_OF_ARGS*MAX_LEN_OF_ARG];
        char fromfile[MAX_NUMBER_OF_ARGS*MAX_LEN_OF_ARG];
        read(indesc, fromfile, sizeof(fromfile));
        strcat(line, args[0]);
        strcat(line, " ");
        strcat(line, fromfile);
        inputc = split(line, &input);

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
        status = pidcom(&output);
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

    else {
        printf("idk komandu\n");
        return -1;
    }

    if(output != NULL) {
        write(outdesc, output, strlen(output));
        free(output);
    }
    return status;
}

int main(int argc, char* argv[]) {
    strcpy(shellname, "mysh");

    //interactive and non-interactive
    if(isatty(0) == 1) {
        printf("%s> ", shellname);

        while(1) {
            char line[MAX_NUMBER_OF_ARGS*MAX_LEN_OF_ARG] = { 0 };
            //line available
            if(fgets(line, sizeof(line), stdin)) {
                fflush(stdin); //flush
                if(strlen(line) == 0 || line[0] == '#' || (line[0] == '\n' && strlen(line) == 1)) {
                    printf("%s> ", shellname);
                    continue;
                }
                if(line[strlen(line)-1] == '\n')
                    line[strlen(line)-1] = '\0';

                //split to tokens
                char** tokens;
                int n = split(line, &tokens);
                if(tokens[0][0] == '#' || strcmp(tokens[0], "\n") == 0) {
                    printf("%s> ", shellname);
                    continue; 
                }


                //evaluate
                laststatus = evaluate(n, tokens);



                printf("%s> ", shellname);
            }
            sleep(1);
        }
        return 0;
    }




    return 0;
}
