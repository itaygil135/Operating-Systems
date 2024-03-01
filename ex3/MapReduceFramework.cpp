#include "Barrier.h"
#include <pthread.h>
#include <atomic>
#include <map>
#include <algorithm>
#include <iostream>
#include "MapReduceFramework.h"
#include <semaphore.h>
#include <set>


// ----- CONSTANTS ERROR MASSAGES --------
#define ERR_PTHREAD_CREATE "system error: failure in the pthread creat function"
#define ERR_PTHREAD_JOIN "system error: failure in the pthread join function"
#define ERR_PTHREAD_MUTEX_DESTROY "system error: failure in the pthread mutex destroy function"
#define ERR_PTHREAD_MUTEX_LOCK "system error: failure in the pthread mutex lock function"
#define ERR_PTHREAD_MUTEX_UNLOCK "system error: failure in the pthread mutex unlock function"
#define ERR_SEMAPHORE_CREAT "system error: failure in the semaphore creat function"
#define ERR_SEMAPHORE_WAIT "system error: failure in the semaphore wait function"
#define ERR_SEMAPHORE_POST "system error: failure in the semaphore post function"
#define ERR_SEMAPHORE_DESTROY "system error: failure in the semaphore destroy function"


// CONSTANTS
enum Action {
    LOCK,
    UNLOCK
};

// ----- typedef DECLARATION  --------
using namespace std;

class JobContext;

// ----- METHOD DECLARATION  --------
void *MainFrameworkFunc(void *context);

void system_call_failure_printer(const std::string &problem);



// ----- STRUCTS DECLARATION  --------
class ThreadContext {
public:
    JobContext *jobContext;
    int ID;
    IntermediateVec intermediateVec;
    pthread_t thread;


     ThreadContext(JobContext *jobContext, int i) : jobContext(jobContext),ID(i) {}
//    {
//     //   thread = 0;
//        if (pthread_create(&this->thread, nullptr, MainFrameworkFunc, this)) {
//            system_call_failure_printer(ERR_PTHREAD_CREATE);
//            exit(EXIT_FAILURE);
//        }
//    }
};

K2 *getMaxK2(ThreadContext *threadContext, K2 *currMaxKey);


class JobContext {
public:
    const MapReduceClient *client; // A struct that hold the map and reduce functions.
    const InputVec *inputVec;
    OutputVec *outputVec;
    int multiThreadLevel; // Saves how many threads the job can use.
    unsigned int numOfJobs;
    JobState jobState;
    vector<ThreadContext *> threadContexts; //A vector for the job threads contexts.
    pthread_mutex_t jobStateMutex; // A mutex for the job state parentage.
    // Atomic counter to know how many started the job - using to know which job is next to start.
    atomic<size_t> atomicCounter;
    // Atomic counter to know how many finish the job - using for the percentage calculator.
    atomic<size_t> finishCounterMap;
    atomic<size_t> finishCounterShuffle;
    // Atomic counter to know how many elements are in the Intermediate vectors.
    atomic<size_t> intermediaryElementsCounter;
    //A synchronisation mechanism that makes sure no thread continues before all threads arrived at the barrie.
    //Get the number of threads (multiThreadLevel) that need to get to the barrier before continue.
    Barrier barrier;
    sem_t semaphore{}; // A semaphore that we use for separating the shuffle and reduce stage.
    vector<IntermediateVec> shuffleVec; // The shuffle Vector is a vector that each element is an Intermediate Vector.
    pthread_mutex_t reducePhaseMutex; // A mutex for reduce state.
    pthread_mutex_t emitThreeMutex; // A mutex for reduce state.
    // Atomic counter to know how many finis reduce part.
    atomic<size_t> pairsDoneCounter; //
    std::atomic_flag waitForJobFlag{false}; // atomic flag for preventing multiple calls to waitForJOb

