#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void read_requesthdrs(rio_t *rp, char *appbuf);
void parse_url(char *url, char *hostname, char *port, char *uri);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) { //입력된 인자의 개수, 명령줄 인자의 문자열 배열. 
  int listenfd, clientfd;
  char hostname[MAXLINE], port[MAXLINE];  //MAXLINE은 최대 MAXLINE길이의 문자를 저장할 수 있음.
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); //포트 번호를 인자로 받아야함. 명령줄 인자로 포트 번호라 제공되지 않으면 종료
    exit(1);                                        //예를 들면, ./tiny 8080 이렇게 실행하면 argc는 2, argv0은 ./tiny argv1은 8080이 된다.
  }

  listenfd = Open_listenfd(argv[1]);  //listen소켓 생성. 지정된 포트에서 클라이언트 연결 요청을 기다리는 소켓 생성
  while (1) {                         //메인 루프. 클라이언트 요청이 있을떄마다 처리
    clientlen = sizeof(clientaddr); //클라 연결 정보 초기화 : clientaddr의 크기를 설정해서, accept호출시 사용
    clientfd = Accept(listenfd, (SA *)&clientaddr,  //클라 요청 수락 : 클라가 연결을 요청하면 accept함수가 이를 수락하고,
                                                  //클라와 연결을 위한 새로운 파일 식별자 connfd를 반환한다. connfd는 연결된 클라와 데이터를 주고받을때 사용
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    //이 함수를 써서 클라의 IP주소와 포트 번호를 가져오고, 출력한다. 클라의 IP주소와 포트 번호는 hostname과 port에 저장.
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(clientfd);   // line:netp:tiny:doit  //요청 처리 : 클라의 요청을 처리하는 함수. 클라이언트가 요청한 URI 분석하고, 정적 또는 동적 컨텐츠를 제공
    Close(clientfd);  // line:netp:tiny:close //연결 끝~
  }
}

void doit(int clientfd) //클라이언트 요청을 처리하는 역할
{
  char buf[MAXLINE], sebuf[MAXLINE], appbuf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; //클라이언트 요청 라인에서 buf, method, uri, version 메소드, URI, HTTP버전을 저장할 버퍼
  rio_t rio , srio;  //읽기 버퍼를 위한 rio_t구조체
  char hostname[MAXLINE], port[MAXLINE], url[MAXLINE];
  int serverfd, n;
  struct sockaddr_in server_addr;

  Rio_readinitb(&rio, clientfd);  //요청라인을 읽고 분석한다. csapp10.8그림에서 rio로 읽어들인다. 클라 소켓에서 읽기 위한 버퍼 초기화 (fd는 클라 소켓 파일 식별자)
  Rio_readlineb(&rio, buf, MAXLINE);  //클라의 요청 라인을 읽어들이는 함수. 요청 라인에는 HTTP메소드, URI, 버전 정보가 포함

  printf("requestheader: %s", buf); 
  read_requesthdrs(&rio, appbuf); //host, user-agent, 이런거만 뺴고 appbuf에 넣는다.

  sscanf(buf, "%s %s %s", method, url, version);  //여기에서 url, version, method를 추출
  printf("method : %s", method);
  printf("url : %s\n", url);

  parse_url(url, hostname, port, uri);  //url을 파싱해서 hostname, port, url을 추출한다
  printf("HOSTNAME : %s", hostname);
  printf("PORT : %s", port);
  printf("URI : %s", uri);

  serverfd = Open_clientfd(hostname, port); //프록시가 클라의 입장이 되어서 hostname, port를 서버에 연결하려고
  if (serverfd < 0){
    clienterror(clientfd, "서버 연결안됨", "503", "서비스가 안됨", "아무튼 안됨");
    return;
  }
  Rio_readinitb(&srio, serverfd);           //서버 소켓열어서, 새로운 버퍼를 선언한다
  sprintf(sebuf, "GET %s HTTP/1.0\r\n", uri);
  sprintf(sebuf, "%s HOST : %s\r\n",sebuf, hostname);
  sprintf(sebuf, "%s%s", sebuf, user_agent_hdr);
  sprintf(sebuf, "%sConnection : close\r\n", sebuf);
  sprintf(sebuf, "%sProxy-Connection : close\r\n", sebuf);
  sprintf(sebuf, "%s%s", sebuf, appbuf);
  sprintf(sebuf, "%s\r\n", sebuf);
  Rio_writen(serverfd, sebuf, strlen(sebuf)); //아까 appbuf에서뺐던 정보를 내가 입맛대로 넣기
                                              //서버에 요청을 보냄
  printf("Read start \r\n");
  while((n = Rio_readlineb(&srio, sebuf, MAXLINE)) > 0)  //서버에 요청을 받아서 읽고, 
  {
    Rio_writen(clientfd, sebuf, n);         //클라에 전송하기. (buf를 새로 설정하지 않아도 작동함)
    printf("%s", sebuf);
  }
  Close(serverfd);  //클라 소켓 닫기
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char*longmsg) //fd : 클라리언트와의 연결 소켓 파일 식별자. 클라에게 응답을 전송
{ //cause는 오류의 원인, 요청한 자원의 이름 errnum은 오류 코드(404같은) shortmsg : 오류의 자세한 설명 longmsg : 오류에 대한 자세한 설명
  char buf[MAXLINE], body[MAXBUF];  //오류 메시지 본문 작성 buf : 응답 헤더를 저장하기 위한 버퍼, 오류 메시지를 저장하기 위한 버퍼

  sprintf(body, "<html><title>TinyError</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s : %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  //버퍼에 HTML형식으로 오류 메시지를 작성. HTML문서의 제목 설정, 페이지 배경색, 오류 번호, 자세한 설명, 웹서버 정보들을 포함
  sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);  //HTTP응답 헤더를 buf버퍼에 작성 헤더를 클라에 전송
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");  //응답 헤더에 content-type를 text/html로 설정.
  Rio_writen(fd, buf, strlen(buf));             
  sprintf(buf, "content-length: %d\r\n\r\n", (int)strlen(body));
  } //content-length헤더 작성. body길이를 바이트 단위로 지정. 

