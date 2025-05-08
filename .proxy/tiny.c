// 들어가기 앞서 if문에서 0은 false이고 1은 true를 의미한다. 
// 해당 코드에서 함수 앞에 !을 적어 true를 false를 시키고 그걸 또 if문으로 비교하는 경우가 많으니 유념해서 해석하자.
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;                    // 소켓 파일 디스크립터들
  char hostname[MAXLINE], port[MAXLINE];   // 클라이언트의 호스트이름과 포트번호 저장용
  socklen_t clientlen;                     // 주소 길이
  struct sockaddr_storage clientaddr;      // 클라이언트 주소 정보 저장용 구조체

  // 포트 번호 인자가 제대로 들어왔는지 확인
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);  // 사용법 출력
    exit(1);                                         // 인자 부족 시 종료
  }

  // 인자로 받은 포트 번호로 듣기 소켓 오픈
  listenfd = Open_listenfd(argv[1]);

  // while문으로 서버 무한 루프 실행
  while (1) {
    // clientaddr 구조체 크기 설정
    clientlen = sizeof(clientaddr);

    // 클라이언트로부터 연결 요청을 수락하고 연결 소켓 생성
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // 클라이언트 정보(호스트명, 포트)를 문자열로 가져옴
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);

    // 연결된 클라이언트의 정보 출력
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    doit(connfd);    // 해당 연결에 대해 HTTP 처리 루틴 실행
    Close(connfd);   // 연결 종료
  }
}

/////////////////////////////////////////////////////////

void doit(int fd)
{
  int is_static;                            // 정적 페인지 여부를 저장(1: 정적, 0: 동적)
  struct stat sbuf;                         // 파일 정보 저장용 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];  // 요청 라인 저장
  char filename[MAXLINE], cgiargs[MAXLINE];  //파일 이름과 CGI 인자 저장
  rio_t rio;                                 // Rio I/O 구조체

  // 요청 헤더를 읽기 위한 초기화 및 요청 라인 읽기
  Rio_readinitb(&rio, fd);             // Rio I/O 초기화
  Rio_readlineb(&rio, buf, MAXLINE);   // 요청 라인 읽기 (ex. "GET /index.html HTTP/1.0")
  printf("Request headers:\n");        // 헤더 요청
  printf("%s", buf);                   // 요청 라인 출력(디버깅)

  // 요청 라인에서 메서드, URI, 버전 추출
  sscanf(buf, "%s %s %s", method, uri, version);

  // 지원하지 않는 메서드일 경우 501 에러 전송 후 종료
  if (strcasecmp(method, "GET")) {      // method가 GET 요청이 아닐때 if문 실행
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio);                          // 요청 헤더들을 모두 읽어 들임 (분석은 X)
  is_static = parse_uri(uri, filename, cgiargs);   // URI 분석을 통해 정적/동적 구분 및 파일 이름, CGI 인자 추출

  // 요청한 파일이 실제 존재하는지 확인
  if (stat(filename, &sbuf) < 0) {   // 함수를 뜯어보면 알겠지만, stat가 파일이 존재하지 않으면 -1을 내보내므로 0보다 작아 if문이 실행된다
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  // 정적 컨텐츠 처리
  if (is_static) {
    // 일반 파일이 아니거나 읽기 권한이 없을 경우 403 에러 리턴
    // S_ISREG나 S_IRUSR이 아닐경우 True를 내보내는데, !에 따라 false으로 if문을 연산한다.
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
  // 정상이면 정적 파일을 클라이언트에 전송
  serve_static(fd, filename, sbuf.st_size);
  } 

  // 동적 컨텐츠 처리(CGI)
  else {
    // 일반 파일이 아니거나 실행 권한이 없을 경우 403 에러 리턴
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 정상이면 CGI 프로그램 실행 및 결과 전송
    serve_dynamic(fd, filename, cgiargs);
  }
}


void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);      // 첫번째 요청 헤더 줄을 읽음

  // 빈 줄("\r\n")을 만날 때까지 요청 헤더를 계속 읽어들임
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);  // 다음 줄 읽기
    printf("%s", buf);                // 읽은 헤더 줄 출력
  }
  // 헤더 끝 (빈 줄)을 만나면 함수 종료
  return;
}


