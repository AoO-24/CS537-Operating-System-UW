#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "proxyserver.h"
#include "safequeue.h"

// 1111

/*
 * Constants
 */
#define RESPONSE_BUFSIZE 10000

/*
 * Global configuration variables.
 * Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int num_listener;
int *listener_ports;
int num_workers;
char *fileserver_ipaddr;
int fileserver_port;
int max_queue_size;

priority_queue_t *priorityQueue; // Priority queue
pthread_mutex_t queueMutex;      // Mutex for priority queue access

void parse_client_request(int fd, ClientRequest *result);

void http_start_response(int fd, int status_code)
{
    dprintf(fd, "HTTP/1.0 %d %s\r\n", status_code,
            http_get_response_message(status_code));
}

void http_send_header(int fd, char *key, char *value)
{
    dprintf(fd, "%s: %s\r\n", key, value);
}

void http_end_headers(int fd)
{
    dprintf(fd, "\r\n");
}

void http_send_string(int fd, char *data)
{
    http_send_data(fd, data, strlen(data));
}

int http_send_data(int fd, char *data, size_t size)
{
    ssize_t bytes_sent;
    while (size > 0)
    {
        bytes_sent = write(fd, data, size);
        if (bytes_sent < 0)
            return -1; // Indicates a failure
        size -= bytes_sent;
        data += bytes_sent;
    }
    return 0; // Indicate success
}

void http_fatal_error(char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(ENOBUFS);
}

#define LIBHTTP_REQUEST_MAX_SIZE 8192

char *http_get_response_message(int status_code)
{
    switch (status_code)
    {
    case 100:
        return "Continue";
    case 200:
        return "OK";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 304:
        return "Not Modified";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    default:
        return "Internal Server Error";
    }
}

void parse_client_request(int fd, ClientRequest *result)
{

    char *read_buffer = malloc(LIBHTTP_REQUEST_MAX_SIZE + 1);

    if (!read_buffer)
        http_fatal_error("Malloc failed");

    int bytes_read = read(fd, read_buffer, LIBHTTP_REQUEST_MAX_SIZE);
    read_buffer[bytes_read] = '\0'; /* Always null-terminate. */
    printf("read buffer %s\n\n", read_buffer);

    result->read_buffer = malloc(LIBHTTP_REQUEST_MAX_SIZE + 1);
    strcpy(result->read_buffer, read_buffer);

    int delay = -1;
    int priority = -1;
    char *path = NULL;

    int is_first = 1;
    size_t size;

    char *token = strtok(read_buffer, "\r\n");
    while (token != NULL)
    {
        size = strlen(token);
        if (is_first)
        {
            is_first = 0;
            // get path
            char *s1 = strstr(token, " ");
            char *s2 = strstr(s1 + 1, " ");
            size = s2 - s1 - 1;
            path = strndup(s1 + 1, size);

            if (strcmp(GETJOBCMD, path) == 0)
            {
                break;
            }
            else
            {
                // get priority
                s1 = strstr(path, "/");
                s2 = strstr(s1 + 1, "/");
                size = s2 - s1 - 1;
                char *p = strndup(s1 + 1, size);
                priority = atoi(p);
            }
        }
        else
        {
            char *value = strstr(token, ":");
            if (value)
            {
                size = value - token - 1; // -1 for space
                if (strncmp("Delay", token, size) == 0)
                {
                    delay = atoi(value + 2); // skip `: `
                }
            }
        }
        token = strtok(NULL, "\r\n");
    }

    printf("\n\tParsed HTTP request:\n");
    printf("\tPath: '%s'\n", path);
    printf("\tPriority: '%d'\n", priority);
    printf("\tDelay: '%d'\n\n", delay);

    // result->request = malloc(strlen(read_buffer) + 1);
    // strcpy(result->request, read_buffer);
    result->path = path;
    result->priority = priority;
    result->delay = delay;
    result->bytes_read = bytes_read;
    result->client_fd = fd;
    printf("read_buffer %s\n", read_buffer);
    printf("result->path %s\n", result->path);
    printf("result->priority %d\n", result->priority);
    printf("result->delay %d\n", result->delay);
    printf("result->bytes_read %d\n", result->bytes_read);

    free(read_buffer);
    return;
}

void free_client_request(ClientRequest *request)
{
    if (request)
    {
        if (request->path)
        {
            free(request->path);
        }
        free(request);
    }
}

void send_error_response(int client_fd, status_code_t err_code, char *err_msg)
{
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    http_send_string(client_fd, buf);
    return;
}

/*
 * forward the client request to the fileserver and
 * forward the fileserver response to the client
 */
