#include <stdio.h>
#include "csapp.h"  // csapp 라이브러리는 CS:APP 교재에서 제공하는 네트워크 및 시스템 프로그래밍 지원 함수 모음입니다.

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000   // 캐시 전체 최대 크기 (약 1MB)
#define MAX_OBJECT_SIZE 102400   // 한 객체의 최대 크기 (약 100KB)

/* You won't lose style points for including this long line in your code */
// 프록시 서버가 요청할 때 사용하는 User-Agent 헤더
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

// 연결을 닫도록 지정하는 HTTP 헤더
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";

// Host 헤더를 만들기 위한 형식 문자열
static const char *host_hdr_format = "Host: %s\r\n";

// GET 요청을 위한 형식 문자열
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";

// HTTP 요청 헤더의 끝을 나타낸다.
static const char *endof_hdr = "\r\n";

// 특정 헤더 이름을 비교할 때 사용하기 위한 문자열 상수
static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

// 클라이언트 요청을 처리하는 함수
void doit(int connfd);

// URI를 호스트명, 경로(path), 포트로 분해하는 함수
void parse_uri(char *uri,char *hostname,char *path,int *port);

// 클라이언트의 요청을 바탕으로 서버에 보낼 HTTP 헤더를 작성하는 함수
void build_http_header(char *http_header,char *hostname,char *path,int port, rio_t *client_rio);

// 엔드서버와 연결하는 함수
int connect_endServer(char *hostname,int port,char *http_header);

// 멀티스레딩 처리를 위한 함수
void *thread(void *vargsp);

// functions for caching
void init_cache(void);               // 캐시 초기화
int reader(int connfd, char *url);   // 클라이언트가 요청한 URL이 캐시에 존재하는지 확인하고 데이터를 전송하는 함수 (리더 역할)
void writer(char *url, char *buf);   // 새 데이터를 캐시에 저장하는 함수 (라이터 역할)

// 개별 캐시 블록 정보를 담을 구조체
typedef struct {
  char *url;        // 요청된 URL
  int *flag;        // 캐시 블록이 사용 중인지 아닌지 표시 (0: 빈 블록, 1: 사용 중)
  int *cnt;         // 사용 빈도 또는 LRU 정책을 위한 카운트
  int *content;     // 실제 응답 데이터를 저장하는 공간 (클라이언트에 보낼 내용)
} Cache_info;

Cache_info *cache;  // 캐시 전체를 담을 포인터 배열
int readcnt;        // 동시에 읽는 reader 수를 관리하기 위한 변수
sem_t mutex, w;     // 세마포어: mutex는 reader 수 갱신, w는 writer 접근 제어용

int main(int argc, char **argv) {
  // 포트 번호가 입력되지 않았을 시, 사용법 출력
  if(argc != 2) {
    fprintf(stderr, "usage :%s <port> \n", argv[0]);
    exit(1);
  }

  init_cache();             // 캐시 초기화 함수 호출

  int listenfd, connfd;
  socklen_t clientlen;
  char hostname[MAXLINE], port[MAXLINE];
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  listenfd = Open_listenfd(argv[1]);   //클라이언트로부터 연결을 수신할 소켓을 열고 대기 상태로 만든다.

  // 무한 루프: 클라이언트의 연결 요청을 지속적으로 수락
  while(1) {
    clientlen = sizeof(clientaddr);

    // 클라이언트의 연결 요청을 수락하고, 연결된 소켓의 파일 디스크립터 변환
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                     

    // 연결된 클라이언트의 호스트명과 포트 정보를 가져온다
    Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s %s).\n",hostname, port);

    // 새로운 스레드를 생성하여 클라이언트 요청을 처리 (connfd를 인자로 전달)
    Pthread_create(&tid, NULL, thread, (void *)connfd);
  }
  return 0;
}


void *thread(void *vargs) {
  int connfd = (int)vargs;        // main 함수에서 전달받은 연결 소켓 번호를 정수형으로 변환
  Pthread_detach(pthread_self()); // 현재 스레드를 분리 상태로 설정하고, 스레드 종료시 자동으로 자원이 반환되어 메모리 누수 방지함
  doit(connfd);                   // 클라이언트 요청 처리 (HTTP 요청을 파싱하고 응답 생성 등 역할)
  Close(connfd);                  // 처리 후 클라이언트 소켓 닫음
}


