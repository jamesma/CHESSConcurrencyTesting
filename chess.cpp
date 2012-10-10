#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <map>

#define THREAD_RUNNING_NOT_WAITING_FOR_LOCK     111
#define THREAD_RUNNING_WAITING_FOR_LOCK         222
#define THREAD_TERMINATED                       333

#define CURRENT_MODE                            0
#define DEBUG_MODE                              1

using namespace std;

struct Thread_Arg {
    // start_routine is ptr to a func taking one arg, void *, and returns void *
    void* (*start_routine)(void*);
    void* arg;
};

// Pointers to functions
int (*original_pthread_create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*) = NULL;
int (*original_pthread_join)(pthread_t, void**) = NULL;
int (*original_pthread_mutex_lock)(pthread_mutex_t*) = NULL;
int (*original_pthread_mutex_unlock)(pthread_mutex_t*) = NULL;

static void initialize_original_functions();

static pthread_t                                        CURRENT_THREAD = 0;
static map<pthread_mutex_t*, pthread_t>                 MUTEX_MAP;
static map<pthread_t, int>                              THREAD_MAP;
static pthread_mutex_t                                  GLOBAL_LOCK = PTHREAD_MUTEX_INITIALIZER;

static
void* thread_main(void *arg)
{
    struct Thread_Arg thread_arg = *(struct Thread_Arg*)arg;
    free(arg);

    THREAD_MAP[pthread_self()] = THREAD_RUNNING_NOT_WAITING_FOR_LOCK;

    while ( pthread_equal(CURRENT_THREAD, pthread_self()) == 0 ) {}

    // Enter a thread
    original_pthread_mutex_lock(&GLOBAL_LOCK);

    if (CURRENT_MODE == DEBUG_MODE)
        fprintf(stderr, "thread: %u started\n", pthread_self());

    void* ret = thread_arg.start_routine(thread_arg.arg);

    if (CURRENT_MODE == DEBUG_MODE)
        fprintf(stderr, "thread: %u terminated\n", pthread_self());

    THREAD_MAP[pthread_self()] = THREAD_TERMINATED;

    // Exit a thread
    original_pthread_mutex_unlock(&GLOBAL_LOCK);

    return ret;
}

extern "C"
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void*), void *arg)
{
    initialize_original_functions();

    struct Thread_Arg *thread_arg = (struct Thread_Arg*)malloc(sizeof(struct Thread_Arg));
    thread_arg->start_routine = start_routine;
    thread_arg->arg = arg;

    // Lock global lock before entering a new thread
    original_pthread_mutex_lock(&GLOBAL_LOCK);

    int ret = original_pthread_create(thread, attr, thread_main, thread_arg);

    return ret;
}

extern "C"
int pthread_join(pthread_t joinee, void **retval)
{
    initialize_original_functions();

    original_pthread_mutex_unlock(&GLOBAL_LOCK);

    // Select joinee thread if joinee is still running
    if (THREAD_MAP[joinee] != THREAD_TERMINATED) {
        if (CURRENT_MODE == DEBUG_MODE)
            fprintf(stderr, "\t\t\tpthread_join - thread: %u waiting on joinee thread: %u (%d)\n", pthread_self(), joinee, THREAD_MAP[joinee]);
        CURRENT_THREAD = joinee;
    }

    while ( THREAD_MAP[joinee] != THREAD_TERMINATED ) {}
    
    original_pthread_mutex_lock(&GLOBAL_LOCK);

    return original_pthread_join(joinee, retval);
}

extern "C"
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    initialize_original_functions();

    if (MUTEX_MAP[mutex] != 0) {
        if (CURRENT_MODE == DEBUG_MODE)
            fprintf(stderr, "thread: %u, program lock 1 > executing thread: %u and waiting for it\n", pthread_self(), MUTEX_MAP[mutex]);
        original_pthread_mutex_unlock(&GLOBAL_LOCK);

        // Select next available thread to execute, we select the thread holding this lock
        CURRENT_THREAD = MUTEX_MAP[mutex];

        // Wait until program lock is not held by any other threads
        THREAD_MAP[pthread_self()] = THREAD_RUNNING_WAITING_FOR_LOCK;
        while (MUTEX_MAP[mutex] != 0) {}
        CURRENT_THREAD = pthread_self();

        original_pthread_mutex_lock(&GLOBAL_LOCK);

        if (CURRENT_MODE == DEBUG_MODE)
            fprintf(stderr, "thread: %u now holds program lock %x\n", pthread_self(), mutex);
        THREAD_MAP[pthread_self()] = THREAD_RUNNING_NOT_WAITING_FOR_LOCK;
        MUTEX_MAP[mutex] = pthread_self();
    } else {
        if (CURRENT_MODE == DEBUG_MODE) {
            fprintf(stderr, "thread: %u, program lock 2 > ", pthread_self());
            fprintf(stderr, "thread %u now holds program lock %x\n", pthread_self(), mutex);
        }
        THREAD_MAP[pthread_self()] = THREAD_RUNNING_NOT_WAITING_FOR_LOCK;
        MUTEX_MAP[mutex] = pthread_self();
    }

    // Continue execution

    return original_pthread_mutex_lock(mutex);
}

extern "C"
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    initialize_original_functions();

    if (CURRENT_MODE == DEBUG_MODE)
        if (MUTEX_MAP[mutex] != 0)
            fprintf(stderr, "\t\t\tthread: %u frees on program lock %x\n", MUTEX_MAP[mutex], mutex);

    // This program lock is no longer held by this thread
    if ( pthread_equal(MUTEX_MAP[mutex], pthread_self()) )
        MUTEX_MAP[mutex] = 0;

    return original_pthread_mutex_unlock(mutex);
}

extern "C"
int sched_yield(void)
{
    original_pthread_mutex_unlock(&GLOBAL_LOCK);

    map<pthread_t, int>::iterator it;

    pthread_t newThread;

    bool foundThread = false;
    for (it = THREAD_MAP.begin(); it != THREAD_MAP.end(); it++) {
        if ( it->first != pthread_self() && it->second == THREAD_RUNNING_NOT_WAITING_FOR_LOCK ) {
            if (CURRENT_MODE == DEBUG_MODE)
                fprintf(stderr, "\t\t\tsched_yield - thread: %u is yielding to thread: %u\n", pthread_self(), it->first);
            newThread = it->first;
            CURRENT_THREAD = newThread;
            //fprintf(stderr, "CURRENT_THREAD is now: %u (pthread_self is: %u)\n", CURRENT_THREAD, pthread_self());
            foundThread = true;
            break;
        }
    }
    // if (foundThread == false)
    //     CURRENT_THREAD = pthread_self();
    if (foundThread)
        CURRENT_THREAD = newThread;
    //fprintf(stderr, "SANITY CHECK ON CURRENT_THREAD: %u, newThread: %u\n", CURRENT_THREAD, newThread);
    while ( pthread_equal(CURRENT_THREAD, pthread_self()) == 0 ) {}

    original_pthread_mutex_lock(&GLOBAL_LOCK);

    return 0;
}

static
void initialize_original_functions()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        if (CURRENT_MODE == DEBUG_MODE)
            fprintf(stderr, "----------\n");

        original_pthread_create =
            (int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*))dlsym(RTLD_NEXT, "pthread_create");
        original_pthread_join = 
            (int (*)(pthread_t, void**))dlsym(RTLD_NEXT, "pthread_join");
        original_pthread_mutex_lock =
            (int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_lock");
        original_pthread_mutex_unlock =
            (int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    }
}
