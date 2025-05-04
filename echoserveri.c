/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"           // 소켓 프로그래밍과 Robust I/O 함수들이 정의된 헤더 포함

void echo(int connfd);       // 클라이언트와 데이터를 주고받는 echo 함수 선언

int main(int argc, char **argv)  
{
    int listenfd, connfd;                 // listenfd: 서버 리슨 소켓, connfd: 연결 소켓
    socklen_t clientlen;                  // 클라이언트 주소 구조체의 길이
    struct sockaddr_storage clientaddr;   // 어떤 주소든 저장할 수 있는 구조체 (IPv4, IPv6 모두 지원) //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];    // 클라이언트의 호스트이름과 포트 번호 저장 버퍼

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);    // 포트번호 인자가 없을 경우 사용법 출력 후 종료
	exit(0);
    }

    listenfd = Open_listenfd(argv[1]);   // 지정된 포트에서 연결을 기다리는 리슨 소켓 생성
    while (1) {                          // 무한 루프: 클라이언트의 연결을 계속 수락
	clientlen = sizeof(struct sockaddr_storage); 
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트 연결 수락 accept

        // 연결된 클라이언트의 호스트 이름과 포트 번호를 문자열로 얻어옴
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);

        // 연결된 클라이언트의 정보 출력
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
	echo(connfd);    // 클라이언트로부터 데이터를 받아 다시 보내는 에코 함수 실행
	Close(connfd);   // 클라이언트와의 연결 종료
    }
    exit(0);
}
/* $end echoserverimain */

// 'gcc -o echoserveri echoserveri.c echo.c csapp.c -lpthread' 로 컴파일 실행
// './echoserveri 포트번호' 로 서버 실행
// 새 SSH 열어서 'telnet 127.0.0.1 12345' 으로 접속 후 결과 확인