void doit(int connfd) {
  int end_serverfd;  // 엔드서버(tiny.c)와 연결된 파일 디스크립터

  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];  // 요청 라인을 저장할 변수들
  char endserver_http_header[MAXLINE];    // 엔드서버에 보낼 최종 HTTP 헤더
  
  // URI 파싱을 통해 추출할 값들
  char hostname[MAXLINE], path[MAXLINE];
  int port;

  rio_t rio, server_rio; // 클라이언트와 통신할 rio / 엔드서버와 통신할 rio 선언

  char url[MAXLINE];                 // 요청받은 URL 저장용
  char content_buf[MAX_OBJECT_SIZE]; // 캐시에 저장할 응답 데이터 버퍼

  // 1. 클라이언트 요청 라인 읽기: ex) GET http://localhost:8000/home.html HTTP/1.1
  Rio_readinitb(&rio, connfd);                   // 클라이언트 소켓에 대해 rio 초기화
  Rio_readlineb(&rio, buf, MAXLINE);             // 요청 라인 한 줄 읽기
  sscanf(buf, "%s %s %s", method, uri, version); // (메서드, URI, 버전) 파싱
  strcpy(url, uri);                              // 캐시 키로 사용할 URL 복사

  // 2. GET 요청만 지원함 (다른 메서드 무시)
  if (strcasecmp(method, "GET")) {                 // method가 GET으로 같으면 0(false)를 반환함 즉, 다르다면 출력함
    printf("Proxy does not implement the method"); // 에러 출력
    return;
  }
  
  // 3. 캐시에 해당 요청 결과가 있는지 확인 (있으면 클라이언트에 바로 응답 후 종료)
  if (reader(connfd, url)) {
    return ;
  }

  // 4. URI를 파싱하여 hostname(localhost), path, port(20000) 추출
  // 프록시 서버가 엔드 서버로 보낼 정보들을 파싱
  parse_uri(uri, hostname, path, &port);

  // 5. 엔드서버에 보낼 요청 헤더 구성 (GET /path HTTP/1.0 + Host + User-Agent 등)
  // 프록시 서버가 엔드 서버로 보낼 요청 헤더들을 만든다. endserver_http_header가 채워짐
  build_http_header(endserver_http_header, hostname, path, port, &rio);

  // 6. 엔드서버에 연결 시도
  // 프록시 서버와 엔드 서버를 연결함
  end_serverfd = connect_endServer(hostname, port, endserver_http_header); // 프록시 측에서 엔드 서버로 연결된 clinetfd
  if (end_serverfd < 0) {
    printf("connection failed\n");  // 연결 실패시 종료
    return;
  }

  // 7. 엔드서버에 HTTP 요청 헤더 전송
  Rio_readinitb(&server_rio, end_serverfd);                                        // 엔드서버와 소켓에 대해 rio 초기화
  Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));  // HTTP 요청 헤더 전송

  // 8. 엔드서버의 응답 메시지를 읽어서 클라이언트에 보내고, content_buf에 복사
  size_t n;
  int total_size = 0;  // content_buf에 저장한 총 크기 (캐시용)
  while((n = Rio_readlineb(&server_rio,buf, MAXLINE)) != 0) {
    printf("proxy received %d bytes,then send\n",n);  // 디버깅 로그
    Rio_writen(connfd, buf, n);                       // 클라이언트에 응답 전달
    // connfd -> client와 proxy 연결 소켓. proxy 관점.

    // 캐시 버퍼에 응답 내용 저장 (최대 크기 초과하지 않도록)
    if (total_size + n < MAX_OBJECT_SIZE) {
      strcpy(content_buf + total_size, buf);
    }
    total_size += n;
  }
  
  // 9. 전체 응답 크기가 MAX_OBJECT_SIZ 보다 작으면 캐시에 저장
  if (total_size < MAX_OBJECT_SIZE) {
    writer(url, content_buf);
  }

  // 10. 엔드서버와 연결 종료
  Close(end_serverfd);
}


