/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p, *method;    //buf : 쿼리 문자열을 저장하는 포인터, p는 문자열 내에서 구분자&를 찾기 위한 포인터
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&'); // strchr함수는 문자열 buf에서 첫&문자의 위치를 찾는다. &는 두 숫자를 구분한다
    *p = '\0';  //&문자를 '\0', NULL문자로 바까서 buf문자열을 두개로 분리한다.
    sscanf(buf, "num1=%s", arg1);
    sscanf(p+1, "num2=%s", arg2);
    n1 = atoi(arg1);    //atoi함수를 사용해 arg1과 arg2를 정수로 변환한다.
    n2 = atoi(arg2);
  }
  method = getenv("REQUEST_METHOD");

  sprintf(content, "QUERY_STRING=%s", buf); //printf로 HTTP응답 헤더 출력하기
  sprintf(content, "Welcome to add.com:");  
  sprintf(content, "%s The internet addition portal. \r\n<p>", content);
  sprintf(content, "%s The answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);

  printf("Connection : close\r\n");
  printf("Content-length : %d\r\n", (int)strlen(content));
  printf("Content-type : text/html\r\n\r\n");
  if(strcasecmp(method, "GET") == 0)
  {
    printf("%s", content);
  }
  
  fflush(stdout);

  exit(0);
}
/* $end adder */
