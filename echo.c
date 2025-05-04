/*
 * echo - read and echo text lines until client closes connection
 */
/* $begin echo */
#include "csapp.h"    // Robust I/O와 소켓 함수가 정의된 헤더 파일 포함

void echo(int connfd) 
{
    size_t n;             // 읽은 바이트 수를 저장할 변수
    char buf[MAXLINE];    // 데이터 버퍼
    rio_t rio;            // Robust I/O를 위한 구조체

    Rio_readinitb(&rio, connfd);   // Robust I/O으로 소켓 디스크럽터 초기화

    // 클라이언트가 보낸 메시지를 한 줄씩 읽어서 다시 클라이언트에 전송
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {   // 읽을 내용이 있으면 반복   //line:netp:echo:eof
	printf("server received %d bytes\n", (int)n);           // 서버에서 받은 바이트 수 출력
	Rio_writen(connfd, buf, n);                             // 받은 내용을 그대로 클라이언트에 전송 (에코)
    }
}
/* $end echo */