void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio) {
  char buf[MAXLINE],request_hdr[MAXLINE],other_hdr[MAXLINE],host_hdr[MAXLINE];

  /* 요청 헤더 초기화 */
  memset(buf, 0, MAXLINE);
  memset(request_hdr, 0, MAXLINE);
  memset(other_hdr, 0, MAXLINE);
  memset(host_hdr, 0, MAXLINE);

  // 1. 요청 라인 작성: ex) GET /index.html HTTP/1.0
  sprintf(request_hdr, requestlint_hdr_format, path);

  // 2. 클라이언트 요청 헤더 읽으면서 필요한 부분만 필터링
  // 클라이언트 요청 헤더들에서 Host header와 나머지 header들을 구분해서 넣어줌
  while(Rio_readlineb(client_rio, buf, MAXLINE)>0) {
    if (strcmp(buf, endof_hdr) == 0) break;               // EOF처리로 빈 줄('\r\n') 만나면 헤더 끝

    // 2-1. Host 헤더 찾기
    if (!strncasecmp(buf, host_key, strlen(host_key))) {  //Host: 일치하는 게 있으면 0으로 false이지만 !으로 true되어 if문 실행
        strcpy(host_hdr, buf);                            // Host 헤더는 저장만 함
        continue;
    }

    // 2-2. Connection / Proxy-Connection / User-Agent는 무시 (우리가 고정된 값 사용)
    if (strncasecmp(buf, connection_key, strlen(connection_key)) && 
        strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key)) &&
        strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
      strcat(other_hdr,buf);    // 그 외 헤더들은 누적 저장
    }
  }

  // 3. 클라이언트가 Host 헤더를 안 보낸 경우 직접 만들어준다.
  if (strlen(host_hdr) == 0) {
      sprintf(host_hdr,host_hdr_format,hostname);
  }

  // 4. 최종 HTTP 요청 헤더 구성 (엔드서버에 보낼 요청)
  // 프록시 서버가 엔드 서버로 보낼 요청 헤더 작성
  sprintf(http_header,"%s%s%s%s%s%s%s", 
          request_hdr,      // GET /path HTTP/1.0
          host_hdr,         // Host: example.com
          conn_hdr,         // Connection: close
          prox_hdr,         // Proxy-Connection: close
          user_agent_hdr,   // 고정된 User-Agent
          other_hdr,        // 클라이언트의 기타 헤더들
          endof_hdr);       // 마지막 줄 "\r\n"
  
  return ;
}


// 프록시 서버와 엔드 서버 연결
inline int connect_endServer(char *hostname,int port,char *http_header) {
  char portStr[100];     
  sprintf(portStr,"%d",port);              // 1. 정수형 포트 번호를 문자열로 변환 (Open_clientfd는 문자열 포트 번호를 받음)
  return Open_clientfd(hostname,portStr);  // 2. 엔드 서버에 연결 요청 후 연결 소켓 디스크립터(fd)를 반환
}


void parse_uri(char *uri,char *hostname,char *path,int *port) {
  *port = 80;                    // 기본 포트를 HTTP의 기본 포트인 80으로 설정

  char* pos = strstr(uri,"//");  // "http://"이후의 위치를 찾음
  pos = pos!=NULL? pos+2:uri;    // "http://"이 있으면 건너뛰고, 없으면 uri 전체 사용

  // hostname, port, path를 본격적으로 파싱
  char *pos2 = strstr(pos,":");      // 포트 구분자 ':' 찾기
  if (pos2 != NULL) {                // 포트가 NULL이 아니라면 실행
    *pos2 = '\0';                    // ':' 위치를 기준으로 문자열 자르기
    sscanf(pos,"%s",hostname);       // ':' 앞 -> hostname 스캔
    sscanf(pos2+1,"%d%s",port,path); // ':' 뒤 -> port와 path 스캔(포트를 80에서 클라이언트 지정 포트로 변경)
  }

  else {                          // 포트가 NULL이라면 실행
    pos2 = strstr(pos,"/");       // ':'이 없으면 '/' 기준으로 분리
    if (pos2!=NULL) {
      *pos2 = '\0';               // '/' 앞을 hostname으로 잘라내기
      sscanf(pos,"%s",hostname);
      *pos2 = '/';                // '/' 을 다시 복원
      sscanf(pos2,"%s",path);     // '/'부터 나머지를 path로 저장
    } 
    else {
      sscanf(pos,"%s",hostname);  // '/'도 없으면 전체를 hostname으로 처리
    }
  }
  return;
}

// cache 초기화
void init_cache() {
  Sem_init(&mutex, 0, 1);  // readcnt 값을 보호하기 위한 세마포어 초기화 (binary semaphore)
  Sem_init(&w, 0, 1);      // writer(쓰기 접근)를 위한 세마포어 초기화 (binary semaphore)
  readcnt = 0;             // 현재 읽고 있는 reader의 수를 0으로 초기화
  cache = (Cache_info *)Malloc(sizeof(Cache_info) * 10);                  // 캐시의 최대 크기는 1MB이고 캐시의 객체는 최대 크기가 100KB이라서 10개의 공간을 동적할당
  for (int i = 0; i < 10; i++) {
      cache[i].url = (char *)Malloc(sizeof(char) * 256);                  // URL 저장 공간 (최대 256바이트)
      cache[i].flag = (int *)Malloc(sizeof(int));                         // 캐시 사용 여부 플래그 4바이트 저장
      cache[i].cnt = (int *)Malloc(sizeof(int));                          // LRU(Latest Recently Used) 순서 추적용(얼마나 사용되지 않았나) 4바이트
      cache[i].content = (char *)Malloc(sizeof(char) * MAX_OBJECT_SIZE);  // 실제 콘텐츠 저장 공간 100KB 할당
      *(cache[i].flag) = 0;                                               // flag 0 → 사용되지 않은 상태로 설정
      *(cache[i].cnt) = 1;                                                // LRU 용으로 cnt +1로 증가 설정, 최근에 찾은 것일 수록 0이랑 가까움
  }
}

