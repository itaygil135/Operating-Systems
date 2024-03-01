#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#include <queue>
#include <iostream>
#include <list>
#include "uthreads.h"
#include <istream>
#include <csetjmp>
#include <setjmp.h>
#include <map>
#include <algorithm>
#include <string>
#include <signal.h>
#include <csignal>
#include <sys/time.h>
#include <set>

#define FAILED -1
#define GOOD 0

#define MILLION 1000000

using namespace std;
typedef void (*thread_entry_point)(void);
// variables for managing process id

typedef class Thread *T;
int s_running_thread_id;
static int next_tid = 1;
static std::set<int>  tids_for_reuse_set{};
static std::queue<int> wants_to_run_queue;
static std::map<int, T> s_thread_map;
static int s_quantum_usecs = 0;
static int s_total_system_quantum = 0;



enum State {
	RUNNING, BLOCKED, READY
};

enum Reason {
    SWITCH_REASON_INIT, SWITCH_REASON_TERMINATE, SWITCH_REASON_BLOCK, SWITCH_REASON_TIMER
};

// ----- FUNCTIONS DECLARATIONS  --------

static void context_switch(int switch_reason);

//void set_timer(void);

void timer_handler(int sig);

int get_next_tid();

//static void reuse_tid(int tid);

//static void err_print(const std::string &msg);

void remove_from_queue(int tid);

int is_id_valid(int tid);


struct Thread {

    size_t tid;
    thread_entry_point entry_point;
    int state_t;
    int running_quantum_counter;
    int sleeping_quantum_counter;
    char *p_stack;

    sigjmp_buf env{};

    Thread(size_t tid, thread_entry_point entry_point) :
            tid(tid), entry_point(entry_point), state_t(READY) {
        if (tid > 0) {
            sigsetjmp(env, 1);
            p_stack = new char[STACK_SIZE];
            if (p_stack == nullptr) {
                err_print("Allocation failed");
                exit(1);
            }
            //

            /*   Each thread should be allocated with a stack of size STACK_SIZE bytes.

               p_stack = new(STACK_SIZE)
            */
        }
    }

    ~Thread() {
        // free the stack
        if(tid != 0)
        {
            delete[] p_stack;
            p_stack = nullptr;
        }
    }
};




/*-------------------------------   static functions  implemenation  --------------------------------*/
    static int get_next_tid() {
        int id;

        if (!tids_for_reuse_set.empty()) {
            id = *(tids_for_reuse_set.begin());
            tids_for_reuse_set.erase(tids_for_reuse_set.begin());

        } else {
            if (next_tid >= MAX_THREAD_NUM) {
                id = -1;
            } else {
                id = next_tid;
                next_tid++;
            }
        }
        return id;
    }

     void reuse_tid(int tid) {
        // add the id to the tids_for_reuse_set
        tids_for_reuse_set.insert(tid);

    }


/**
 *
 * @param msg - err massage to print.
 */
     void err_print(const std::string &msg) {
        std::cerr << "thread library err: " << msg << std::endl;
    }


