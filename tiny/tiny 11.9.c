/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
//void SIGCHLD(int fd, )


int main(int argc, char **argv) { //입력된 인자의 개수, 명령줄 인자의 문자열 배열. 
  int listenfd, connfd;
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
    connfd = Accept(listenfd, (SA *)&clientaddr,  //클라 요청 수락 : 클라가 연결을 요청하면 accept함수가 이를 수락하고,
                                                  //클라와 연결을 위한 새로운 파일 식별자 connfd를 반환한다. connfd는 연결된 클라와 데이터를 주고받을때 사용
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    //이 함수를 써서 클라의 IP주소와 포트 번호를 가져오고, 출력한다. 클라의 IP주소와 포트 번호는 hostname과 port에 저장.
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit  //요청 처리 : 클라의 요청을 처리하는 함수. 클라이언트가 요청한 URI 분석하고, 정적 또는 동적 컨텐츠를 제공
    Close(connfd);  // line:netp:tiny:close //연결 끝~
  }
}

void doit(int fd) //클라이언트 요청을 처리하는 역할
{
  int is_static;  //요청된 파일이 정적인지 동적인지를 나타내는 플래그
  struct stat sbuf; //파일의 상태 정보를 저장하는 stat구조체, stat함수로 파일의 정보를 가져옴
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; //클라이언트 요청 라인에서 buf, method, uri, version 메소드, URI, HTTP버전을 저장할 버퍼
  char filename[MAXLINE], cgiargs[MAXLINE]; //filename, cgiargs : URI에서 파싱된 파일 이름과 CGI인자를 저장할 버퍼
  rio_t rio;  //읽기 버퍼를 위한 rio_t구조체

  Rio_readinitb(&rio, fd);  //요청라인을 읽고 분석한다. csapp10.8그림에서 rio로 읽어들인다. 클라 소켓에서 읽기 위한 버퍼 초기화 (fd는 클라 소켓 파일 식별자)
  Rio_readlineb(&rio, buf, MAXLINE);  //클라의 요청 라인을 읽어들이는 함수. 요청 라인에는 HTTP메소드, URI, 버전 정보가 포함
  printf("request headers : \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  //읽어들인 요청 라인에서 메소드, URI, HTTP버전을 추출.
  if (strcasecmp(method, "GET"))  //다른 메소드POST를 요청하면, main루틴으로 들어오고
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); //요청 헤더를 읽어들이는 함수. 헤더 내용을 처리하지 않고 무시.

  is_static = parse_uri(uri, filename, cgiargs);  //URI를 파일이름과 비어있을 수도 있는 CGI인자 스트링으로 분석(분리), 요청이 정적인지 동적인지 플래그 설정.
  if(stat(filename, &sbuf) < 0) //파일의 상태를 가져옴. 만약 파일이 존재하지 않으면 (<0) 404오류 ㄱㄱ
  {
    clienterror(fd, filename, "404", "Notfound", "Tiny couldn't find this file");
    return ;
  }

  if (is_static)  //정적 콘텐츠 처리 : 파일이 정적이면 처리. 
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))  //ISREG : 파일이 일반 파일인지 확인 IRUSR : 파일이 읽기 권한을 가지고 있는지 확인
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return ;
    }
    serve_static(fd, filename, sbuf.st_size); //파일 크기를 기반으로 정적 파일을 클라에게 전송
  }
  else    //정적 처리했으면 나머지가 동적이지뭐
  {       //파일이 CGI스크립트와 같은 동적 컨텐츠인 경우 처리
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))  //S_RXUR : 파일이 실행 권한이 있는지 확인
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); //프로그램 실행 후 결과를 클라에 전송
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char*longmsg) //fd : 클라리언트와의 연결 소켓 파일 식별자. 클라에게 응답을 전송
{ //cause는 오류의 원인, 요청한 자원의 이름 errnum은 오류 코드(404같은) shortmsg : 오류의 자세한 설명 longmsg : 오류에 대한 자세한 설명
  char buf[MAXLINE], body[MAXBUF];  //오류 메시지 본문 작성 buf : 응답 헤더를 저장하기 위한 버퍼, 오류 메시지를 저장하기 위한 버퍼

  sprintf(body, "<html><title>TinyError</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s : %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server<\em>\r\n", body);
  //버퍼에 HTML형식으로 오류 메시지를 작성. HTML문서의 제목 설정, 페이지 배경색, 오류 번호, 자세한 설명, 웹서버 정보들을 포함
  sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);  //HTTP응답 헤더를 buf버퍼에 작성 헤더를 클라에 전송
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");  //응답 헤더에 content-type를 text/html로 설정.
  Rio_writen(fd, buf, strlen(buf));             
  sprintf(buf, "content-length: %d\r\n\r\n", (int)strlen(body));
  } //content-length헤더 작성. body길이를 바이트 단위로 지정. 

