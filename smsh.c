#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

#define CMDLEN 64
#define INPLEN 512

char **commandArrayConstr(char *command, int *cntp, const char key);
void commandHandler(char *input);

int fileChecker(const char *name){
  FILE *file;
  if (file = fopen(name, "r")){
    fclose(file);
    return 1;
  }
  return 0;
}

void redirectionHandler(char* input, const char *key) {

  char *left, *right, *name, *tmp;
  int fd_r, fd_w;
  pid_t pid;

  left = strtok(input, key);
  right = strtok(NULL, key);
  name = strtok(right, " ");

  if ((pid = fork()) < 0) {
    perror("in redirectionHandler: fork");
     abort();
  } else if (pid == 0) {
    switch (key[0]){
    case '>':
      if (key[1] == '>') {
        if ((fd_w = open(name, O_RDWR | O_CREAT, 0644)) < 0) {
          printf("'>>' routine in redirectionHandler: open");
          exit(0);
        }
        lseek(fd_w, 0, SEEK_END);
        dup2(fd_w, STDOUT_FILENO);
        close(fd_w);
        break;
      }
      if (key[1] == '|' && fileChecker(name)) {
        printf("'>|' routine in redirectionHandler: file already exists");
        exit(0);
      }
      if ((fd_w = creat(name, 0644)) < 0) {
        printf("'>' routine in redirectionHandler: open");
        exit(0);
      }
      lseek(fd_w, 0, SEEK_END);
      dup2(fd_w, STDOUT_FILENO);
      close(fd_w);
      break;
    case '<':
      if ((fd_r = open(name, O_RDONLY, 0)) < 0) {
        printf("'<' routine in redirectionHandler: open");
        exit(0);
      }
      lseek(fd_r, 0, SEEK_SET);
      dup2(fd_r, STDIN_FILENO);
      close(fd_r); 
      break;
    default:
      break;
    }
    commandHandler(left);
    exit(0);
  } else {
    wait(NULL);
    exit(0);
  }
  return;
}

void history(){
  int c;
  FILE *file;
  if ((file = fopen("history", "r")) < 0) {
    printf("in history: open");
    exit(1);
  }
  if (file) {
    while ((c = getc(file)) != EOF)
        putchar(c);
    fclose(file);
  }
}

char **parentheHandler(char *input) {
  char **tcmd;
  char **commands;
  char *tmp = strstr(input, ")");
  char *eof = strstr(input, "\0");
  char *op = malloc(CMDLEN);
  char *body = malloc(CMDLEN);
  int n, i, j;
  
  if ((n = snprintf(op, eof-tmp, "%s", tmp+1)) < 0) {
    printf("in parentheHandler: snprintf");
    exit(1);
  }

  tmp = strstr(input, "(");
  eof = strstr(input, ")");
  n = snprintf(body, eof-tmp, "%s", tmp+1);
  n = 1;
  tcmd = commandArrayConstr(body, &n, ';');
  commands = malloc(sizeof(char**) * (n+1));
  for (i = 0; i < n; i++){
    commands[i] = malloc(sizeof(char*) * CMDLEN);
    if ((j = sprintf(commands[i], "%s %s", tcmd[i], op)) < 0) {
      printf("in parentheHandler: sprintf");
      exit(1);
    }
  }
  commands[i] = NULL;

  return commands;
}

