#include <stdio.h>
#include "csapp.h"
#include "pthread.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define PUT(n) (CURR_CACHE_SIZE + n)

static char *server_port = "8080";
static char *server_addr = "/";
static char *host = "localhost";
int total = 0;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void *thread(void *vargp);

int main(int argc, char **argv)
{
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    Pthread_create(&tid, NULL, thread, connfd);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // method: GET, uri: 경로, version: HTTP 1.1
  char filename[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  rio_t rio;
  Cache_t *node;

  /* Read request line and header */
  Rio_readinitb(&rio, fd);           // fd를  rio_t 타입의 rio 버퍼와 연결
  if(!(Rio_readlineb(&rio, buf, MAXLINE))) {
    return;
  } // buf에 rio 다음 텍스트 줄을 복사
  printf("Request headers:\n");
  sscanf(buf, "%s %s %s", method, uri, version); // buf의 내용을 각 char 배열에 매핑
  printf("%s", buf);
  parse_request_hdr(filename, uri, path, hostname, port); // uri parsing 

  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0))
  {
    // GET요청 아니면 에러 발생
    clienterror(fd, method, "501", "Not implemented", "Tiny couldn't find this file");
    return;
  }

  if((node = find_cache(path)) != NULL){
    printf("-----HIT CACHE!!-----\n");
    if(node != NULL){
      Rio_writen(fd, node->data, node->size);
      return;
    }
  }

  char buf2[MAXLINE];
  buf2[0] = '\0';
  strcat(buf2, (char *)method);
  strcat(buf2, " ");
  strcat(buf2, path);
  strcat(buf2, " ");
  strcat(buf2, version);
  strcat(buf2, "\r\n");

  read_requesthdrs(&rio);

  int clientfd;
  rio_t rio_server, rio_client;

  clientfd = Open_clientfd((char *)hostname, (char *)port);

  if (clientfd < 0)
  {
    fprintf(stderr, "Error: Failed to connect to the destination server\n");
    return;
  }
  
  Rio_writen(clientfd, buf2, strlen(buf2));
  Rio_writen(clientfd, "\r\n", 2); // clientfd(서버)에 요청 내용 전송

  Rio_readinitb(&rio_server, clientfd); // 서버에서 받은 내용 저장    // 프록시에서 클라이언트에 내용 전송?

  char response[MAX_OBJECT_SIZE];
  ssize_t size = Rio_readnb(&rio_server,response,MAX_OBJECT_SIZE);

  Rio_writen(fd, response, size);
  printf("--------size%dsize---------", size);
  if(size > MAX_OBJECT_SIZE){
    printf("-----SIZE OVER----------\n");
  }else if(total + size > MAX_CACHE_SIZE){
    total = delete_cache(size, total, path);
    insert_cache(path, response, size);
    printf("-----CACHE MISS SIZE OVER!!-----\n");
  }else{
    total += size;
    printf("-----CACHE MISS!!-----\n");
    insert_cache(path, response, size);
  }
  printf("-----%d------", total);
  Close(clientfd);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // client에게 전송
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, longmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(buf));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}


void parse_request_hdr(char* filename, char* uri, char* path, char* hostname, char* port){
  char* ptr;
  if(strstr(uri, "http://")==NULL){
    strcpy(path,(char*)uri);
    return;
  }else{
    sscanf(uri, "http://%s", hostname);
    ptr = strtok(hostname, ":/");
    ptr = strtok(NULL, ":/");
    if(ptr != NULL) {
      strcpy(port, (char*)ptr);          // ptr이 NULL이 아니면 포트 설정
      ptr = strtok(NULL, ":/");   // 세 번째 호출은 경로
    }
    if(ptr != NULL) {
      strcpy(path,(char*)ptr);          // ptr이 NULL이 아니면 경로 설정
    }

    if(port[0] == '\0'){
      strcpy(port, (char*)server_port);
    }

    if(path[0] == '\0'){
      strcpy(path, (char*)server_addr);
    }else{
      memmove(path + 1, path, strlen(path) + 1); // 기존 문자열을 오른쪽으로 한 칸 이동
      strncpy(path, (char*)server_addr, 1);
    }
    return;
  }
}

void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}