    JobContext(const MapReduceClient &client, const InputVec &inputVec, OutputVec &outputVec, int multiThreadLevel) :
            client(&client), inputVec(&inputVec), outputVec(&outputVec), multiThreadLevel(multiThreadLevel),
            numOfJobs(inputVec.size()), jobState({UNDEFINED_STAGE, 0}), threadContexts(multiThreadLevel),
            jobStateMutex(PTHREAD_MUTEX_INITIALIZER), atomicCounter(0), finishCounterMap(0),
            finishCounterShuffle(0), intermediaryElementsCounter(0), barrier(multiThreadLevel),
            reducePhaseMutex(PTHREAD_MUTEX_INITIALIZER), emitThreeMutex(PTHREAD_MUTEX_INITIALIZER),
            pairsDoneCounter(0) {

        // Init Semaphore that only one thread would be able to use it.
        // If pshared has the value 0, then the semaphore is shared between the threads of a process,
        // If pshared is nonzero, then the semaphore is shared between processes.
        if (sem_init(&semaphore, 0, 0) != 0) {
            system_call_failure_printer(ERR_SEMAPHORE_CREAT);
            exit(EXIT_FAILURE);
        }

        // Init all the threads contexts - a data structure that contains a thread object and more...

//        for (int i = 0; i < multiThreadLevel; ++i) {
//            threadContexts[i] = new ThreadContext(this,i);
//        }
    }

    ~JobContext() {
        //Releasing all the ThreadContext objects we created.
        for (auto const &currThread : this->threadContexts) {
            delete currThread;
        }

        //Destroy all the mutexes we created, making sure no failure system call function happened.
        if (pthread_mutex_destroy(&jobStateMutex) || pthread_mutex_destroy(&reducePhaseMutex)
            || pthread_mutex_destroy(&emitThreeMutex)) {
            system_call_failure_printer(ERR_PTHREAD_MUTEX_DESTROY);
            exit(EXIT_FAILURE);
        }

        // Destroy semaphore
        if (sem_destroy(&semaphore)) {
            system_call_failure_printer(ERR_SEMAPHORE_DESTROY);
            exit(EXIT_FAILURE);
        }
    }
};

// ----- Helper Functions --------
/**
 *
 * @param problem - The string of the massage that need to be printed.
 */
void system_call_failure_printer(const std::string &problem) {
    std::cerr << "system error: " << problem << std::endl;
}


//void mutexHandler(pthread_mutex_t *currMutex, Action currAct) {
//    switch (currAct) {
//
//        case LOCK:
//            if (pthread_mutex_lock(currMutex)) {
//                system_call_failure_printer(ERR_PTHREAD_MUTEX_LOCK);
//                exit(EXIT_FAILURE);
//            }
//            return;
//
//        case UNLOCK:
//            if (pthread_mutex_unlock(currMutex)) {
//                system_call_failure_printer(ERR_PTHREAD_MUTEX_UNLOCK);
//                exit(EXIT_FAILURE);
//            }
//            return;
//    }
//}


void lockMutex(pthread_mutex_t *mutex) {
    if (pthread_mutex_lock(mutex) != 0) {
        system_call_failure_printer(ERR_PTHREAD_MUTEX_LOCK);
        exit(EXIT_FAILURE);
    }
}

void unlockMutex(pthread_mutex_t *mutex) {
    if (pthread_mutex_unlock(mutex) != 0) {
        system_call_failure_printer(ERR_PTHREAD_MUTEX_UNLOCK);
        exit(EXIT_FAILURE);
    }
}

void mutexHandler(pthread_mutex_t *currMutex, Action currAct) {
    if (currAct == LOCK) {
        lockMutex(currMutex);
    } else {
        unlockMutex(currMutex);
    }
}


/**
 *
 * @param threadContext The Details of the current working thread.
 */

void UpdateJobStage(ThreadContext *threadContext) {
    threadContext->jobContext->jobState.stage = MAP_STAGE; //Setting the new job state to be MAP_STAGE.
}

size_t GetNextJob(ThreadContext *threadContext) {
    return (threadContext->jobContext->atomicCounter)++; //Getting the next num of element.
}

bool IsJobRemaining(size_t ticket, ThreadContext *threadContext) {
    return ticket < threadContext->jobContext->numOfJobs; //Check if there are remaining jobs
}