/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
    int uthread_init(int quantum_usecs) {
        if (quantum_usecs <= 0) {
            err_print("INVALID_QUANTUM");
            return FAILED;
        }
        s_quantum_usecs = quantum_usecs;
        Thread *thread = new Thread(0, nullptr);
        s_thread_map[0] = thread;
         context_switch(SWITCH_REASON_INIT);
//        thread->state_t = RUNNING;
//        s_running_thread_id = 0;
        return GOOD;
    }

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
    int uthread_spawn(thread_entry_point entry_point) {
        int status = FAILED;
        int id = -1;  // start with error number and change it to success in case new thread was created successfully

        do {
            if (entry_point == nullptr) {
                err_print("invalid input parameter -> entry_point is null");
                break;
            }

            // generate tid
            id = get_next_tid();
            if (id < 0) // failed to generate new process id
            {
                err_print("invalid id -> no process general id");
                break;
            }

            Thread *thread = new Thread(id, entry_point);
            s_thread_map[id] = thread;

      

            // add the new thread id to the list of threads (at the end of the Q)
            wants_to_run_queue.push(id);
            status = GOOD;

        } while (0);

        if (status == GOOD) {
            return id;
        } else {
            return FAILED;
        }
    }


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
    int uthread_terminate(int tid) {
        if(is_id_valid(tid) == FAILED)
        {
            return  FAILED;
        }

        if (tid == 0) {
            for (auto thread: s_thread_map) {
                T thread_to_delete = thread.second;
                delete thread_to_delete;
            }
            exit(0);
        }
        // is tid not 0 - err
        // is not exists - err


        reuse_tid(tid);  // free the id so it can be reused later


        // remove the tid from the 'ready Q' if it is there
        remove_from_queue(tid);

        // remove the thread from the map of threads

        Thread *p_cur_thread = s_thread_map[tid];
        s_thread_map.erase(tid);

        if(p_cur_thread->state_t == RUNNING)
        {
            //todo if the queue empty
            delete p_cur_thread;

             context_switch(SWITCH_REASON_TERMINATE);
//            s_running_thread_id = wants_to_run_queue.front();
//            wants_to_run_queue.pop();
//            s_thread_map[s_running_thread_id]->state_t = RUNNING;
        }
        else
        {
            delete p_cur_thread;
        }
        // destroy the thread object (will free the stack)
    return  GOOD;
    }

    void remove_from_queue(int tid) const {
        for (int i = 0; i <= wants_to_run_queue.size(); ++i) {
            int out_maybe_back_id = wants_to_run_queue.front();
            wants_to_run_queue.pop();
            if (out_maybe_back_id != tid) {
                wants_to_run_queue.push(out_maybe_back_id);
            }
        }
    }
    int is_id_valid(int tid) const
    {
        if ((tid < 0) || (tid >= MAX_THREAD_NUM) ||
            (s_thread_map.find(tid) == s_thread_map.end()))
        {
            err_print("invalid ID");
            return FAILED;
        }
        return GOOD;
    }


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
    int uthread_block(int tid) {
        if(is_id_valid(tid == FAILED))
        {
            return  FAILED;
        }

        if(tid == 0)
        {
            err_print("You cannot block the OS");
            return FAILED;
        }
        if(s_thread_map[tid]->state_t == RUNNING)
        {
             context_switch(SWITCH_REASON_BLOCK);
//            s_running_thread_id = wants_to_run_queue.front();
//            wants_to_run_queue.pop();
//            s_thread_map[tid]->state_t = BLOCKED;
//            s_thread_map[s_running_thread_id]->state_t = RUNNING;
        }
        else
        {
            remove_from_queue(tid);
            s_thread_map[tid]->state_t = BLOCKED;
        }
        return GOOD;
    }


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
    int uthread_resume(int tid) {
        if(is_id_valid(tid == FAILED))
        {
            return FAILED;
        }
        if(s_thread_map[tid]->state_t == BLOCKED)
        {
            s_thread_map[tid]->state_t = READY;
            wants_to_run_queue.push(tid);
        }
        return GOOD;
    }


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
    int uthread_sleep(int num_quantums) {

        if (s_running_thread_id == 0)
            reutrn FAILED;

        s_thread_map[s_running_thread_id]->sleeping_quantum_counter = num_quantums;
        context_switch(SWITCH_REASON_BLOCK);
        return GOOD;
    }


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
    int uthread_get_tid() {
        return s_running_thread_id;
    }


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
    int uthread_get_total_quantums() {
        return s_total_system_quantum;
    }


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
    int uthread_get_quantums(int tid) {
        if (is_id_valid(tid))
        {
            return  s_thread_map[tid]->running_quantum_counter;
        }
        else return FAILED;
    }




    static void  context_switch(int switch_reason)
    {

        s_total_system_quantum++;
        switch (switch_reason)
        {
            case SWITCH_REASON_INIT:
                s_running_thread_id = 0;
                s_thread_map[s_running_thread_id]->state_t = RUNNING;
                break;
            case SWITCH_REASON_TERMINATE:

                break;
            case SWITCH_REASON_BLOCK:
                s_thread_map[s_running_thread_id]->state_t = BLOCKED;
                break;
            case SWITCH_REASON_TIMER:
                // change current running thread to 'READY'
                s_thread_map[s_running_thread_id]->state_t = READY;
                // push current running thread to end of Queue
                wants_to_run_queue.push(s_running_thread_id);
                break;
            default:
                // error
                break;

        }
        // find next 'READY' thread
        s_running_thread_id = wants_to_run_queue.front();
        // pop next 'READY' thread from Queue
        wants_to_run_queue.pop();
        // context switch to the new thread
        s_thread_map[s_running_thread_id]->state_t = RUNNING;
        // start counting thread running time
        set_timer();
        // increase thread running counter
        s_thread_map[s_running_thread_id]->running_quantum_counter++;

    }



    void timer_handler(int sig)
    {
         context_switch(SWITCH_REASON_TIMER);
    }


    void set_timer(void)
    {
        struct sigaction sa = {0};
        struct itimerval timer;

        // Install timer_handler as the signal handler for SIGVTALRM.
        sa.sa_handler = &timer_handler;
        if (sigaction(SIGVTALRM, &sa, NULL) < 0)
        {
            printf("sigaction error.");
        }

        // Configure the timer to expire after 1 sec...
        timer.it_value.tv_sec = s_quantum_usecs / MILLION;        // first time interval, seconds part
        timer.it_value.tv_usec = s_quantum_usecs % MILLION;        // first time interval, microseconds part

        // configure the timer to expire every sec after that.
        timer.it_interval.tv_sec = s_quantum_usecs / MILLION;    // following time intervals, seconds part
        timer.it_interval.tv_usec = s_quantum_usecs % MILLION;    // following time intervals, microseconds part

        // Start a virtual timer. It counts down whenever this process is executing.
        if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
        {
            printf("setitimer error.");
        }
    }

    void decreas_sleeping_counter()
    {

    }



