#include <pthread.h>

//A node of the unbounded queue
typedef struct Node {
    void *data;
    struct Node *next;
} node_t;

//The unbounded queue
typedef struct Queue {
    node_t *head;
    node_t *tail;
    unsigned int length;
    pthread_mutex_t mux;
    pthread_cond_t empty;
} queue_t;


//Allocates and initializes an unbounded queue
//Returns the queue pointer if successfull, else NULL (sets errno)
queue_t *initQueue();


//Add an element to the unbounded queue
//Returns 0 if successfull, else -1 (sets errno)
int push(queue_t*, void*);


//Eliminates an element from the unbounded queue
//Returns the popped element's data if successful, else NULL (sets errno)
void *pop(queue_t*);


//Deletes the queue
void delQueue(queue_t*);