void ExecuteMapJob(size_t ticket, ThreadContext *threadContext) {
    //Sending to the client map function the next element to work on.
    threadContext->jobContext->client->map(threadContext->jobContext->inputVec->at(ticket).first,
                                           threadContext->jobContext->inputVec->at(ticket).second, threadContext);
    threadContext->jobContext->finishCounterMap++;
}

void MapPhase(ThreadContext *threadContext) {
    UpdateJobStage(threadContext);
    size_t ticket = GetNextJob(threadContext);
    //Keep working until we finish all the elements in the Input vector.
    while (IsJobRemaining(ticket, threadContext)) {
        ExecuteMapJob(ticket, threadContext);
        ticket = GetNextJob(threadContext); //Updating how many elements has finished the map phase.
    }
}

/**
 * In this phase is to create new sequences of (k2, v2) where in each sequence all
 * keys are identical and all elements with a given key are in a single sequence.
 *
 * From the sorted intermediary vectors , for each key we create a combine new vector and we we put it in a queue,
 * that representes as another vector(a vector of vectors)
 * @param threadContext
 */
void ShufflePhase(ThreadContext *threadContext) {

    unsigned long totalPairsLeft = threadContext->jobContext->intermediaryElementsCounter;
    K2 *currMaxKey;
    while (totalPairsLeft > 0) {
        currMaxKey = nullptr;

        //Finding max Key
        currMaxKey = getMaxK2(threadContext, currMaxKey);

        // Making a vector of all the elements that's equal to the max key in the shuffle vector.
        if (currMaxKey != nullptr) {
            IntermediateVec WorkVecPerKey;

            for (auto &currContext : threadContext->jobContext->threadContexts) {
                // Iterating over the vector of threads contexts
                if (currContext == NULL || currContext->intermediateVec.empty()) continue; // Skipping if the context is NULL or vector is empty

                // Iterate from the back until a non-matching key is found or the vector is empty.
                for (auto rit = currContext->intermediateVec.rbegin();
                        rit != currContext->intermediateVec.rend() && rit->first != nullptr &&
                        !(*currMaxKey < *(rit->first)) && !(*(rit->first) < *currMaxKey); ) {

                    WorkVecPerKey.push_back(*rit); // Adding the element.
                    currContext->intermediateVec.pop_back(); // Removing it from the thread IntermediateVec.
                    totalPairsLeft -= 1;
                    threadContext->jobContext->finishCounterShuffle += 1;

                    if (currContext->intermediateVec.empty()) break;
                    rit = currContext->intermediateVec.rbegin();}
            }
            threadContext->jobContext->shuffleVec.push_back(WorkVecPerKey);
        }
    }
}

K2 *getMaxK2(ThreadContext *threadContext, K2 *currMaxKey) {
    for (auto currContext : threadContext->jobContext->threadContexts) {
        if (currContext == NULL) continue;
        const IntermediateVec currIntermediateVec = currContext->intermediateVec;


        if (!currIntermediateVec.empty()) {
            if (currMaxKey == nullptr) {
                // Setting the currMaxKey to get is first value
                currMaxKey = currIntermediateVec.back().first;
            }

                // If we found a bigger element than the current currMaxKey
            else if (*currMaxKey < *(currIntermediateVec.back().first))
                currMaxKey = currIntermediateVec.back().first;
        } // Skipping if the vector is empty
    }
    return currMaxKey;
}

/**
 * The function can be called Just after the shuffle stage is done.
 * The thread pop a vector from the back of the queue (the vector that has been created in the shuffle stage)
 * and run reduce on it.
 * The reduce function in turn will produce (k3, v3) pairs and will call emit3 to add them to the
 * framework data structures.
 */
void ReducePhase(ThreadContext *threadContext) {
    threadContext->jobContext->jobState.stage = REDUCE_STAGE;//Setting the new job state to be MAP_STAGE.
    while (true) {
        mutexHandler(&threadContext->jobContext->reducePhaseMutex, LOCK);
        //Keep getting element until the shuffleVec is empty.
        if (!threadContext->jobContext->shuffleVec.empty()) {

            // Getting the last element from the queue and pop
            IntermediateVec vec = threadContext->jobContext->shuffleVec.back();
            threadContext->jobContext->shuffleVec.pop_back();

            mutexHandler(&threadContext->jobContext->reducePhaseMutex, UNLOCK);

            //Run the reduce function on the curr vec
            threadContext->jobContext->client->reduce(&vec, threadContext);

            // This is for the job state percentage.
            threadContext->jobContext->pairsDoneCounter += vec.size();
        } else {
            mutexHandler(&threadContext->jobContext->reducePhaseMutex, UNLOCK);
            break;
        }
    }
}


