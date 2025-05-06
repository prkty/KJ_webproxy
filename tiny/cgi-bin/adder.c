/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
#include "csapp.h"

int main(void)
{
  char *buf, *p;        // QUERY_STRING을 저장할 포인터와 분리용 포인터
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];   // 파싱된(분석) 인자들과 출력할 content 버퍼
  int n1 = 0, n2 = 0;   // 숫자형으로 변환된 두 값

  // QUERY_STRING 환경 변수 가져오기 (예시로 "100&200")
  if ((buf = getenv("QUERY_STRING")) != NULL)  {
    p = strchr(buf, '&');   // '&' 문자 위치 탐색(strchr의 역할)
    *p = '\0';              // '&'를 NULL 종료로 바꿔서 문자열을 분리(문자열의 끝 표시)
    // buf → "100", p+1 → "200"

    // 문자열을 arg로 복사한 후 atoi로 정수형으로 변환
    // 문자열을 개별 변수로 복사
    strcpy(arg1, buf);     // arg1 ← "100"
    strcpy(arg2, p + 1);   // arg2 ← "200"

    //문자열을 정수로 변환
    n1 = atoi(arg1);       // n1 = 100
    n2 = atoi(arg2);       // n2 = 200
  }

  // content 버퍼에 응답 HTML 본문 작성(응답 몸체)
  sprintf(content, "QUERY_STRING=%S", buf);                                       // 원래 쿼리 문자열 출력
  sprintf(content, "Welgcome to add.com: ");                                      // 인삿말
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);           // 누적해서 작성
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n1 + n2); // 덧셈 결과 출력
  sprintf(content, "%sThanks for visiting!\r\n", content);                        // 마무리 메시지

  // HTTP 응답 헤더 출력
  printf("Connection: close\r\n");                         // 연결 종료
  printf("Content-length: %d\r\n", (int)strlen(content));  // 본문 길이
  printf("Content-type: text/html\r\n\r\n");               // MIME 타입
  printf("%s", content);                                   // 본문 출력
  fflush(stdout);                                          // 출력 버퍼 비움

  exit(0);    // 프로그램 종료
}