#include "so_scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>

#define REMOVE 1
#define KEEP 2

typedef void *( *start ) ( void* );

typedef struct thread {
    so_handler *func;
    unsigned int time, prio;
    tid_t id;
    struct thread *next;
    sem_t plan, run;
} Thread, *AT;

typedef struct scheduler {
    unsigned int tq, io, no;
    AT finished;
    AT* vec;
    AT* waiting;
    sem_t wait;
} Scheduler, *AS;

AS scheduler = NULL;
AT running = NULL;
int initiated = 0;

void free_thread ( AT* thread ) {
    pthread_join ( (*thread)->id, NULL );
    sem_destroy ( &(*thread)->plan );
    sem_destroy ( &(*thread)->run );
    free ( (*thread) );
}

AT next_thread ( int removal ) {
    AT next;
    for ( int i = SO_MAX_PRIO; i >= 0; i-- ) {
        if ( scheduler->vec[i] ) {
            next = scheduler->vec[i];
            if ( removal == REMOVE )
                scheduler->vec[i] = scheduler->vec[i]->next;
            return next;
        }
    }
    return NULL;
}

int isSchedEmpty ( ) {

    for ( int i = 0; i <= SO_MAX_PRIO; i++ )
        if ( scheduler->vec[i] != NULL )
            return 0;
    return 1;

}

void add ( AT* L, AT el ) {
    if ( !(*L) ) {
        (*L) = el;
        return;
    }
    AT list = (*L);
    AT prev = list;
    while ( list != NULL ) {
        prev = list;
        list = list->next;
    }
    prev->next = el;
    el->next = list;
}

void* start_thread ( void* arg ) {

    perror ( "ENtered here notice me senpai pls\n" );
    AT thread = ( AT ) arg;
    printf ( "started with %p\n", thread );

    if ( running == NULL ) {
        printf ( "running empty now not: %p\n", thread );
        running = thread;
    }
    else if ( running->prio < thread->prio ) {
        printf ( "switched to a new running thread: %p\n", thread );
        add ( &scheduler->vec[running->prio], running );
        running = thread;
    }
    else {
        printf ( "added new thread %p\n", thread );
        add ( &scheduler->vec[thread->prio], thread );
    }

    sem_post ( &thread->plan );
    printf ( "thread %p has been planned\n", thread );
    sem_wait ( &thread->run );
    printf ( "thread %p is running\n", thread );
    thread->func ( thread->prio );
    printf ( "ended thread: %p\n", thread );
    running->next = scheduler->finished;
    scheduler->finished = running;
    running = next_thread ( REMOVE );
    printf ( "i got next thread: %p\n", running );
    if ( running ) {
        printf ( "now i should run %p\n", running );
        sem_post ( &running->run );
    }
    else if ( isSchedEmpty ( ) ) {
        printf ( "scheduler is empty\n" );
        sem_post ( &scheduler->wait );
    }
    return NULL;

}

int so_init(unsigned int time_quantum, unsigned int io) {

    if ( initiated == 1 || io > SO_MAX_NUM_EVENTS || time_quantum == 0 ) return -1;

    printf ( "%d, %d\n", time_quantum, io );

    scheduler = malloc ( sizeof ( Scheduler ) );
    if ( !scheduler ) return -1;
    scheduler->vec = calloc ( SO_MAX_PRIO + 1, sizeof ( AT ) );
    scheduler->waiting = calloc ( io, sizeof ( AT ) );
    scheduler->tq = time_quantum;
    scheduler->io = io;
    scheduler->no = 0;
    scheduler->finished = NULL;
    if ( !scheduler->vec ) {
        free ( scheduler );
        return -1;
    }
    if ( sem_init ( &scheduler->wait, 0, 0 ) < 0 ) {
        free ( scheduler->vec );
        free ( scheduler );
        return -1;
    }
    for ( int i = 0; i <= SO_MAX_PRIO; i++ )
        scheduler->vec[i] = NULL;
    initiated = 1;
    return 0;
   
}

tid_t so_fork(so_handler *func, unsigned int priority) {

    if ( func == NULL || priority > SO_MAX_PRIO ) return INVALID_TID;

    printf ( "entered so fork with prio: %d\n", priority );

    AT thread = malloc ( sizeof ( Thread ) );
    printf ( "created thread: %p\n", thread );
    thread->func = func;
    thread->time = scheduler->tq;
    thread->prio = priority;
    thread->next = NULL;
    scheduler->no++;
    if ( sem_init ( &thread->plan, 0, 0 ) < 0 ) {
        free ( thread );
        return INVALID_TID;
    }
    if ( sem_init ( &thread->run, 0, 0 ) < 0 ) {
        free ( thread );
        return INVALID_TID;
    }
    if ( pthread_create ( &thread->id, NULL, start_thread, thread ) < 0 ) {
        free ( thread );
        return INVALID_TID;
    }
    AT old = running;
    printf ( "old running is: %p\n", old );
    sem_wait ( &thread->plan );
    printf ( "new running is: %p\n", running );
    if ( running != thread )
        so_exec ( );
    else
        sem_post ( &thread->run );
    if ( old && old != running )
        sem_wait ( &old->run );
    return thread->id;
}

int so_wait(unsigned int io) {
    if ( io >= scheduler->io ) return -1;
    AT old = running;
    add ( &scheduler->waiting[io], running );
    running = next_thread ( REMOVE );
    if ( running ) 
        sem_post ( &running->run );
    sem_wait ( &old->run );
    printf ( "so wait\n" );
    return 0;
}

int so_signal(unsigned int io) {
    if ( io >= scheduler->io ) return -1;
    int count = 0;
    while ( scheduler->waiting[io] ) {
        count++;
        AT list = scheduler->waiting[io];
        add ( &scheduler->vec[list->prio], list );
        scheduler->waiting[io] = scheduler->waiting[io]->next;
    }
    so_exec ( );
    printf ( "so signal\n" );
    return count;
}

void preempt ( AT* current, AT* new ) {

    add ( &scheduler->vec[(*current)->prio], (*current) );
    (*new) = next_thread ( REMOVE );
    printf ( "added %p\n", (*current) );
    (*current) = (*new);
    printf ( "posted %p\n", (*current) );
    sem_post ( &(*current)->run );
}

void so_exec() {
    printf ( "exec xD\n" );
    if ( !running ) return;
    running->time--;
    AT old = running;
    AT next = next_thread ( KEEP );
    printf ( "next thread is: %p\n", next );
    printf ( "%d -> running time\n", running->time );
    if ( running->time == 0 ) {
        running->time = scheduler->tq;
        if ( next ) preempt ( &running, &next );
        printf ( "preempted bcs of time new running is :%p\n", running );
    }
    else if ( next && next->prio > running->prio )
        preempt ( &running, &next );
    if ( old != running ) {
        printf ( "switched running and now it should stop\n" );
        sem_wait ( &old->run );
    }
}

void so_end ( ) {

    if ( initiated == 0 ) return;
    initiated = 0;
    if ( scheduler->no != 0 )
        sem_wait ( &scheduler->wait );
    if ( running )
        free_thread ( &running );
    
    while ( scheduler->finished != NULL ) {
        AT list = scheduler->finished;
        scheduler->finished = scheduler->finished->next;
        free_thread ( &list );
    }
    free ( scheduler->vec );
    free ( scheduler->waiting );
    sem_destroy ( &scheduler->wait );
    free ( scheduler );
    printf ( "ended\n" );
}