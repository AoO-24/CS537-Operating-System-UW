#include "safequeue.h"
#include <stdlib.h>

priority_queue_t *create_queue(int max_size)
{
    priority_queue_t *queue = (priority_queue_t *)malloc(sizeof(priority_queue_t));
    queue->elements = (ClientRequest **)malloc(sizeof(ClientRequest *) * max_size);
    queue->capacity = max_size;
    queue->size = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

void display(priority_queue_t *queue)
{
    printf("\n\tDisplaying queue:\n");
    for (int i = 0; i < queue->size; i++)
    {
        printf("\tElement %d:\n", i);
        printf("\tPath: '%s'\n", queue->elements[i]->path);
        printf("\tPriority: '%d'\n", queue->elements[i]->priority);
        printf("\tDelay: '%d'\n\n", queue->elements[i]->delay);
        printf("\tClient fd: '%d'\n\n", queue->elements[i]->client_fd);
        printf("\tread_buffer: %s\n", queue->elements[i]->read_buffer);
        // printf("\trequest: %s\n", queue->elements[i]->request);
    }
}
void move_down(priority_queue_t *queue)
{
    for (int i = 0; i < queue->size; i++)
    {
        queue->elements[i] = queue->elements[i + 1];
    }
    queue->size--;
}

void add_work(priority_queue_t *queue, ClientRequest *request)
{
    pthread_mutex_lock(&queue->lock);
    printf("Adding work to queue\n");

    // if queue is not full
    if (queue->size < queue->capacity)
    {

        int i = queue->size;
        for (; i > 0 && queue->elements[i - 1]->priority < request->priority; i--)
        {
            queue->elements[i] = queue->elements[i - 1];
        }
        queue->elements[i] = request;
        queue->size++;
        pthread_cond_signal(&queue->cond);
    }
    pthread_mutex_unlock(&queue->lock);
}

ClientRequest *get_work(priority_queue_t *queue)
{
    pthread_mutex_lock(&queue->lock);

    while (queue->size == 0)
    {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }
    ClientRequest *request = queue->elements[0];
    move_down(queue);

    pthread_mutex_unlock(&queue->lock);
    return request;
}