char **colonParentheParser(char *command, int *cntp) {

  char **commands;
  int marker = 0;
  int i, n, tmp, flag = 0;

  for (marker = 0; command[marker] != '\0'; marker++){
    if (command[marker] == '(') flag = 1;
    if (command[marker] == ')') flag = 0;
    if (command[marker] == ';' && flag == 0)
      (*cntp)++;
  }
  commands = malloc(sizeof(char*) * (*cntp)+1);

  for (i = 0; i < (*cntp); i++)
    commands[i] = malloc(sizeof(char) * CMDLEN);

  marker = 0;
  n = 0;
  i = 0;
  while (1) {
    if (command[marker] == '(') {
      if (flag) {
        printf("in colonParentheParser: parenthesis unmatch");
        exit(1);
      }
      flag = 1;
    }
    if (command[marker] == ')') {
      if (!flag) {
        printf("in colonParentheParser: parenthesis unmatch");
        exit(1);
      }
      flag = 0;
    }
    if (command[marker] == ';') {
      if (!flag) {
        if ((tmp = snprintf(commands[i], marker-n+1, "%s", &(command[n]))) < 0) {
          printf("colon routine in colonParentheParser: snprintf");
          exit(1);
        }
        n = marker+1;
        i++;
      }
    }
    if (command[marker] == '\0') {
      if (flag) {
        printf("in colonParentheParser: parenthesis unmatch");
        exit(1);
      }
      if ((tmp = snprintf(commands[i++], marker-n+1, "%s", &(command[n]))) < 0) {
        printf("eof routine in colonParentheParser: eof snprintf");
        exit(1);
      }
      break;
    }
    marker++;
  }
  commands[i] = NULL;
  return commands;
}

char ***pipeSeperator(char *input) {
  int n, i = 1, j = 1;
  char **split = commandArrayConstr(input, &i, '|');
  char ***cmds = malloc(sizeof(char**) * (i+1));
  n = 0;
  while(i > n) {
    cmds[n] = commandArrayConstr(split[n], &j, ' ');
    j = 1;
    n++;
  }
  cmds[n] = NULL;
  return cmds;
}

void pipeHandler(char ***input) {
  int pfds[2], p_in = 0;
  pid_t pid;
  
  while(*input != NULL) {
     pipe(pfds);
     if ((pid = fork()) < 0){
      printf("in pipeHandler: fork");
      exit(1);
    } else if (pid == 0) {
      dup2(p_in, 0);
      if (*(input + 1) != NULL) {
        dup2(pfds[1], 1);
      }
      close(pfds[0]);
      execvp((*input)[0], (*input));
      exit(0);
    } else {
      wait(NULL);
      close(pfds[1]);
      p_in = pfds[0];
      input++;
    }
  }
  return;
}

char **commandArrayConstr(char *command, int *cntp, const char key) {

  char **commands;
  char *tmp;
  int i, j = 0, k = 0, flag = 0;

  for (i = 0; command[i] != '\0'; i++){
    if (command[i] == key)
      (*cntp)++;
  }
  if (key == ' ') (*cntp)++;
  commands = malloc(sizeof(char*) * (*cntp));

  for (i = 0; i < (*cntp); i++) 
    commands[i] = malloc(sizeof(char) * CMDLEN);

  commands[0] = strtok(command, &key);
  i = 1;
  while ((tmp = strtok(NULL, &key)) != NULL) {
    if (strcmp(tmp, "~") == 0) commands[i++] = getenv("HOME");
    else commands[i++] = tmp;
  }
  if (key == ' ') commands[i] = NULL;
  return commands;
}

void helper() {
  printf("rE4l MaN nEeD5 nO h3LP [B]OI\n");
}

void commandHandler(char *input) {

  int n, i = 1, j = 1;
  int flag = 1;
  char *command = input;
  char *tmp;
  char **cve;

  flag = 1;
  command = strtok(input, "&");
  if ((tmp = strstr(command, "|")) != NULL) {
    char ***pipeCommand = pipeSeperator(command);
    pipeHandler(pipeCommand);
    exit(1);
  } else {
    cve = commandArrayConstr(command, &flag, ' ');
    if (cve[0][0] == 'c' && cve[0][1] == 'd'){
      chdir(cve[1]);
      exit(1);
    }
    else execvp(cve[0], cve);
    exit(1);
  }

  return;
}

