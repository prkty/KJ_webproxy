// 클라이언트 파일
#include "csapp.h"    // Robust I/O와 소켓 함수가 정의된 헤더 파일 포함

int main(int argc, char **argv) 
{
    int clientfd;                     // 서버와 연결된 소켓 파일 디스크립터
    char *host, *port, buf[MAXLINE];  // 호스트 이름, 포트 번호, 입출력 버퍼
    rio_t rio;                        // robust I/O를 위한 rio 버퍼 구조체

    if (argc != 3) {  // 명령행 인자의 수가 올바르지 않으면
	fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
	exit(0);          // 사용법 출력 후 종료
    }
    host = argv[1];  // 첫 번째 인자: 서버 호스트 이름
    port = argv[2];  // 두 번째 인자: 서버 포트 번호

    clientfd = Open_clientfd(host, port);  // 서버에 연결하고 소켓 fd를 받아옴
    Rio_readinitb(&rio, clientfd);         // rio 버퍼를 소켓과 연결하여 초기화

    // 표준 입력에서 한 줄씩 입력을 받아 서버로 전송하고, 서버 응답을 출력
    while (Fgets(buf, MAXLINE, stdin) != NULL) {   // 표준 입력에서 문자열 읽기
	Rio_writen(clientfd, buf, strlen(buf));        // 입력 문자열을 서버로 전송
	Rio_readlineb(&rio, buf, MAXLINE);             // 서버가 echo한 문자열을 수신
	Fputs(buf, stdout);                            // 수신한 문자열을 화면에 출력
    }
    Close(clientfd);  // 연결 종료 (소켓 닫기)  //line:netp:echoclient:close
    exit(0);          // 정상 종료
}
/* $end echoclientmain */
