#include "ring_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LogError(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)
#define LogInfo(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)

int rb_init(rb_t *rb, size_t capacity, size_t element_size)
{
    size_t i;

    if (capacity == 0 || element_size == 0) {
        LogError("Invalid parameters: capacity=%zu, element_size=%zu", 
                capacity, element_size);
        return -1;
    }
    
    rb->capacity = capacity + 1;
    rb->element_size = element_size;
    
    rb->buffer = (void **)malloc(rb->capacity * sizeof(void *));
    if (rb->buffer == NULL) {
        LogError("Failed to allocate buffer memory");
        return -1;
    }
    
    for (i = 0; i < rb->capacity; i++) {
        rb->buffer[i] = NULL;
    }
    
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    
    if (pthread_mutex_init(&rb->mutex, NULL) != 0) {
        LogError("Failed to initialize mutex");
        free(rb->buffer);
        return -1;
    }
    
    if (pthread_cond_init(&rb->not_empty, NULL) != 0) {
        LogError("Failed to initialize not_empty condition");
        pthread_mutex_destroy(&rb->mutex);
        free(rb->buffer);
        return -1;
    }
    
    if (pthread_cond_init(&rb->not_full, NULL) != 0) {
        LogError("Failed to initialize not_full condition");
        pthread_cond_destroy(&rb->not_empty);
        pthread_mutex_destroy(&rb->mutex);
        free(rb->buffer);
        return -1;
    }
    
    return 0;
}

void rb_destroy(rb_t *rb)
{
    size_t i;

    pthread_mutex_lock(&rb->mutex);
    
    for (i = 0; i < rb->capacity; i++) {
        if (rb->buffer[i] != NULL) {
            free(rb->buffer[i]);
            rb->buffer[i] = NULL;
        }
    }
    
    pthread_mutex_unlock(&rb->mutex);
    
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);
    
    free(rb->buffer);
    rb->buffer = NULL;
}

bool rb_is_empty(rb_t *rb)
{
    bool empty;

    pthread_mutex_lock(&rb->mutex);
    empty = (rb->head == rb->tail);
    pthread_mutex_unlock(&rb->mutex);

    return empty;
}

bool rb_is_full(rb_t *rb)
{
    bool full;

    pthread_mutex_lock(&rb->mutex);
    full = ((rb->tail + 1) % rb->capacity == rb->head);
    pthread_mutex_unlock(&rb->mutex);

    return full;
}

size_t rb_size(rb_t *rb)
{
    size_t size;

    pthread_mutex_lock(&rb->mutex);
    size = rb->count;
    pthread_mutex_unlock(&rb->mutex);

    return size;
}

size_t rb_capacity(rb_t *rb)
{
    return rb->capacity - 1;
}

int rb_push_blocking(rb_t *rb, const void *data)
{
    void *new_data;
    int ret = 1;

    pthread_mutex_lock(&rb->mutex);
    
    while ((rb->tail + 1) % rb->capacity == rb->head) {
        pthread_cond_wait(&rb->not_full, &rb->mutex);
    }
    
    new_data = malloc(rb->element_size);
    if (new_data == NULL) {
        LogError("Failed to allocate memory for new data");
        ret = -1;
        goto unlock;
    }
    
    memcpy(new_data, data, rb->element_size);
    
    if (rb->buffer[rb->tail] != NULL) {
        free(rb->buffer[rb->tail]);
    }
    
    rb->buffer[rb->tail] = new_data;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    
    pthread_cond_signal(&rb->not_empty);

unlock:
    pthread_mutex_unlock(&rb->mutex);
    return ret;
}

int rb_push_nonblocking(rb_t *rb, const void *data)
{
    void *new_data;
    int ret = 1;

    pthread_mutex_lock(&rb->mutex);
    
    if ((rb->tail + 1) % rb->capacity == rb->head) {
        ret = 0;
        goto unlock;
    }
    
    new_data = malloc(rb->element_size);
    if (new_data == NULL) {
        LogError("Failed to allocate memory for new data");
        ret = -1;
        goto unlock;
    }
    
    memcpy(new_data, data, rb->element_size);
    
    if (rb->buffer[rb->tail] != NULL) {
        free(rb->buffer[rb->tail]);
    }
    
    rb->buffer[rb->tail] = new_data;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    
    pthread_cond_signal(&rb->not_empty);

unlock:
    pthread_mutex_unlock(&rb->mutex);
    return ret;
}

int rb_pop_blocking(rb_t *rb, void *output)
{
    int ret = 1;

    pthread_mutex_lock(&rb->mutex);
    
    while (rb->head == rb->tail) {
        pthread_cond_wait(&rb->not_empty, &rb->mutex);
    }
    
    if (output != NULL && rb->buffer[rb->head] != NULL) {
        memcpy(output, rb->buffer[rb->head], rb->element_size);
    }
    
    if (rb->buffer[rb->head] != NULL) {
        free(rb->buffer[rb->head]);
        rb->buffer[rb->head] = NULL;
    }
    
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    
    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
    
    return ret;
}

int rb_pop_nonblocking(rb_t *rb, void *output)
{
    int ret = 1;

    pthread_mutex_lock(&rb->mutex);
    
    if (rb->head == rb->tail) {
        ret = 0;
        goto unlock;
    }
    
    if (output != NULL && rb->buffer[rb->head] != NULL) {
        memcpy(output, rb->buffer[rb->head], rb->element_size);
    }
    
    if (rb->buffer[rb->head] != NULL) {
        free(rb->buffer[rb->head]);
        rb->buffer[rb->head] = NULL;
    }
    
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    
    pthread_cond_signal(&rb->not_full);

unlock:
    pthread_mutex_unlock(&rb->mutex);
    return ret;
}

int rb_peek(rb_t *rb, void *output)
{
    int ret = 1;

    pthread_mutex_lock(&rb->mutex);
    
    if (rb->head == rb->tail) {
        ret = 0;
        goto unlock;
    }
    
    if (output != NULL && rb->buffer[rb->head] != NULL) {
        memcpy(output, rb->buffer[rb->head], rb->element_size);
    }

unlock:
    pthread_mutex_unlock(&rb->mutex);
    return ret;
}