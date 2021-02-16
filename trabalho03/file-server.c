#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#define REQ_GET "GET"
#define REQ_CREATE "CREATE"
#define REQ_APPEND "APPEND"
#define REQ_REMOVE "REMOVE"
#define MAX_REQ_LEN 1000
#define FBLOCK_SIZE 4000

#define COD_OK0_GET "OK-0 method GET OK\n"
#define COD_OK1_FILE "OK-1 File open OK\n"
#define COD_OK2_CREATE "OK-1 method CREATE OK\n"
#define COD_OK3_APPEND "OK-2 method APPEND OK\n"
#define COD_OK4_REMOVE "OK-3 method REMOVE OK\n"

#define COD_ERROR_0_METHOD "ERROR-0 Method not supported\n"
#define COD_ERROR_1_FILE "ERROR-1 File could not be opened\n"
#define COD_ERROR_2_FILE "ERROR-2 File already exists\n"
#define COD_ERROR_3_FILE "ERROR-3 File not found or could not be opened\n"
#define COD_ERROR_4_FILE "ERROR-4 Can't append an empty text\n"
#define COD_ERROR_5_FILE "ERROR-5 File not found or could not be removed\n"

struct req_t
{
    char method[128];
    char filename[256];
    char text[256];
};

typedef struct req_t req_t;

struct conn_params
{
    pthread_t tid;
    int sc;
    struct sockaddr_in caddr;
};

typedef struct conn_params conn_params;

void get_request(req_t *r, char *rstr)
{
    bzero(r, sizeof(req_t));
    sscanf(rstr, "%s %s %s", r->method, r->filename, r->text);
}

void send_file(int sockfd, req_t r)
{
    int fd, nr;
    unsigned char fbuff[FBLOCK_SIZE];
    fd = open(r.filename, O_RDONLY, S_IRUSR);
    if (fd == -1)
    {
        perror("open");
        send(sockfd, COD_ERROR_1_FILE, strlen(COD_ERROR_1_FILE), 0);
        return;
    }
    send(sockfd, COD_OK1_FILE, strlen(COD_OK1_FILE), 0);
    do
    {
        bzero(fbuff, FBLOCK_SIZE);
        nr = read(fd, fbuff, FBLOCK_SIZE);
        if (nr > 0)
        {
            send(sockfd, fbuff, nr, 0);
        }
    } while (nr > 0);

    close(fd);
    return;
}

void create_file(int sockfd, req_t r)
{
    int fd = open(r.filename, O_RDONLY, S_IRUSR);
    if (fd == -1)
    {
        fd = open(r.filename, O_WRONLY | O_CREAT, S_IRUSR);
        close(fd);
        return;
    }
    perror("create");
    send(sockfd, COD_ERROR_2_FILE, strlen(COD_ERROR_2_FILE), 0);
    close(fd);
    return;
}

void append_to_file(int sockfd, req_t r)
{
    if (strlen(r.text) == 0)
    {
        send(sockfd, COD_ERROR_4_FILE, strlen(COD_ERROR_4_FILE), 0);
        return;
    }

    int fd = open(r.filename, O_WRONLY | O_APPEND, S_IRUSR);
    if (fd == -1)
    {
        perror("append");
        send(sockfd, COD_ERROR_3_FILE, strlen(COD_ERROR_3_FILE), 0);
        close(fd);
        return;
    }

    write(fd, &r.text, strlen(r.text));
    close(fd);
    return;
}

void remove_file(int sockfd, req_t r)
{
    int fd = open(r.filename, O_WRONLY | O_TRUNC, S_IRUSR);
    if (fd == -1)
    {
        perror("remove");
        send(sockfd, COD_ERROR_5_FILE, strlen(COD_ERROR_5_FILE), 0);
        close(fd);
        return;
    }
    close(fd);
    return;
}

void proc_request(int sockfd, req_t r)
{
    if (strcmp(r.method, REQ_GET) == 0)
    {
        send(sockfd, COD_OK0_GET, strlen(COD_OK0_GET), 0);
        send_file(sockfd, r);
    }
    else if (strcmp(r.method, REQ_CREATE) == 0)
    {
        send(sockfd, COD_OK2_CREATE, strlen(COD_OK2_CREATE), 0);
        create_file(sockfd, r);
    }
    else if (strcmp(r.method, REQ_APPEND) == 0)
    {
        send(sockfd, COD_OK3_APPEND, strlen(COD_OK3_APPEND), 0);
        append_to_file(sockfd, r);
    }
    else if (strcmp(r.method, REQ_REMOVE) == 0)
    {
        send(sockfd, COD_OK4_REMOVE, strlen(COD_OK4_REMOVE), 0);
        remove_file(sockfd, r);
    }
    else
    {
        send(sockfd, COD_ERROR_0_METHOD, strlen(COD_ERROR_0_METHOD), 0);
    }
}

void *connect_client(void *args)
{
    conn_params conn = *(conn_params *)args;
    int nr;
    char request[MAX_REQ_LEN];
    req_t req;
    printf("Conectado com cliente %s:%d\n", inet_ntoa(conn.caddr.sin_addr), ntohs(conn.caddr.sin_port));
    bzero(request, MAX_REQ_LEN);
    nr = recv(conn.sc, request, MAX_REQ_LEN, 0);
    if (nr > 0)
    {
        get_request(&req, request);
        printf(" method: %s\n filename: %s\n", req.method, req.filename);
        proc_request(conn.sc, req);
    }
    close(conn.sc);
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Uso %s <porta>n\n", argv[0]);
        return 0;
    }
    int sl;
    sl = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sl == -1)
    {
        perror("socket");
        return 0;
    }
    struct sockaddr_in saddr;
    saddr.sin_port = htons(atoi(argv[1]));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sl, (struct sockaddr *)&saddr,
             sizeof(struct sockaddr_in)) == -1)
    {
        perror("bind");
        return 0;
    }
    if (listen(sl, 1000) == -1)
    {
        perror("listen");
        return 0;
    }

    struct sockaddr_in caddr;
    int addr_len;
    int sc;
    struct conn_params conn[MAX_REQ_LEN];
    while (1)
    {
        addr_len = sizeof(struct sockaddr_in);
        sc = accept(sl, (struct sockaddr *)&caddr, (socklen_t *)&addr_len);
        if (sc == -1)
        {
            perror("accept");
            continue;
        }
        conn[sc].sc = sc;
        conn[sc].caddr = caddr;
        pthread_create(&conn[sc].tid, NULL, connect_client, &conn[sc]);
    }
    int i = 0;
    while (i < MAX_REQ_LEN)
    {
        pthread_join(conn[i].tid, NULL);
    }
    close(sl);
    return 0;
}