int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // "cgi-bin"이 포함되어 있지 않으면 정적 콘텐츠 요청으로 판단
  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");             // CGI 인자는 없음
    strcpy(filename, ".");           // 상대 경로 시작
    strcat(filename, uri);           // uri를 파일명으로 추가
    if (uri[strlen(uri)-1] == '/')   // URI가 '/'로 끝나면
      strcat(filename, "home.html"); // 기본 파일로 home.html 지정
    
    return 1;                        // 정적 콘텐추임을 의미한다
  } 

  else {
    // 동적 콘텐츠 요청 (cgi-bin 포함)
    ptr = index(uri, '?');       // '?' 위치 탐색 (인자 시작점)

    if (ptr) {                   
        strcpy(cgiargs, ptr+1);  // '?' 이후 문자열을 cgiargs에 저장
        *ptr = '\0';             // '?'를 null로 바꿔 URI를 자른다
    }
    else {
      strcpy(cgiargs, "");       // 인자가 없는 경우
    }
    
    strcpy(filename, ".");       // 상대 경로 사적
    strcat(filename, uri);       // uri를 파일명으로 추가
    return 0;                    // 동적 콘텐츠임을 의미함.
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;    // 파일 디스크립터
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  get_filetype(filename, filetype);     // 확장자에 따라 MIME 타입이 결정(.html -> text/html 등)

  // HTTP 응답 헤더 작성
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                       // 상태 라인
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);        // 서버 정보
  sprintf(buf, "%sConnection: close\r\n", buf);              // 연결 종료 암시
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);   // 응답 본문 크기
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // MIME 타입(html, jpg 등)

  Rio_writen(fd, buf, strlen(buf));  // 클라이언트에게 응답 헤더 전송
  printf("Response headers:\n");     // 응답헤더 관련 출력
  printf("%s", buf);                 // 서버 측에 헤더 출력(디버깅)

  // 파일을 열어서 메모리 맵핑 후 응답 본문으로 전송
  srcfd = Open(filename, O_RDONLY, 0);   // 읽기 전용으로 파일 열기
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  // 파일 내용을 메모리에 매핑
  Close(srcfd);                          // 매핑 후 파일 디스크립터는 닫음

  Rio_writen(fd, srcp, filesize);        // 매핑된 메모리에서 클라이언트로 데이터 전송
  Munmap(srcp, filesize);                // 메모리 매핑 해제
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))          // 파일 이름에 ".html"이 포함되어 있으면
    strcpy(filetype, "text/html");        // MIME 타입: HTML 문서
  else if (strstr(filename, ".gif"))      // ".gif" 확장자 → 이미지(gif)
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))      // ".png" 확장자 → 이미지(png)
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))      // ".jpg" 확장자 → 이미지(jpeg)
    strcpy(filetype, "image/jpeg");
  else                                    // 위에 해당되지 않으면 일반 텍스트로 처리
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };    // execve 인자 없이 실행할 배열 (argv)

  // 클라이언트에게 응답 헤더 전송 (동적 컨텐츠는 내용을 CGI가 출력)
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스 생성 -> CGI 생성
  if (Fork() == 0) {
    setenv("QUERY_STRING", cgiargs, 1);   // 환경변수 QUERY_STRING 설정 (ex: "100&200")
    Dup2(fd, STDOUT_FILENO);              //stdout을 클라이언트 연결(fd)로 리다이렉트
    Execve(filename, emptylist, environ); // CGI 프로그램 실행 (argv 없음)
  }
  Wait(NULL);  // 부모 프로세스는 자식이 끝날 때까지 대기
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];   // 버퍼 설정

  // HTML 본문 작성
  sprintf(body, "<html><title>Tiny Error</title>");               // HTML 제목
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);         // 배경색 흰색
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);          // 에러 코드와 메시지(ex. 404: Not Found)
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);         // 상세 설명 (원인 포함)
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);  // 서버 이름 추가

  // HTTP 응답 헤더 작성 및 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);           // 상태 줄
  Rio_writen(fd, buf, strlen(buf));                               // 헤더 전송
  sprintf(buf, "Content-type: text/html\r\n");                    // MIME 타입
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));  // 본문 길이
  Rio_writen(fd, buf, strlen(buf));

  // 본문(body) 전송
  Rio_writen(fd, body, strlen(body));
}