#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H
#include <pthread.h>

#include "proxyserver.h"

// typedef struct
// {
//     char *path;    // Request path
//     int client_fd; // Client file descriptor
//     int priority;  // Priority of the request
// } element_t;

typedef struct
{
    ClientRequest **elements; // Array of pointers to requests
    int capacity;             // Maximum size of the queue
    int size;                 // Current size of the queue
    pthread_mutex_t lock;     // Mutex for thread-safe access
    pthread_cond_t cond;      // Condition variable for signaling
} priority_queue_t;

priority_queue_t *create_queue(int max_size);
void add_work(priority_queue_t *queue, ClientRequest *request);
ClientRequest *get_work(priority_queue_t *queue);
void display(priority_queue_t *queue);

#endif