// cache에서 요청한 url 있는지 찾는 함수
// 세마포어를 이용해서 reader가 먼저 되고 여러 thread가 읽고 있으면 writer는 할 수가 없게함
int reader(int connfd, char *url) {
  int return_flag = 0;    // 캐시에서 찾았으면 1, 못 찾았으면 0 (return 값으로 사용됨)
  P(&mutex);              // readcnt를 보호하기 위해 mutex 진입
  readcnt++;              // 첫 번째 reader는 writer를 막기 위해 write 세마포어 획득
  if(readcnt == 1) {
    P(&w);
  }
  V(&mutex);              // 뮤텍스 해제

  // 캐시를 다 돌면서 캐시에 써있고 캐시의 URL과 현재 요청한 URL이 같으면,
  // 클라이언트 디스크립터에 캐시의 내용을 쓰고 해당 캐시의 cnt를 0으로 초기화 후 break 실행
  for(int i = 0; i < 10; i++) {

    // flag가 1이면 해당 캐시 블록이 사용중이고, strcmp로 URL과 비교하여 일치하면 Hit
    if(*(cache[i].flag) == 1 && !strcmp(cache[i].url, url)) {
      Rio_writen(connfd, cache[i].content, MAX_OBJECT_SIZE);  // 클라이언트에게 캐시 데이터 전송
      return_flag = 1;                                        // flag 1로 캐시 Hit 표시
      *(cache[i].cnt) = 1;                                    // LRU 정책에 따라 cnt 0으로 초기화 (최근사용)
      break;
    }
  }    
    
  // 모든 캐시 객체의 cnt를 +1 함. 즉, 방문안한 일수를 올려줌
  for(int i = 0; i < 10; i++) {
    (*(cache[i].cnt))++;  // 모든 캐시 블록의 cnt 증가 (오래된 정도 반영)
  }
  P(&mutex);              // readcnt 접근을 위한 mutex 진입
  readcnt--;              // reader 수 감소

  if(readcnt == 0) {      // 마지막 reader가 나갈 때, writer에게 문을 열어준다
    V(&w);
  }
  V(&mutex);              // mutex 해제
  return return_flag;     // 캐시 Hit 여부를 반환한다(1 또는 0)
}

// 캐시에서 요청한 URL의 정보 쓰기
// 세마포어를 이용해서 writer는 한번 수행함
void writer(char *url, char *buf) {
  P(&w);                         // 쓰기 작업 전, 세마포어 w를 통해 진입 제한(여러 스레드가 동시 캐시 수정 x토록 동기화)

  int idx = 0;                   // 데이터를 저장할 캐시 인덱스
  int max_cnt = 0;               // 현재까지 방문 안한 최대 일수 cnt(가장 오래된 캐시 찾기)

  // 10개의 캐시를 돌고 만약 비어있는 곳이 있으면 비어있는 곳에 index를 찾고, 
  // 없으면 가장 오래 방문 안한 곳의 index 찾음
  for(int i = 0; i < 10; i++) {
    if(*(cache[i].flag) == 0) {     // 캐시 블록이 비어 있는 경우
      idx = i;                      // 해당 인덱스를 선택
      break;                        // 더 이상 탐색 X
    }

    if(*(cache[i].cnt) > max_cnt) { // 가장 오래전에 사용된 캐시 블록을 찾음 (cnt가 가장 큰 것)
      idx = i;                      // 현재 인덱스 선택
      max_cnt = *(cache[i].cnt);    // 최대 cnt 갱신
    }
  }
  // 선택한 캐시 블록에 데이터 저장
  *(cache[idx].flag) = 1;          // 캐시 블록이 사용 중임을 표시
  strcpy(cache[idx].url, url);     // 요청 URL 저장
  strcpy(cache[idx].content, buf); // 응답 데이터 저장
  *(cache[idx].cnt) = 1;           // 최근 사용했음을 나타내기 위해 cnt를 1로 초기화

  V(&w);                  // writer 세마포어를 올려서 다른 스레드들이 캐시에 접근 가능하게 함
}