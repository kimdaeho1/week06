#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int clientfd, int *serverfd);
void parse_url(char *url, char *hostname, char *port, char *uri);
void proxy_request(int cliendfd, char *url, int *serverfd);
void proxy_accept(int serverfd, int clientfd);

int main(int argc, char **argv) {
  int listenfd, connfd, serverfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);  //포트번호를 입력하셔야 합니다~ usage : 알려주는 함수
    exit(1); //exit 코드 1
  }

  listenfd = Open_listenfd(argv[1]);
  while(1)  //무한루프를 돌아요
  {
    clientlen = sizeof(clientaddr); // 클라이언트의 주소 정보를 저장하는 구조체인 clientaddr의 크기를 나타냄
    connfd  = Accept(listenfd, (SA *)&clientaddr, &clientlen);  //accept했을때 소켓 생성. 인자로 listenfd(지금 듣기상태인지), clientaddr은 클라의 주소 정보를 SA로 형변환해서 저장하기 위해 사용된다. 그리고 클라이언트 주소의 크기도 저장해야겠지?
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); //나에게 보여줄 이름도 생성(출력문에서 쓸라고)
    printf("Accepted connection from (%s, %s)\n", hostname, port);  //연결 성공 메시지
    doit(connfd, &serverfd);
    Close(connfd);
  }
  return 0;
}

void doit(int clientfd, int *serverfd)
{
  char buf[MAXLINE], url[MAXLINE], version[MAXLINE];
  rio_t rio;
  
  Rio_readinitb(&rio, clientfd);
  if(Rio_readlineb(&rio, buf, MAXLINE) <= 0)
  {
    printf("클라 요청을 읽을 수 없음");
    return;
  }

  sscanf(buf, "%*s %s %*s", url);


  proxy_request(clientfd, url, serverfd);
  if(*serverfd >= 0)
  {
    proxy_accept(*serverfd, clientfd);
    Close(*serverfd);
  }
}

void proxy_request(int clientfd, char *url, int *serverfd)
{
  struct sockaddr_in server_addr;
  char buf[MAXLINE];
  char hostname[MAXLINE], port[MAXLINE], uri[MAXLINE];
  rio_t rio;
  
  Rio_readinitb(&rio, clientfd);

  if(Rio_readlineb(&rio, buf, MAXLINE) <= 0)
  {
    printf("클라이언트 요청 안옴");
    return;
  }

  parse_url(url, hostname, port, uri);          //호스트와 포트 정보 URI추출
  

  memset(&server_addr, 0, sizeof(server_addr)); //서버 구조체 초기화
  server_addr.sin_family = AF_INET;             //서버 구조체에 넣을 아이들
  server_addr.sin_port = htons(atoi(port));

  if(inet_pton(AF_INET, hostname, &server_addr.sin_addr)<=0)
  {
    printf("호스트 IP변환 안됨");
    return;
  }

  *serverfd = socket(AF_INET, SOCK_STREAM, 0);   //소켓 생성

  if(connect(*serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("연결 안됨 ㅅㄱ");
    return;
  }

  Rio_writen(*serverfd, buf, strlen(buf));
  while(Rio_readlineb(&rio, buf, MAXLINE)>0)
  {
    if(strcmp(buf, "\r\n") == 0)
      break;
    Rio_writen(*serverfd, buf, strlen(buf));
  }

}

void proxy_accept(int serverfd, int clientfd)
{
  char buf[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, serverfd);
  while(Rio_readlineb(&rio, buf, MAXLINE) > 0)
  {
    Rio_writen(clientfd, buf, strlen(buf));
  }
}

void parse_url(char *url, char *hostname, char *port, char *uri)
{
  char *uriptr, *hostptr, *portptr;

  hostptr = strstr(url, "://");
  if(hostptr)
  {
    hostptr += 3; // ://길이만큼 이동
  }
  else
  {
    hostptr = url;
  }

  uriptr = strchr(hostptr, '/');
  if(uriptr)
  {
    *uriptr = '\0';
    strcpy(uri, uriptr + 1);
  }
  else 
  {
    strcpy(uri, "");
  }

  portptr = strchr(hostptr, ':');
  if (portptr)
  {
    *portptr = '\0';
    strcpy(hostname, hostptr);
    strcpy(port, portptr +1 );

  }
  else 
  {
    strcpy(hostname, hostptr);
    strcpy(port, "80");
  }

}