/**
 * This function is the argument that all the job threads get.
 * That mean that all threads gets to this function and do as it says.
 * @param context - the argument that we send at the tread creation.
 */
void *MainFrameworkFunc(void *context) {
    auto *threadContext = (ThreadContext *) context;
    //Doing the Map phase.
    MapPhase(threadContext);

    //Sorting the intermediate Vectors
    sort(threadContext->intermediateVec.begin(), threadContext->intermediateVec.end(),
         [](const IntermediatePair &x, const IntermediatePair &y) {
             return *(x.first) < *(y.first);
         });

    //Using a barrier because the Shuffle phase must only start after all threads finished their sort phases.
    threadContext->jobContext->barrier.barrier();

    if(threadContext->ID == 0)
    {
        threadContext->jobContext->jobState.stage = SHUFFLE_STAGE;
        ShufflePhase(threadContext);
        //Make it ready for to Reduce phase
        // Getting the num of vectors in the shuffle vector.
        threadContext->jobContext->atomicCounter = threadContext->jobContext->shuffleVec.size();
        //Wake up all the threads that are blocked.
        for (int i = 0; i < threadContext->jobContext->multiThreadLevel - 1; ++i) {
            if (sem_post((&threadContext->jobContext->semaphore)) != 0) {
                system_call_failure_printer(ERR_SEMAPHORE_POST);
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        // Decrement the semaphore to get blocked until thread 0 finishes the shuffle
        if (sem_wait(&threadContext->jobContext->semaphore) != 0)
        {
           system_call_failure_printer(ERR_SEMAPHORE_WAIT);
           exit(EXIT_FAILURE);
        }
    }


    //Doing to Reduce phase.
    ReducePhase(threadContext);
    return nullptr;
}

// ----- Library Functions --------
/**
 *
 * @param client – The implementation of MapReduceClient or in other words the task that the framework should run.
 * @param inputVec -A vector of type std::vector<std::pair<K1*, V1*>>, the input elements.
 * @param outputVec - A vector of type std::vector<std::pair<K3*, V3*>>, to which the output
 * elements will be added before returning. You can assume that outputVec is empty.
 * @param multiThreadLevel - the number of worker threads to be used for running the algorithm.
 * @return The function returns JobHandle that will be used for monitoring the job.
 */
//JobHandle startMapReduceJob(const MapReduceClient &client, const InputVec &inputVec,
//                            OutputVec &outputVec, int multiThreadLevel) {
//    //In the pdf noted that we can assume the arguments are valid.
//    auto *newJob = new JobContext(client, inputVec, outputVec, multiThreadLevel);
//    return newJob;
//}


JobHandle startMapReduceJob(const MapReduceClient &client, const InputVec &inputVec,
                            OutputVec &outputVec, int multiThreadLevel) {
    //In the pdf noted that we can assume the arguments are valid.
    auto *newJob = new JobContext(client, inputVec, outputVec, multiThreadLevel);
    for (int i = 0; i < multiThreadLevel; ++i)
    {
        newJob->threadContexts[i] = new ThreadContext(newJob,i);
        if (pthread_create(&newJob->threadContexts[i] ->thread, nullptr, MainFrameworkFunc, newJob->threadContexts[i])) {
            system_call_failure_printer(ERR_PTHREAD_CREATE);
            exit(EXIT_FAILURE);
        }
    }
    return newJob;
}

/**
 * The function gets JobHandle returned by startMapReduceFramework and waits until it is finished.
 */
//void waitForJob(JobHandle job) {
//    auto *jobContext = (JobContext *) job;
//    if (jobContext->waitForJobFlag.test_and_set()) {
//        while (jobContext->jobState.stage != REDUCE_STAGE && jobContext->jobState.percentage != 100.0) {
//
//        }
//        return;
//    } else {
//
//        for (const auto &threadContext : jobContext->threadContexts) {
//            if (pthread_join(threadContext->thread, nullptr)) {
//                system_call_failure_printer(ERR_PTHREAD_JOIN);
//                exit(EXIT_FAILURE);
//            }
//        }
//    }
//}




bool isJobReady(JobContext *jobContext) {
    return jobContext->waitForJobFlag.test_and_set();
}

void waitForJobToFinish(JobContext *jobContext) {
    while (jobContext->jobState.stage != REDUCE_STAGE && jobContext->jobState.percentage != 100.0) {
        // Keep waiting
    }
}

void joinThreadContexts(JobContext *jobContext) {
    for (const auto &threadContext : jobContext->threadContexts) {
        if (pthread_join(threadContext->thread, nullptr)) {
            system_call_failure_printer(ERR_PTHREAD_JOIN);
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * The function gets JobHandle returned by startMapReduceFramework and waits until it is finished.
 */
void waitForJob(JobHandle job) {
    auto *jobContext = (JobContext *) job;
    if (isJobReady(jobContext)) {
        waitForJobToFinish(jobContext);
    } else {
        joinThreadContexts(jobContext);
    }
}
/**
 * The function gets a JobHandle and updates the state of the job into the given JobState struct.
 */
void getJobState(JobHandle job, JobState *state) {
    auto *jobContext = (JobContext *) job;

    //Using mutex in this function because it's a global data.
    mutexHandler(&jobContext->jobStateMutex, LOCK);

    auto curStage = jobContext->jobState.stage;
    state->stage = curStage;

    //For each stage we are using different atomic counters,
    // and because of that we are making different calculation for each stage.
    switch (curStage) {

        case UNDEFINED_STAGE:  //stage 0
            state->percentage = 0;
            break;

        case MAP_STAGE:  //stage 1
            state->percentage = 100 * ((float) jobContext->finishCounterMap.load() / (float) jobContext->numOfJobs);
            break;

        case SHUFFLE_STAGE:  //stage 2
            state->percentage = 100 * ((float) jobContext->finishCounterShuffle.load() /
                                       (float) jobContext->intermediaryElementsCounter.load());
            break;

        case REDUCE_STAGE:  //stage 3
            state->percentage = 100 * ((float) jobContext->pairsDoneCounter.load() /
                                       (float) jobContext->intermediaryElementsCounter.load());
            break;
    }
    mutexHandler(&jobContext->jobStateMutex, UNLOCK);
}

/**
 *
 * Releasing all resources of a the job given as a parameter.
 * We prevent releasing resources before the job finished.
 * After this function is called the job handle will be invalid.
 */
void closeJobHandle(JobHandle job) {
    waitForJob(job);
    auto *jobContext = (JobContext *) job;
    delete jobContext;
}


/**
 * The function is called from the client's map function.
 *  The function saves the intermediary element in the context data structures.
 *  In addition, the function updates the number of intermediary elements, by updating an atomic counter.
 * @param (key ,value)  intermediary element.
 * @param context - passed from the framework to the client's map function as parameter.
 *                  contains a data structure of the thread that created the intermediary element.
 */
void emit2(K2 *key, V2 *value, void *context) {
    auto threadContext = (ThreadContext *) context;
    threadContext->intermediateVec.push_back(IntermediatePair(key, value));
    threadContext->jobContext->intermediaryElementsCounter++;
}

/**
 * The function saves the output element in the context data structures (output vector). I
 * The function is called from the client's reduce function and the context is
 * passed from the framework to the client's reduce function as parameter.
 * @param (key , value) - (K3, V3) element.
 * @param context - contains data structure of the thread that created the output element,
 */
void emit3(K3 *key, V3 *value, void *context) {
    auto threadContext = (ThreadContext *) context;
    auto jobContext = (JobContext *) threadContext->jobContext;
    mutexHandler(&jobContext->emitThreeMutex, LOCK);
    jobContext->outputVec->push_back(OutputPair(key, value));
    mutexHandler(&jobContext->emitThreeMutex, UNLOCK);
}







