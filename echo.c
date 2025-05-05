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
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {   // 읽을 내용이 있으면 반복   //line:netp:echo:eof
        // for (size_t i = 0; i < n; i++) {                                                     // hi를 치면 4바이트가 찍힌다. 왜 그럴까? 나는 해당 echo 서버를 telnet으로 돌렸기 때문이다.
        //     printf("buf[%zu] = 0x%x (%c)\n", i, buf[i], isprint(buf[i]) ? buf[i] : '.');     // Carriage Return과 Line Feed가 들어간 hi\r\n 식으로 출력된다. 해당 코드로 바이트수가 2많음을 확인할 수 있다.
        // }
	printf("server received %d bytes\n", (int)n);           // 서버에서 받은 바이트 수 출력
	Rio_writen(connfd, buf, n);                             // 받은 내용을 그대로 클라이언트에 전송 (에코)
    }
}
/* $end echo */