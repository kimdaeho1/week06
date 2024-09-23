#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct cache_block
{
  char url[MAXLINE];
  char data[MAX_OBJECT_SIZE];
  int size;
  struct cache_block *prev, *next;
}cache_block;


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);
void read_requesthdrs(rio_t *rp, char *appbuf);
void parse_url(char *url, char *hostname, char *port, char *uri);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void *thread(void *vargp);
void addcache(cache_block *new_block, int cache_size);
void removecache();
cache_block* getcache(char *url);
pthread_mutex_t cache_mutex;
void print_cache();

cache_block * cache_head = NULL; //캐시의 헤드 설정
cache_block * cache_tail = NULL;
int total_size = 0; //캐시의 총 사이즈 선언


int main(int argc, char **argv) { //입력된 인자의 개수, 명령줄 인자의 문자열 배열. 
  int listenfd, *clientfd;
  char hostname[MAXLINE], port[MAXLINE];  //MAXLINE은 최대 MAXLINE길이의 문자를 저장할 수 있음.
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  pthread_mutex_init(&cache_mutex, NULL); //뮤텍스 초기화

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); //포트 번호를 인자로 받아야함. 명령줄 인자로 포트 번호라 제공되지 않으면 종료
    exit(1);                                        //예를 들면, ./tiny 8080 이렇게 실행하면 argc는 2, argv0은 ./tiny argv1은 8080이 된다.
  }

  listenfd = Open_listenfd(argv[1]);  //listen소켓 생성. 지정된 포트에서 클라이언트 연결 요청을 기다리는 소켓 생성
  while (1) {                         //메인 루프. 클라이언트 요청이 있을떄마다 처리
    clientlen = sizeof(struct sockaddr_storage);
    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    Pthread_create(&tid, NULL, thread, clientfd);
  }

  pthread_mutex_destroy(&cache_mutex);
  return 0;
}

void doit(int clientfd) //클라이언트 요청을 처리하는 역할
{
  char buf[MAXLINE], sebuf[MAXLINE], appbuf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; //클라이언트 요청 라인에서 buf, method, uri, version 메소드, URI, HTTP버전을 저장할 버퍼
  rio_t rio , srio;  //읽기 버퍼를 위한 rio_t구조체
  char hostname[MAXLINE], port[MAXLINE], url[MAXLINE];
  int serverfd, n;
  char cache_buf[MAX_OBJECT_SIZE]= "";
  int cache_size = 0; //캐시 버퍼의 사이즈를 저장하기 위한 지역변수 설정
  struct sockaddr_in server_addr;
  cache_block *cached_block;

  Rio_readinitb(&rio, clientfd);      //요청라인을 읽고 분석한다. csapp10.8그림에서 rio로 읽어들인다. 클라 소켓에서 읽기 위한 버퍼 초기화 (fd는 클라 소켓 파일 식별자)
  Rio_readlineb(&rio, buf, MAXLINE);  //클라의 요청 라인을 읽어들이는 함수. 요청 라인에는 HTTP메소드, URI, 버전 정보가 포함

  printf("requestheader: %s", buf); 
  read_requesthdrs(&rio, appbuf);     //host, user-agent, 이런거만 뺴고 appbuf에 넣는다.

  sscanf(buf, "%s %s %s", method, url, version);  //여기에서 url, version, method를 추출
  printf("요청된 url: %s\r\n", url);
  printf("url의 길이 : %d\r\n", strlen(url));
  cached_block = getcache(url);                   //캐시된 것이 있으면 찾아보기
  if(cached_block != NULL)
  {
    Rio_writen(clientfd, cached_block->data, cached_block->size); //아마도, 캐시된 데이터를 읽는데 무언가 문제가 생긴듯 하다
    return;
  }

  parse_url(url, hostname, port, uri);      //url을 파싱해서 hostname, port, url을 추출한다

  serverfd = Open_clientfd(hostname, port); //프록시가 클라의 입장이 되어서 hostname, port를 서버에 연결하려고
  if (serverfd < 0){
    clienterror(clientfd, "서버 연결안됨", "503", "서비스가 안됨", "아무튼 안됨");
    return;
  }
  Rio_readinitb(&srio, serverfd);           //서버 소켓열어서, 새로운 버퍼를 선언한다. 
                                            //그리고 아까 requestheader에서 아래에 조건에 맞는 헤더를 제외하고 버퍼에 넣었으니까, 조건에 맞는 헤더를 아래에 써주고, 버퍼에 넣는 과정.
  sprintf(sebuf, "GET %s HTTP/1.0\r\n", uri);
  sprintf(sebuf, "%s HOST : %s\r\n",sebuf, hostname);
  sprintf(sebuf, "%s%s", sebuf, user_agent_hdr);
  sprintf(sebuf, "%sConnection : close\r\n", sebuf);
  sprintf(sebuf, "%sProxy-Connection : close\r\n", sebuf);
  sprintf(sebuf, "%s%s", sebuf, appbuf);    //아까 appbuf에서 뺐던 헤더들을 위에서 열심히 쓰고 넣었으니, appbuf에 넣음
  sprintf(sebuf, "%s\r\n", sebuf);
  Rio_writen(serverfd, sebuf, strlen(sebuf)); //서버에 요청을 보냄
  
  while((n = Rio_readlineb(&srio, sebuf, MAXLINE)) > 0)  //서버에 요청을 받아서 읽고, 
  {
    strcat(cache_buf, sebuf);               //읽어들이는 버퍼를 캐시버프에 복사하는것
    cache_size += n;                        //캐시사이즈의 사이즈를 늘려준다
    Rio_writen(clientfd, sebuf, n);         //클라에 전송하기. (buf를 새로 설정하지 않아도 작동함)
  }

  cache_block *new_block = Malloc(sizeof(cache_block));
  sprintf(new_block->url, "%s:%s%s", url,port,uri);
  strcpy(new_block->data, cache_buf);
  new_block->size = cache_size;

  while((total_size + cache_size ) > MAX_CACHE_SIZE)
  {
    removecache();
  }

  addcache(new_block, cache_size);

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
    // 호스트명을 가리키는 포인터 설정
    char *hostname_ptr = strstr(url, "//") != NULL ? strstr(url, "//") + 2 : url + 1;
    char *port_ptr = strstr(hostname_ptr, ":");    // 포트를 가리키는 포인터 설정
    char *uri_ptr = strstr(hostname_ptr, "/");    // 경로를 가리키는 포인터 설정
    char old_url;
    strcpy(old_url, url); //파싱하기 전 url을 old_url에 저장헀다가
    
                        
    if (uri_ptr > 0) {   // 경로가 존재
        *uri_ptr = '\0'; // 경로 부분을 끝낼 문자'\0'
        strcpy(uri, "/");
        strcat(uri, uri_ptr + 1); // 경로를 버퍼에 복사
    }

    if (port_ptr > 0) { //포트가 존재
        *port_ptr = '\0';           // 포트 부분을 끝낼 문자'\0'
        strcpy(port, port_ptr + 1); // 포트를 port 버퍼에 복사
    }
    strcpy(url, old_url); //복구(왜했냐면, 캐시를 저장할때 전체의 url이 저장되어야 같은 요청을 받았을 경우 캐시에서 찾아서 클라이언트에 넘겨주는데, url이 파싱되서 조각나버려서 임시조치했다)
    strcpy(hostname, hostname_ptr); // 호스트명을 hostname 버퍼에 복사
} 

