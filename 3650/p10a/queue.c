//#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define QUEUE_SIZE 8 // power of two
#define NN 1024      // textual replacement done by the preprocessor
const int NNV = 1024; // variable that the compiler sees

typedef struct shq {
    int queue[QUEUE_SIZE];
    unsigned int qii; // get index
    unsigned int qjj; // put index
    sem_t isem; // space
    sem_t osem; // items
    // note, invariant: isem + osem = 8
    // qii - qjj is between 0 and 8 ... modulo 2^32
} shq;

// Question: 
//  - What happens when qii or qjj overflow?
//  - Does this break anything?
//
// Simpler question - with 16-bit ints.
//
//  unsigned short ii = 65535;
//  ii += 1;
//  What's ii? ii == 0
//
//  65535 % 8 = 7
//  (65535 + 1) % 8 = 0
//  ((65535 + 1) % 65536) % 8 = 0

shq* shared = 0;

int
queue_get()
{
    int rv;
    rv = sem_wait(&(shared->osem));
    assert(rv == 0);

    unsigned int ii = atomic_fetch_add(&(shared->qii), 1);
    int yy = shared->queue[ii % QUEUE_SIZE];

    rv = sem_post(&(shared->isem));
    assert(rv == 0);

    return yy;
}

void
queue_put(int xx)
{
    int rv;
    rv = sem_wait(&(shared->isem));
    assert(rv == 0);
    
    unsigned int jj = atomic_fetch_add(&(shared->qjj), 1);
    shared->queue[jj % QUEUE_SIZE] = xx;
    
    rv = sem_post(&(shared->osem));
    assert(rv == 0);
}

void
producer_proc()
{
    for (int ii = 0; ii < NN; ++ii) {
        queue_put(ii);
    }
}

int
main(int _ac, char* _av[])
{
    int kids[2];

    shared = mmap(0, 1024*1024, PROT_READ|PROT_WRITE,
            MAP_SHARED|MAP_ANONYMOUS, -1, 0);

    shared->qii = 0;
    shared->qjj = 0;

    sem_init(&(shared->isem), 1, QUEUE_SIZE);
    sem_init(&(shared->osem), 1, 0);

    for (int ii = 0; ii < 2; ++ii) {
        kids[ii] = fork();
        if (kids[ii] == 0) {
            producer_proc();
            return 0;
        }
    }

    for (int ii = 0; ii < 2*NN; ++ii) {
        int yy = queue_get();
        printf("%d\n", yy);
        usleep(1000);
    }

    for (int ii = 0; ii < 2; ++ii) {
        waitpid(kids[ii], 0, 0);
    }

    return 0;
}

