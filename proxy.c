#include <stdio.h>
#include "csapp.h"
#include "pthread.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
static char *server_port = "80";
static char *server_addr = "/";

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
    Pthread_create(&tid, NULL, thread, connfd);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
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

  /* Read request line and header */
  Rio_readinitb(&rio, fd);           // fd를  rio_t 타입의 rio 버퍼와 연결
  Rio_readlineb(&rio, buf, MAXLINE); // buf에 rio 다음 텍스트 줄을 복사
  printf("Request headers:\n");
  sscanf(buf, "%s %s %s", method, uri, version); // buf의 내용을 각 char 배열에 매핑

  parse_request_hdr(filename, uri, path, hostname, port); // uri parsing 

  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0))
  {
    // GET요청 아니면 에러 발생
    clienterror(fd, method, "501", "Not implemented", "Tiny couldn't find this file");
    return;
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

  Rio_readinitb(&rio_server, clientfd); // 서버에서 받은 내용 저장
  Rio_readinitb(&rio_client, fd);       // 프록시에서 클라이언트에 내용 전송?

  ssize_t n;
  while ((n = Rio_readnb(&rio_server, buf, MAXLINE)) > 0)
  {
    // 목적지 서버로부터 받은 응답을 클라이언트에게 전송
    char *content_length = strstr(buf, "Content-length:");
        if (content_length != NULL) {
            int size = atoi(content_length + strlen("Content-length:"));
            printf("Resource size: %d bytes\n", size);
        }
    Rio_writen(fd, buf, n);
  }
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

}

void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}