void *thread(void *vargp)
{
  int clientfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(clientfd);
  Close(clientfd);
  return NULL;

}

void addcache(cache_block *new_block, int cache_size)
{
  pthread_mutex_lock(&cache_mutex);
  printf("total_size = %d\r\n", total_size);

  new_block->next = cache_head; //새 블록의 다음이 캐시의 맨앞값
  new_block->prev = NULL;   //새로운 블록의 이전값은 NULL

  if(cache_head != NULL)  //리스트에 이미 캐시가 있었다면
  {
    cache_head->prev = new_block; // 맨 앞 캐시의 이전값이 새로운 블록이 되고
  }
  else 
  {
    cache_tail = new_block; //캐시의 끝이 없을때만 cache_tail을 업데이트한다.(계속 마지막을 가리킬 것)
  }
  cache_head = new_block;
  total_size += new_block->size;
  printf("new total size = %d\r\n", total_size);
  pthread_mutex_unlock(&cache_mutex);
}

void removecache()
{
  pthread_mutex_lock(&cache_mutex);
  printf("prevdeletetotal_size = %d", total_size);
  if(cache_tail == NULL)  //tail이 null이면
  {
    pthread_mutex_unlock(&cache_mutex);
    return; //return
  }

  cache_block *old_block = cache_tail;
  total_size -= old_block->size;

  if(cache_tail->prev != NULL)        //끝블록의 앞이 존재하면.
  {
    cache_tail = cache_tail->prev;    //끝블록을 업데이트
    cache_tail->next = NULL;          //끝블록의 다음을 NULL로 바꾸고, 연결을 끊는다
  }
  else
  {
    cache_head = NULL;  //끝블록의 앞이 존재하지 않으면(head와 tail이 같은, 한개의 블록)
    cache_tail = NULL;  //둘다 NULL
  }
  
  free(old_block);
  old_block = NULL;
  printf("deletetotal_size = %d", total_size);
  pthread_mutex_unlock(&cache_mutex);
}

cache_block* getcache(char *url)
{
  print_cache();
  pthread_mutex_lock(&cache_mutex);
  printf("getcacheurl = %s\r\n",url);
  printf("url의 길이 : %d\r\n", strlen(url));
  cache_block *curr = cache_head;
  while(curr != NULL)
  {
    if(strcmp(curr->url, url) == 0)
    {
      if(curr != cache_head)//헤드가 아니라면
      {
        if(curr->prev != NULL)
        {
          curr->prev->next = curr->next;
        }
        if(curr->next != NULL)
        {
          curr->next->prev = curr->prev;
        }
        else{
          cache_tail = curr->prev;
        }

        curr->next = cache_head;
        curr->prev = NULL;
        if(cache_head != NULL)
        {
          cache_head->prev = curr;
        }
        cache_head = curr;
      }
      printf("캐시 찾았다\r\n");
      pthread_mutex_unlock(&cache_mutex);
      return curr;
    }
    curr = curr->next;
  }
  printf("아니 못찾았음\r\n");
  pthread_mutex_unlock(&cache_mutex);
  return NULL;
} 

void print_cache() {
    cache_block *current = cache_head; // 캐시의 헤드부터 시작
    while (current != NULL) {
        printf("캐시 블록 URL: %s\n", current->url);
        current = current->next; // 다음 블록으로 이동
    }
}