void read_requesthdrs(rio_t *rp)  //HTTP요청의 헤더를 읽는 역할. 헤더를 읽어서 콘솔에 출력
{
  char buf[MAXLINE];  //헤더를 읽을 버퍼를 선언(여기에 저장해야겠지)
  Rio_readlineb(rp, buf, MAXLINE);  //버퍼에 한줄씩 데이터를 읽어들이는 rio_readlineb
  while(strcmp(buf, "\r\n"))  //http요청의 헤더는 빈 줄로 끝나는데, while루프는 빈줄이 아닌 경우 계속 헤더를 읽는다.
  {                           //buf의 내용이 빈 줄인지 비교한다
    Rio_readlineb(rp, buf, MAXLINE);  //빈줄이 아니면 다음 헤더를 읽어온다
    printf("%s", buf);  //읽은 헤더 줄을 콘솔에 출력
  }
  return;
}
//uri를 분석해서 요청된 파일의 경로와 CGI인자를 분리하는 역할을 함. 
//이 함수는 HTTP요청의 URI를 해석해서 정적 또는 동적 CGI스크립트를 결정하고, 해당 파일과 인자 설정.
int parse_uri(char *uri, char *filename, char *cgiargs)//클라가 요청한 uri, 요청된 파일의 경로를 지정하는 버퍼, cgi스크립트에 전달할 인자를 저장
{
  char *ptr;
  if (!strstr(uri, "cgi-bin"))  //uri에 cgi-bin이 없으면, 요청은 정적파일에 대한 것.
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");  //filename을 현재 디렉토리 "."와 요청 uri를 결합해서 파일 경로를 생성.
    strcat(filename, uri);  //요청 uri가 디렉토리 경로로 끝나면 기본파일 생성.
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1; 
  }
  else //동적 CGI스크립트 처리
  {
    ptr = index(uri, '?');  //URI에서 ?를 찾아서 CGI인자와 파일 경로 분리
    if(ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else    //CGI인자는 
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) //HTTP요청에 대해 정적 파일을 응답으로 제공하는 역할. 주어진 파일을 읽어서 HTTP응답으로 클라에게 전송
{
  int srcfd;  //정적파일을 읽기 위해 열 소켓을 나타냄
  char *srcp, filetype[MAXLINE], buf[MAXBUF]; //filetype는 파일의 MIME타입을 저장하는 버퍼. buf는 HTTP응답 헤더를 저장하는 버퍼, srcp는 팡리의 내용을 메모리에매핑한 주소를 저장


  get_filetype(filename, filetype); //주어진 파일의 이름에 따라 적절한 MIME타입 설정. .html은 text/html로 설정.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  //응답 상태 코드 설정
  sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);  //서버의 이름 지정
  sprintf(buf, "%sConnection : close\r\n", buf);  //요청 처리 후 연결 종료
  sprintf(buf, "%sContent-length : %d\r\n", buf, filesize); //파일의 크기를 응답 헤더에 포함
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);  //content-type파일의 MIME응답을 응답 헤더에 포함
  Rio_writen(fd, buf, strlen(buf)); //생성한 HTTP응답 헤더를 클라 소켓에 전송
  printf("Response headers : \n");
  printf("%s", buf);

  srcfd = Open(filename, O_RDONLY, 0);  //파일을 읽기 전용 모드로 연다
  srcp = (char *)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize); //srcp(메모리의 포인터) srcfd가 파일디스크립터
  Close(srcfd); //파일 식별자 닫기
  Rio_writen(fd, srcp, filesize); //매핑된 파일 내용을 클라 소켓에 전송
  free(srcp);
  }

  void get_filetype(char *filename, char *filetype)
  {
    if(strstr(filename, ".html"))
      strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
      strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
      strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
      strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
      strcpy(filetype, "video/mp4");
    else
      strcpy(filetype, "test/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) //동적인 컨텐츠를 클라에 제공하는 역할. 
//클라의 요청을 처리하고, CGI 프로그램을 자식 프로세스로 실행하여 결과를 클라에게 반환합니다. 
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  //응답 헤더 작성
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server : Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  //자식 프로세스 생성 및 CGI스크립트 실행
  if (Fork() == 0)
  {
    setenv("QUERY_STRING", cgiargs, 1); //환경변수 QUERY_STRING 실행
    Dup2(fd, STDOUT_FILENO);            //표준 출력을 클라이언트 소켓으로 리디렉션
    Execve(filename, emptylist, environ); //CGI 스크립트 실행
  }
  Wait(NULL); //자식프로세스 종료를 기다림 
}