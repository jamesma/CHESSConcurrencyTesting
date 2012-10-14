#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#define THREAD_RUNNING_NOT_WAITING_FOR_LOCK     111
#define THREAD_RUNNING_WAITING_FOR_LOCK         222
#define THREAD_TERMINATED                       333

#define CURRENT_MODE                            0
#define DEBUG_MODE                              1

#define CHESS_EXPLORE_MODE                      1
#define EXPLORE_CHESS_SCHEDULES                 1

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
static void update_track_sync_pts_file();
static void deserialize_track_sync_pts_file(string);
static void chess_switch_thread();
static void synchronization_point();
static void switch_back_to_other_running_thread();

static pthread_t                                        CURRENT_THREAD = 0;
static map<pthread_mutex_t*, pthread_t>                 MUTEX_MAP;
static map<pthread_t, int>                              THREAD_MAP;
static pthread_mutex_t                                  GLOBAL_LOCK = PTHREAD_MUTEX_INITIALIZER;

static const char*                                      TRACK_SYNC_PTS_FILE_NAME = ".tracksyncpts";
static ifstream                                         TRACK_SYNC_PTS_FILE;
static bool                                             FIRST_EXECUTION = false;
static bool                                             THREAD_SWITCHED = false;
static int                                              CURRENT_EXECUTION = -1;
static int                                              TOTAL_EXECUTIONS = -1;
static int                                              SYNC_PTS_ITERATED = 1;
static bool                                             THREAD_WAITING_FOR_JOINEE = false;

static
void* thread_main(void *arg)
{
  CURRENT_THREAD = pthread_self();

  // Sync - Thread created
  synchronization_point();

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

  switch_back_to_other_running_thread();

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

    THREAD_WAITING_FOR_JOINEE = true;

    while ( THREAD_MAP[joinee] != THREAD_TERMINATED ) {}

    THREAD_WAITING_FOR_JOINEE = false;
  }

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

    // Sync - Before mutex is locked
    synchronization_point();
    MUTEX_MAP[mutex] = pthread_self();
  } else {
    if (CURRENT_MODE == DEBUG_MODE) {
      fprintf(stderr, "thread: %u, program lock 2 > ", pthread_self());
      fprintf(stderr, "thread %u now holds program lock %x\n", pthread_self(), mutex);
    }
    THREAD_MAP[pthread_self()] = THREAD_RUNNING_NOT_WAITING_FOR_LOCK;

    // Sync - Before mutex is locked
    synchronization_point();
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

  int ret = original_pthread_mutex_unlock(mutex);

  // This program lock is no longer held by this thread
  if ( pthread_equal(MUTEX_MAP[mutex], pthread_self()) ) {
    MUTEX_MAP[mutex] = 0;
    // Sync - After mutex is released
    synchronization_point();
  }

  return ret;
}

extern "C"
int sched_yield(void)
{
  original_pthread_mutex_unlock(&GLOBAL_LOCK);

  map<pthread_t, int>::iterator it;

  for (it = THREAD_MAP.begin(); it != THREAD_MAP.end() && !THREAD_WAITING_FOR_JOINEE; it++) {
    if ( it->first != pthread_self() && it->second == THREAD_RUNNING_NOT_WAITING_FOR_LOCK ) {
      if (CURRENT_MODE == DEBUG_MODE)
        fprintf(stderr, "\t\t\tsched_yield - thread: %u is yielding to thread: %u\n", pthread_self(), it->first);
      CURRENT_THREAD = it->first;
      break;
    }
  }

  while ( pthread_equal(CURRENT_THREAD, pthread_self()) == 0 ) {}

  original_pthread_mutex_lock(&GLOBAL_LOCK);

  return 0;
}

static 
void switch_back_to_other_running_thread()
{
  map<pthread_t, int>::iterator it;

  for (it = THREAD_MAP.begin(); it != THREAD_MAP.end(); it++) {
    if ( it->first != pthread_self() && it->second != THREAD_TERMINATED ) {
      CURRENT_THREAD = it->first;
      break;
    }
  }
}

