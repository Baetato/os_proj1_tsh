/*
 * Copyright(c) 2023-2024 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 2024.04.04, ICT융합학부, 2022088031, 배수연, 표준 입출력 리다이렉션과 파이프 기능을 추가했습니다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80             /* 명령어의 최대 길이 */

/*
 * cmdexec - 명령어를 파싱해서 실행한다.
 * 스페이스와 탭을 공백문자로 간주하고, 연속된 공백문자는 하나의 공백문자로 축소한다.
 * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
 * 기호 '<' 또는 '>'를 사용하여 표준 입출력을 파일로 바꾸거나,
 * 기호 '|'를 사용하여 파이프 명령을 실행하는 것도 여기에서 처리한다.
 */

static void cmdexec(char *cmd)
{
    char *argv[MAX_LINE/2+1];   /* 명령어 인자를 저장하기 위한 배열 */
    int argc = 0;               /* 인자의 개수 */
    char *p, *q;                /* 명령어를 파싱하기 위한 포인터 변수 */
    pid_t pid;                  //프로세스 ID를 저장하기 위한 변수
    
    /*
     * 명령어 앞부분 공백문자를 제거하고 인자를 하나씩 꺼내서 argv에 차례로 저장한다.
     * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
     */
    p = cmd; p += strspn(p, " \t"); //명령어 문자열 앞부분의 공백 제거
    
    do {
        
        /*
         * 공백문자, 큰 따옴표, 작은 따옴표가 있는지 검사한다.
         */
        q = strpbrk(p, " \t\'\"<>|");
        
        /*
         * 공백문자가 있거나 아무 것도 없으면 공백문자까지 또는 전체를 하나의 인자로 처리한다.
         */
        if (q == NULL || *q == ' ' || *q == '\t') {
            q = strsep(&p, " \t");
            if (*q) argv[argc++] = q;
        }
        
        //pipe
        else if (*q == '|'){
       
          q = strsep(&p, "|");
          int pipe_fd[2];
          argv[argc] = NULL;
          
          //예외처리
          if (pipe(pipe_fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
          }
          pid = fork(); // 자식 프로세스 생성
          
          //예외처리
          if (pid < 0){
              perror("fork");
              exit(EXIT_FAILURE);
          }
          //부모 프로세스에서 cmd 0 처리
          else if (pid > 0){
             close(pipe_fd[1]); //파이프의 쓰기 끝을 닫음
             dup2(pipe_fd[0], 0); //파이프의 읽기 끝을 표준 입력으로 연결
             close(pipe_fd[0]); //원본 파이프 읽기 끝을 닫음
             cmdexec(p); //재귀
             perror("execvp"); //execvp 호출 실패 시 예외 처리
             exit(EXIT_SUCCESS);
          
          }
          else{
             close(pipe_fd[0]); //파이프의 읽기 끝을 닫음
             dup2(pipe_fd[1], 1); //파이프의 쓰기 끝을 표준 출력으로 연결
             close(pipe_fd[1]); //원본 파이프 쓰기 끝 닫음
             execvp(argv[0], argv); //execvp 호출해 새 프로그램 실행
             perror("execvp"); //예외처리
             exit(EXIT_FAILURE);
          }
       }
        
        //input redirection
        else if (*q == '<'){
            //'<'를 기준으로 문자열 분리 후, q에는 '<'이전 내용, p에는 '>'이후 내용 저장
            q = strsep(&p, "<");
            //p가 가리키는 문자열에서 시작하는 공백과 탭을 건너뛰고 다음 위치를 p에 저장
            p += strspn(p, " \t");
            //p에서 공백이나 탭을 기준으로 다시 문자열 파싱 후, q에 파일 이름 저장
            q = strsep(&p," \t");
            //q에 저장된 파일 이름으로 파일을 읽기 전용으로 연다음 해당 파일 디스크립터를 input_fd에 저장
            int input_fd = open(q, O_RDONLY);
            //파일 열기 실패했을 경우의 예외처리
            if(input_fd == -1){
                perror("open");
                exit(EXIT_FAILURE);
            }
            // 열린 파일 디스크립터(input_fd)를 표준입력(STDIN_FILENO)에 연결
            dup2(input_fd, STDIN_FILENO);
            //복사 후 원본 파일 디스크립터(input_fd)를 닫음
            close(input_fd);
            //argv 배열의 마지막을 NULL로 설정하여 인자 목록의 끝을 표시해줌
            argv[argc] = NULL;
        }
        
        //output redirection
        else if (*q == '>'){
            //'>'을 기준으로 문자열 분리 후, q에는 '>'이전 내용, p에는 '>'이후 내용 저장
            q = strsep(&p, ">");
            //p가 가리키는 문자열에서 시작하는 공백이나 탭을 건너뛰고 다음 위치를 p에 저장
            p += strspn(p, " \t");
            //p에서 공백이나 탭을 기준으로 다시 문자열을 파싱 후, q에 파일 이름 저장
            q = strsep(&p," \t");
            //q에 저장된 파일 이름으로 파일을 쓰기 전용으로 열고, 파일이 없으면 생성, 파일이 있으면 내용을 비우고 (O_TRUNC), 모든 사용자가 읽고 쓸 수 있도록 권한 설정 (0666) !
            int output_fd = open(q, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            //파일 열기에 실패했을 경우의 예외처리
            if(output_fd == -1){
                perror("open");
                exit(EXIT_FAILURE);
            }
            //열린 파일 디스크립터(output_fd)를 표준출력(STDOUT_FILENO)에 연결
            dup2(output_fd, STDOUT_FILENO);
            //복사 후 원본 파일 디스크립터(output_fd)를 닫음
            close(output_fd);
            //argv 배열의 마지막을 NULL로 설정하여 인자 목록의 끝을 표시
            argv[argc] = NULL;
        }
        
        /*
         * 작은 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고,
         * 작은 따옴표 위치에서 두 번째 작은 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 작은 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        
        else if (*q == '\'') {
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\'");
            if (q && *q) argv[argc++] = q;
        }
        /*
         * 큰 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고,
         * 큰 따옴표 위치에서 두 번째 큰 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 큰 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else {
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
        }
    } while (p);
    argv[argc] = NULL;
    /*
     * 파싱되어 argv에 저장된 명령어를 실행한다.
     */
    if (argc > 0)
        execvp(argv[0], argv);
}

/*
 * 기능이 간단한 유닉스 셸인 tsh (tiny shell)의 메인 함수이다.
 * tsh은 프로세스 생성과 파이프를 통한 프로세스간 통신을 학습하기 위한 것으로
 * 백그라운드 실행, 파이프 명령, 표준 입출력 리다이렉션 일부만 지원한다.
 */
int main(void)
{
    char cmd[MAX_LINE+1];       /* 명령어를 저장하기 위한 버퍼 */
    int len;                    /* 입력된 명령어의 길이 */
    pid_t pid;                  /* 자식 프로세스 아이디 */
    bool background;            /* 백그라운드 실행 유무 */
    
    /*
     * 종료 명령인 "exit"이 입력될 때까지 루프를 무한 반복한다.
     */
    while (true) {
        /*
         * 좀비 (자식)프로세스가 있으면 제거한다.
         */
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            printf("[%d] + done\n", pid);
        /*
         * 셸 프롬프트를 출력한다. 지연 출력을 방지하기 위해 출력버퍼를 강제로 비운다.
         */
        printf("tsh> "); fflush(stdout);
        /*
         * 표준 입력장치로부터 최대 MAX_LINE까지 명령어를 입력 받는다.
         * 입력된 명령어 끝에 있는 새줄문자를 널문자로 바꿔 C 문자열로 만든다.
         * 입력된 값이 없으면 새 명령어를 받기 위해 루프의 처음으로 간다.
         */
        len = read(STDIN_FILENO, cmd, MAX_LINE);
        if (len == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        cmd[--len] = '\0';
        if (len == 0)
            continue;
        /*
         * 종료 명령이면 루프를 빠져나간다.
         */
        if(!strcasecmp(cmd, "exit"))
            break;
        /*
         * 백그라운드 명령인지 확인하고, '&' 기호를 삭제한다.
         */
        char *p = strchr(cmd, '&');
        if (p != NULL) {
            background = true;
            *p = '\0';
        }
        else
            background = false;
        /*
         * 자식 프로세스를 생성하여 입력된 명령어를 실행하게 한다.
         */
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        /*
         * 자식 프로세스는 명령어를 실행하고 종료한다.
         */
        else if (pid == 0) {
            cmdexec(cmd);
            exit(EXIT_SUCCESS);
        }
        /*
         * 포그라운드 실행이면 부모 프로세스는 자식이 끝날 때까지 기다린다.
         * 백그라운드 실행이면 기다리지 않고 다음 명령어를 입력받기 위해 루프의 처음으로 간다.
         */
        else if (!background)
            waitpid(pid, NULL, 0);
    }
    return 0;
}


