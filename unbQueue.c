#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "unbQueue.h"

queue_t *initQueue() {
    queue_t *q=(queue_t*)malloc(sizeof(queue_t));
    if (q!=NULL) {
        q->head=(node_t*)malloc(sizeof(node_t));
        if (q->head==NULL) {
            return NULL;
        }
        (q->head)->data=NULL;
        (q->head)->next=NULL;
        q->tail=q->head;
        q->length=0;
        if (pthread_mutex_init(&(q->mux), NULL)!=0) {
            return NULL;
        }
        if (pthread_cond_init(&(q->empty), NULL)!=0) {
            pthread_mutex_destroy(&(q->mux));
            return NULL;
        }
    } else {
    }
    return q;
}

int push(queue_t *q, void *data) {
    if ((q==NULL) || (data==NULL)) {
        errno=EINVAL;
        return -1;
    }
    node_t *node=(node_t*)malloc(sizeof(node_t));
    if (node==NULL) {
        return -1;
    }
    node->data=data;
    node->next=NULL;
    if (pthread_mutex_lock(&(q->mux))!=0) {
        perror("FATAL ERROR ON LOCKING MUTEX, exiting to avoid unexpected beheviour");
        exit(EXIT_FAILURE);
    }
    (q->tail)->next=node;
    q->tail=node;
    (q->length)++;
    if (pthread_cond_signal(&(q->empty))!=0) {
        perror("FATAL ERROR ON CONDITION VARIABLE SIGNAL, exiting to avoid unexpected beheviour");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_unlock(&(q->mux))!=0) {
        perror("FATAL ERROR ON UNLOCKING MUTEX, exiting to avoid unexpected beheviour");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void *pop(queue_t *q) {
    if (q==NULL) {
        errno=EINVAL;
        return NULL;
    }
    if (pthread_mutex_lock(&(q->mux))!=0) {
        perror("FATAL ERROR ON LOCKING MUTEX, exiting to avoid unexpected beheviour");
        exit(EXIT_FAILURE);
    }
    while (q->head==q->tail) { //queue is empty
        if (pthread_cond_wait(&(q->empty), &(q->mux))!=0) {
            perror("FATAL ERROR ON WAITING FOR CONDITION VARIABLE, exiting to avoid unexpected beheviour");
            exit(EXIT_FAILURE);
        }
    }
    node_t *n=q->head;
    void *data=((q->head)->next)->data;
    q->head=(q->head)->next;
    (q->length)--;
    if (pthread_mutex_unlock(&(q->mux))!=0) {
        perror("FATAL ERROR ON UNLOCKING MUTEX, exiting to avoid unexpected beheviour");
        exit(EXIT_FAILURE);
    }
    free(n);
    return data;
}

void delQueue(queue_t *q) {
    if (q!=NULL) {
        while (q->head!=q->tail) { //queue not empty
            node_t *n=q->head;
            q->head=(q->head)->next;
            free(n);
        }
        free(q->head);
        if (&(q->mux)!=NULL) {
            pthread_mutex_destroy(&(q->mux));
        }
        if (&(q->empty)!=NULL) {
            pthread_cond_destroy(&(q->empty));
        }
        free(q);
    }
    return;
}