static
void synchronization_point()
{
  if (CHESS_EXPLORE_MODE == EXPLORE_CHESS_SCHEDULES) {
    if (FIRST_EXECUTION) {
      TOTAL_EXECUTIONS++;
      fprintf(stderr, "\t\tSynchronization point %d found here\n", TOTAL_EXECUTIONS);
    } else {
      chess_switch_thread();
    }
    update_track_sync_pts_file();
  }
}

static
void chess_switch_thread()
{
  if (SYNC_PTS_ITERATED == CURRENT_EXECUTION && !THREAD_SWITCHED) {
    THREAD_SWITCHED = true;
    CURRENT_EXECUTION++;
    
    // Reset current execution count when total reached
    if (CURRENT_EXECUTION > TOTAL_EXECUTIONS)
      CURRENT_EXECUTION = 1;
    sched_yield();
  }
  SYNC_PTS_ITERATED++;
}

static
void update_track_sync_pts_file()
{
  string newString;
  stringstream temp1;
  stringstream temp2;
  temp1 << CURRENT_EXECUTION;
  newString.append(temp1.str());
  newString.append("/");
  temp2 << TOTAL_EXECUTIONS;
  newString.append(temp2.str());

  if (CURRENT_MODE == DEBUG_MODE)
    fprintf(stderr, "newString: %s\n", newString.c_str());

  // Write back to file
  ofstream outfile;
  outfile.open(TRACK_SYNC_PTS_FILE_NAME);

  if (outfile)
    outfile.write(newString.c_str(), (int)newString.length());

  outfile.close();
}

static
void deserialize_track_sync_pts_file(string str)
{
  int len = str.length();
  int DELIMITER_POS = str.find('/');
  if (DELIMITER_POS == (int)string::npos) {
    fprintf(stderr, "CORRUPT DATA\n");
    return;
  }
  string current = str.substr(0, DELIMITER_POS);
  string total = str.substr(DELIMITER_POS + 1, len);

  // Read data and store them in static global int variables
  CURRENT_EXECUTION = atoi(current.c_str());
  TOTAL_EXECUTIONS = atoi(total.c_str());

  // Check if this is the first execution, set flag if so
  if (CURRENT_EXECUTION == 0 && TOTAL_EXECUTIONS == 0) {
    FIRST_EXECUTION = true;
    CURRENT_EXECUTION = 1;
    if (CURRENT_MODE == DEBUG_MODE)
      fprintf(stderr, ">>>>>>>>>> FIRST EXECUTION <<<<<<<<<<\n");
    return;
  }

  if (CURRENT_MODE == DEBUG_MODE)
    fprintf(stderr, ">>>>>>>>>>>>>>> EXECUTION: %d/%d <<<<<<<<<<<<<<<\n", CURRENT_EXECUTION, TOTAL_EXECUTIONS);
}

static
void initialize_original_functions()
{
  static bool initialized = false;
  if (!initialized) {
    initialized = true;

    if (CHESS_EXPLORE_MODE == EXPLORE_CHESS_SCHEDULES) {
      string input;
      TRACK_SYNC_PTS_FILE.open(TRACK_SYNC_PTS_FILE_NAME);

      if (TRACK_SYNC_PTS_FILE) {
        while (!TRACK_SYNC_PTS_FILE.eof()) {
          TRACK_SYNC_PTS_FILE >> input;
        }
        TRACK_SYNC_PTS_FILE.close();
        deserialize_track_sync_pts_file(input);
      } else {
        FIRST_EXECUTION = true;
        fprintf(stderr, "NO DATA IN .tracksyncpts\n");
      }
    }

    if (CURRENT_MODE == DEBUG_MODE)
      fprintf(stderr, "--------------------------------------------------\n");

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