void read_requesthdrs(rio_t *rp, char *appbuf) {
    char buf[MAXLINE];
    char *byonsu[] = {"Host", "User-Agent", "Connection", "Proxy-Connection"};
    int is_excluded;

    Rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        is_excluded = 0; // 초기화

        // byonsu 배열에 있는지 확인
        for (int i = 0; i < 4; i++) {
            if (strstr(buf, byonsu[i]) == buf) { // 헤더가 byonsu에 포함되어 있으면
                is_excluded = 1; // 제외 플래그 설정
                break;
            }
        }

        // 제외되지 않은 헤더는 appbuf에 저장
        if (!is_excluded) {
            strcat(appbuf, buf); // appbuf에 추가
        }

        printf("%s", buf); // 콘솔에 출력
        Rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 읽기
    }
}

void parse_url(char *url, char *hostname, char *port, char *uri) {
    printf("---parse_url: %s\n", url); // 디버깅용 메시지 출력
    // 호스트명을 가리키는 포인터 설정
    char *hostname_ptr = strstr(url, "//") != NULL ? strstr(url, "//") + 2 : url + 1;
    char *port_ptr = strstr(hostname_ptr, ":");    // 포트를 가리키는 포인터 설정
    char *path_ptr = strstr(hostname_ptr, "/");    // 경로를 가리키는 포인터 설정
    
    // 경로가 존재한다면
    if (path_ptr > 0) {
        *path_ptr = '\0'; // 경로 부분을 끝낼 문자('\0')로 대체
        strcpy(uri, "/");
        strcat(uri, path_ptr + 1); // 경로를 path 버퍼에 복사
    }
    // 포트가 존재한다면
    if (port_ptr > 0) {
        *port_ptr = '\0'; // 포트 부분을 끝낼 문자('\0')로 대체
        strcpy(port, port_ptr + 1); // 포트를 port 버퍼에 복사
    }

    strcpy(hostname, hostname_ptr); // 호스트명을 hostname 버퍼에 복사
    printf("---parse_url url : %s, host: %s, port: %s, uri: %s\n",url, hostname, port, uri); // 호스트명, 포트, 경로를 출력
} 