int main() {

  int length;
  char* command = malloc(sizeof(char)* INPLEN);
  char* tmp;
  char chk[16];
  int t_count = 1;
  pid_t * pids;
  char ** commands;
  char *** pipeCommand;
  int i, n;
  int status;
  pid_t pid, wpid;
  int fd;

  if ((fd = open("history", O_CREAT | O_RDWR, 0644)) < 0) {
    printf("in main: opening history");
    exit(1);
  }

  while (1) {
    
    t_count = 1;
    printf("> ");

    memset(command, 0, INPLEN);
    if ((fgets(command, INPLEN, stdin)) == NULL) {
      printf("in main: fgets input\n");
      exit(1);
    }

    length = strlen(command) - 1;
    command[length] = '\n';

    if ((n = write(fd, command, length+1)) < 0){
      printf("in main: writing history\n");
      exit(1);
    }
    lseek(fd, 0, SEEK_END);

    command[length] = '\0';

    commands = colonParentheParser(command, &t_count);

    pids = malloc(sizeof(pid_t) * t_count);

    for (i = 0; i < t_count; ++i) {
      if ((tmp = strstr(commands[i], "(")) != NULL) {
        char **cmdGrp = parentheHandler(commands[i]);
        while(*cmdGrp != NULL) {
          if ((n = sprintf(chk, "%s", *cmdGrp)) < 0) {
            printf("parenthesis routine in main: sprintf");
            exit(1);
          }
          tmp = strtok(chk, " ");
          if (strcmp(tmp, "history") == 0){
            history();
            continue;
          }
          if (strcmp(tmp, "exit") == 0)
            return 0;
          if (strcmp(tmp, "help") == 0) {
            helper();
            continue;
          }
          if ((pids[i] = fork()) < 0) {
            printf("parenthesis routine in main: fork");
            exit(1);
          } else if (pids[i] == 0) {
            if ((tmp = strstr(*cmdGrp, ">>")) != NULL) redirectionHandler(*cmdGrp, ">>");
            else if ((tmp = strstr(*cmdGrp, ">|")) != NULL) redirectionHandler(*cmdGrp, ">|");
            else if ((tmp = strstr(*cmdGrp, ">")) != NULL) redirectionHandler(*cmdGrp, ">>");
            else if ((tmp = strstr(*cmdGrp, "<")) != NULL) redirectionHandler(*cmdGrp, "<");
            else commandHandler(*cmdGrp);
          }
          else {
            if ((tmp = strstr(*cmdGrp, "&")) != NULL) {
              printf("+[%d]\n", pids[i]);
              continue;
            }
            wait(NULL);
          }
          cmdGrp++; 
        }
      }
      else {
        if ((n = sprintf(chk, "%s", commands[i])) < 0) {
          printf("regular routine in main: sprintf");
          exit(1);
        }
        tmp = strtok(chk, " ");
        if (strcmp(tmp, "history") == 0){
          history();
          continue;
        }
        if (strcmp(tmp, "exit") == 0)
          return 0;
        if (strcmp(tmp, "help") == 0) {
          helper();
          continue;
        }
        if ((pids[i] = fork()) < 0) {
          printf("regular routine in main: fork");
          exit(1);
        } else if (pids[i] == 0) {
          if ((tmp = strstr(commands[i], ">>")) != NULL) redirectionHandler(commands[i], ">>");
          else if ((tmp = strstr(commands[i], ">|")) != NULL) redirectionHandler(commands[i], ">|");
          else if ((tmp = strstr(commands[i], ">")) != NULL) redirectionHandler(commands[i], ">");
          else if ((tmp = strstr(commands[i], "<")) != NULL) redirectionHandler(commands[i], "<");
          else commandHandler(commands[i]);
        }
        else {
          if ((tmp = strstr(commands[i], "&")) != NULL) {
            printf("+[%d]\n", pids[i]);
            continue;
          }
          wait(NULL);
        }
      }
    }
  }

  close(fd); 

  return 0;
}