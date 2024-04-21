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
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char* method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    printf("Done1 !");
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) { // 서버가 연결을 닫을 때까지 열려 있음.
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}


void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // method: GET, uri: 경로, version: HTTP 1.1
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and header */
  Rio_readinitb(&rio, fd); // fd를  rio_t 타입의 rio 버퍼와 연결
  Rio_readlineb(&rio, buf, MAXLINE); // buf에 rio 다음 텍스트 줄을 복사
  printf("Request headers:\n");
  sscanf(buf, "%s %s %s", method, uri, version); // buf의 내용을 각 char 배열에 매핑

  if(!(strcasecmp(method, "GET") == 0 || strcasecmp(method,"HEAD") == 0)) { // GET요청 아니면 에러 발생
    clienterror(fd, method, "501", "Not implemented", "Tiny couldn't find this file");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // uri에서 filename 파싱해 옴
  if (stat(filename, &sbuf) < 0) { // stat 함수는 주어진 파일의 메타데이터를 가져오는 함수. 파일이 존재하지 않거나 액세스할 수 없으면 호출 실패로 -1을 반환
    clienterror(fd, filename, "404","Not found", "Tiny couldn't find the file");
    printf("DONE2 !!!!!!!!!");
    return;
  } 
  if(is_static) { // 정적 파일 요청
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 파일 및 권한 검사
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      printf("DONE3 !!!!!!!!!");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else {
    if(!(S_ISREG(sbuf.st_mode)) ||!(S_IXUSR & sbuf.st_mode)) { // 파일 및 권한 검사
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];
  
  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
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

void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;
    // strstr: (대상문자열, 검색할문자열) -> 검색된 문자열(대상 문자열) 뒤에 모든 문자열이 나오게 됨
    // uri에서 "cgi-bin"이라는 문자열이 없으면, static content
    if(!strstr(uri, "cgi-bin")){ // Static content
        strcpy(cgiargs, ""); //cgiargs 인자 string을 지운다.
        strcpy(filename, "."); // 상대 리눅스 경로이름으로 변환 ex) '.'
        strcat(filename, uri); // 상대 리눅스 경로이름으로 변환 ex) '.' + '/index.html'
        if (uri[strlen(uri)-1] == '/') // URI가 '/'문자로 끝난다면
            strcat(filename, "home.html"); // 기본 파일 이름인 home.html을 추가한다. -> 11.10과제 adder.html로 변경
        return 1;
    }
    else{ // Dynamic content (cgi-bin이라는 문자열 존재)
        // 모든 CGI 인자들을 추출한다.
        // index: 첫 번째 인자에서 두번째 인자를 찾는다. 찾으면 문자의 위치 포인터를, 못찾으면 NULL을 반환
        ptr = index(uri, '?');
        if(ptr){
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else // ?없으면 빈칸으로 둘게
            strcpy(cgiargs, "");
        // 나머지 URI 부분을 상대 리눅스 파일이름으로 변환
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
        // cgiargs: 123&123
        // filename: ./cgi-bin/adder
  }
}

void serve_static(int fd, char *filename, int filesize, char *method){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  if (strcasecmp(method, "HEAD") == 0)
    return;

  /* send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // filename에 해당하는 여는 파일 디스크립터를 반환
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일 디스크립터는 프로세스 별로 디스크립터 테이블에 저장되므로 공유하려면 메모리에 매핑해줘야함

  srcp = (char *)Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Free(srcp);
}

/* get_filetype - Derive file type from filename */

void get_filetype(char *filename, char *filetype){
  if(strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if(strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");  
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char* method){
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork()==0){
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}