void serve_request(ClientRequest *request)
{
    printf("serve_request\n");

    int client_fd = request->client_fd;
    // create a fileserver socket
    int fileserver_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fileserver_fd == -1)
    {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        exit(errno);
    }

    // create the full fileserver address
    struct sockaddr_in fileserver_address;
    fileserver_address.sin_addr.s_addr = inet_addr(fileserver_ipaddr);
    fileserver_address.sin_family = AF_INET;
    fileserver_address.sin_port = htons(fileserver_port);

    // connect to the fileserver
    int connection_status = connect(fileserver_fd, (struct sockaddr *)&fileserver_address,
                                    sizeof(fileserver_address));
    if (connection_status < 0)
    {
        // failed to connect to the fileserver
        printf("Failed to connect to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");
        return;
    }

    // successfully connected to the file server
    char *buffer = (char *)malloc(RESPONSE_BUFSIZE * sizeof(char));

    //  forward the client request to the fileserver
    //  int bytes_read = read(client_fd, buffer, RESPONSE_BUFSIZE);
    // int bytes_read = ;
    printf("read_buffer %s\n", request->read_buffer);
    int ret = http_send_data(fileserver_fd, request->read_buffer, request->bytes_read);
    printf("ret %d\n", ret);
    if (ret < 0)
    {
        printf("Failed to send request to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");
    }
    else
    {
        // forward the fileserver response to the client
        while (1)
        {
            printf("recv\n");
            int bytes_read = recv(fileserver_fd, buffer, RESPONSE_BUFSIZE - 1, 0);
            // here
            printf("bytes_read %d\n", bytes_read);
            if (bytes_read <= 0) // fileserver_fd has been closed, break
                break;
            printf("http_send_data\n");
            ret = http_send_data(client_fd, buffer, bytes_read);
            printf("sent successfully\n");
            if (ret < 0)
            { // write failed, client_fd has been closed
                break;
            }
        }
    }

    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);
    shutdown(client_fd, SHUT_WR);
    close(client_fd);

    // Free resources and exit
    free(buffer);
}

// int server_fd;
/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *server_fd, int portNumber)
{

    // create a socket to listen
    *server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (*server_fd == -1)
    {
        perror("Failed to create a new socket");
        exit(errno);
    }

    // manipulate options for the socket
    int socket_option = 1;
    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1)
    {
        perror("Failed to set socket options");
        exit(errno);
    }

    int proxy_port = listener_ports[portNumber];
    // create the full address of this proxyserver
    struct sockaddr_in proxy_address;
    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = INADDR_ANY;
    proxy_address.sin_port = htons(proxy_port); // listening port

    // bind the socket to the address and port number specified in
    if (bind(*server_fd, (struct sockaddr *)&proxy_address,
             sizeof(proxy_address)) == -1)
    {
        perror("Failed to bind on socket");
        exit(errno);
    }

    // starts waiting for the client to request a connection
    if (listen(*server_fd, 1024) == -1)
    {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", proxy_port);

    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_fd;
    while (1)
    {
        client_fd = accept(*server_fd,
                           (struct sockaddr *)&client_address,
                           (socklen_t *)&client_address_length);
        if (client_fd < 0)
        {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);

        // serve_request(client_fd);
        ClientRequest *request = malloc(sizeof(ClientRequest));
        parse_client_request(client_fd, request);
        request->client_fd = client_fd;
        if (strstr(request->path, "GetJob"))
        {
            // check if queue is empty
            if (priorityQueue->size == 0)
            {
                send_error_response(client_fd, QUEUE_EMPTY, "Queue is empty");
                close(client_fd);
            }
            else
            {
                ClientRequest *job = get_work(priorityQueue);

                send_error_response(client_fd, OK, job->read_buffer + 4);
                close(client_fd);
            }
        }
        else if (strstr(request->read_buffer, "GET"))
        {
            // check if queue is full
            if (priorityQueue->size == max_queue_size)
            {
                send_error_response(client_fd, QUEUE_FULL, "Queue is full");
                close(client_fd);
            }
            else
            {
                // request->client_fd = client_fd;
                add_work(priorityQueue, request);
                display(priorityQueue); // todo:delete
            }
        }
        else
        {
            // printf("request->request %s\n", request->request);       // todo:delete
            // printf("request->path %s\n", request->path);             // todo:delete
            // printf("request->priority %d\n", request->priority);     // todo:delete
            // printf("request->delay %d\n", request->delay);           // todo:delete
            // printf("request->bytes_read %d\n", request->bytes_read); // todo:delete
            send_error_response(client_fd, BAD_REQUEST, "Bad request");
        }

        // // close the connection to the client
        // shutdown(client_fd, SHUT_WR);
        // close(client_fd);
    }

    shutdown(*server_fd, SHUT_RDWR);
    close(*server_fd);
    shutdown(client_fd, SHUT_WR);
    close(client_fd);
}

