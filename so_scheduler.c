#include "so_scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>

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
    AT* vec;
} Scheduler, *AS;

AS scheduler = NULL;
AT running = NULL;
int initiated = 0;

AT next_thread ( ) {
    AT next;
    for ( int i = SO_MAX_PRIO; i >= 0; i-- ) {
        if ( scheduler->vec[i] ) {
            next = scheduler->vec[i];
            scheduler->vec[i] = scheduler->vec[i]->next;
            return next;
        }
    }
    return NULL;
}

void add ( AT el ) {

    int poz = el->prio;
    AT list = scheduler->vec[poz];
    AT prev = list;
    while ( list != NULL ) {
        prev = list;
        list = list->next;
    }
    if ( prev == NULL ) {
        scheduler->vec[poz] = el;
        return;
    }
    prev->next = el;
}

void* start_thread ( void* arg ) {

    perror ( "ENtered here notice me senpai pls\n" );
    AT thread = ( AT ) arg;
    printf ( "started with %p\n", thread );

    if ( running == NULL ) {
        running = thread;
        sem_post ( &thread->run );
    }
    else if ( running->prio < thread->prio ) {
        add ( running );
        running = thread;
    }
    else
        add ( thread );

    sem_post ( &thread->plan );
    printf ( "thread %p has been planned\n", thread );
    sem_wait ( &thread->run );
    printf ( "thread %p is running\n", thread );
    thread->func ( thread->prio );
    printf ( "ended thread: %p\n", thread );
    return NULL;

}

int so_init(unsigned int time_quantum, unsigned int io) {

    if ( initiated == 1 || io > SO_MAX_NUM_EVENTS || time_quantum == 0 ) return -1;

    scheduler = malloc ( sizeof ( Scheduler ) );
    if ( !scheduler ) return -1;
    scheduler->vec = calloc ( SO_MAX_PRIO + 1, sizeof ( AT ) );
    scheduler->tq = time_quantum;
    scheduler->io = io;
    scheduler->no = 0;
    if ( !scheduler->vec ) {
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
    sem_wait ( &thread->plan );
    if ( running != thread )
        so_exec ( );
    return thread->id;
}

int so_wait(unsigned int io) {
    printf ( "so wait\n" );
    return 0;
}

int so_signal(unsigned int io) {
    printf ( "so signal\n" );
    return 0;
}

void preempt ( AT* current, AT* new ) {

    add ( (*current) );
    sem_wait ( &((*current)->run) );
    (*current) = (*new);
    sem_post ( &((*current)->run) );
}

void so_exec() {
    printf ( "exec xD\n" );
    if ( !running ) return;

    AT next = next_thread ( );
    printf ( "next thread is: %p\n", next );
    running->time--;
    if ( running->time == 0 ) {
        running->time = scheduler->tq;
        if ( next ) preempt ( &running, &next );
    }
    else if ( next && next->prio > running->prio )
        preempt ( &running, &next );
    else if ( !next )
        add ( next );
}

void free_thread ( AT* thread ) {
    pthread_join ( (*thread)->id, NULL );
    sem_destroy ( &(*thread)->plan );
    sem_destroy ( &(*thread)->run );
}

void so_end ( ) {

    if ( initiated == 0 ) return;
    initiated = 0;
    if ( scheduler->no != 0 )
    
    if ( &running )
        free_thread ( running );
    free ( scheduler->vec );
    free ( scheduler );
    printf ( "ended\n" );
}