void handle_getjob_request(int client_fd)
{
    pthread_mutex_lock(&priorityQueue->lock);

    if (priorityQueue->size == 0)
    {
        pthread_mutex_unlock(&priorityQueue->lock);
        send_error_response(client_fd, QUEUE_EMPTY, "Queue is empty");
    }
    else
    {
        ClientRequest *job = get_work(priorityQueue);
        pthread_mutex_unlock(&priorityQueue->lock);

        http_start_response(client_fd, OK);
        http_send_header(client_fd, "Content-Type", "text/plain");
        http_end_headers(client_fd);
        http_send_string(client_fd, job->path); // Send the path of the job

        // Free the job after sending
        free(job->path);
        free(job);
    }
}

/*
 * Default settings for in the global configuration variables
 */
void default_settings()
{
    num_listener = 1;
    listener_ports = (int *)malloc(num_listener * sizeof(int));
    listener_ports[0] = 8000;

    num_workers = 1;

    fileserver_ipaddr = "127.0.0.1";
    fileserver_port = 3333;

    max_queue_size = 100;
}

void print_settings()
{
    printf("\t---- Setting ----\n");
    printf("\t%d listeners [", num_listener);
    for (int i = 0; i < num_listener; i++)
        printf(" %d", listener_ports[i]);
    printf(" ]\n");
    printf("\t%d workers\n", num_listener);
    printf("\tfileserver ipaddr %s port %d\n", fileserver_ipaddr, fileserver_port);
    printf("\tmax queue size  %d\n", max_queue_size);
    printf("\t  ----\t----\t\n");
}

void signal_callback_handler(int signum)
{
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    for (int i = 0; i < num_listener; i++)
    {
        // if (close(server_fd) < 0)
        //     perror("Failed to close server_fd (ignoring)\n");
    }
    free(listener_ports);
    exit(0);
}

char *USAGE =
    "Usage: ./proxyserver [-l 1 8000] [-n 1] [-i 127.0.0.1 -p 3333] [-q 100]\n";

void exit_with_usage()
{
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

void *worker_thread(void *arg)
{
    while (1)
    {
        ClientRequest *currentJob = get_work(priorityQueue);
        display(priorityQueue);
        printf("workers thread\n");
        int delay = currentJob->delay;
        if (delay > 0)
        {
            sleep(delay);
        }

        serve_request(currentJob);
    }
    return NULL;
}

void *listener(void *arg)
{
    int index = *(int *)arg;
    int server_fd;
    serve_forever(&server_fd, index);

    return NULL;
}

int main(int argc, char **argv)
{
    signal(SIGINT, signal_callback_handler);

    /* Default settings */
    default_settings();

    int i;
    for (i = 1; i < argc; i++)
    {
        if (strcmp("-l", argv[i]) == 0)
        {
            num_listener = atoi(argv[++i]);
            free(listener_ports);
            listener_ports = (int *)malloc(num_listener * sizeof(int));
            for (int j = 0; j < num_listener; j++)
            {
                listener_ports[j] = atoi(argv[++i]);
                printf("Listener port is %d\n", listener_ports[j]);
            }
        }
        else if (strcmp("-w", argv[i]) == 0)
        {
            num_workers = atoi(argv[++i]);
        }
        else if (strcmp("-q", argv[i]) == 0)
        {
            max_queue_size = atoi(argv[++i]);
        }
        else if (strcmp("-i", argv[i]) == 0)
        {
            fileserver_ipaddr = argv[++i];
        }
        else if (strcmp("-p", argv[i]) == 0)
        {
            fileserver_port = atoi(argv[++i]);
        }
        else
        {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }
    print_settings();

    // serve_forever(&server_fd);

    // Initialize SafeQueue
    priorityQueue = create_queue(max_queue_size);

    // Initialize listener threads
    // Inside main function
    pthread_t *listener_threads = malloc(num_listener * sizeof(pthread_t));
    for (int i = 0; i < num_listener; i++)
    {
        int *index = malloc(sizeof(int));
        *index = i;
        if (pthread_create(&listener_threads[i], NULL, listener, index) != 0)
        {
            // Handle thread creation failure
            perror("Failed to create listener thread");
        }
    }
    pthread_t *worker_threads = malloc(num_workers * sizeof(pthread_t));
    if (!worker_threads)
    {
        perror("Failed to allocate memory for worker threads");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_workers; ++i)
    {
        if (pthread_create(&worker_threads[i], NULL, worker_thread, NULL) != 0)
        {
            perror("Failed to create worker thread");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_listener; i++)
    {
        if (pthread_join(listener_threads[i], NULL) != 0)
        {
            perror("Failed to join listener thread");
        }
    }

    for (int i = 0; i < num_workers; i++)
    {
        if (pthread_join(worker_threads[i], NULL) != 0)
        {
            perror("Failed to join worker thread");
        }
    }

    free(worker_threads);
    free(listener_threads);

    // Clean Up
    // destroy_safe_queue(priorityQueue);todo
    free(priorityQueue);
    free(listener_ports);

    return EXIT_SUCCESS;
}