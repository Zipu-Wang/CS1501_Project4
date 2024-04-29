#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

//define constants for shared memory offsets
#define COUNTER 12
#define IN 13
#define OUT 14
#define TOTAL 15
#define BUFINDEX 16

//semaphore structure
struct cs1550_sem {
    int value;
    struct mutex* lock; //mutex for atomic operations
    struct Node *head, *tail; //queue for blocked processes
};

//down semaphore syscall
void down(struct cs1550_sem *sem) {
    syscall(441, sem); //assuming syscall 441
}

//wrapper function to invoke the up semaphore syscall
void up(struct cs1550_sem *sem) {
    syscall(442, sem); //assuming syscall 442
}

//wrapper function to initialize a semaphore
void seminit(struct cs1550_sem *sem, int value) {
    syscall(443, sem, value); //assuming syscall 443
}

//function to simulate the production process
void produce(int* buffer, int producer_id, struct cs1550_sem* empty, struct cs1550_sem* full, struct cs1550_sem* mutex) {
    while(1) {
        down(empty); //wait if no empty slots available
        down(mutex); //lock access to buffer

        //buffer space starts at index 17
        int next_in = buffer[IN] + 17;
        buffer[next_in] = buffer[TOTAL] + 1; //item value
        buffer[TOTAL]++; //increase total items produced
        printf("Producer %d Produced: %d\n", producer_id, buffer[next_in]);

        //update IN index
        buffer[IN] = (buffer[IN] + 1) % buffer[BUFINDEX];
        buffer[COUNTER]++;

        up(mutex); //unlock buffer
        up(full); //signal that a new item is available
    }
}

//function to consumption process
void consume(int* buffer, int consumer_id, struct cs1550_sem *empty, struct cs1550_sem *full, struct cs1550_sem *mutex) {
    while(1) {
        down(full); //wait if not available
        down(mutex); //lock access to buffer

        //consume an item
        printf("Consumer %d Consumed: %d\n", consumer_id, buffer[buffer[OUT] + 17]);

        //update out index
        buffer[OUT] = (buffer[OUT] + 1) % buffer[BUFINDEX];
        buffer[COUNTER]--;

        up(mutex); //unlock buffer
        up(empty); //signal an empty slot is available
    }
}

//helper function to print program usage
void printUsage() {
    printf("Usage: ./procons [number of producers] [number of consumers] [buffer size]\n");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printUsage();
        return -1;
    }

    int numProd = atoi(argv[1]);
    int numCon = atoi(argv[2]);
    int bufSize = atoi(argv[3]);

    //validate input arguments
    if (numProd <= 0 || numCon <= 0 || bufSize <= 0) {
        printf("Invalid argument(s)\n");
        printUsage();
        return -1;
    }

    //allocate shared memory for buffer
    int *buf = (int*)mmap(NULL, (bufSize * 4) + (3 * sizeof(struct cs1550_sem)) + 20, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    //semaphore initialization
    struct cs1550_sem *empty = (struct cs1550_sem*)buf;
    struct cs1550_sem *full = empty + 1;
    struct cs1550_sem *mutex = full + 1;
    seminit(empty, bufSize);
    seminit(full, 0);
    seminit(mutex, 1);

    //initialize buffer indices and counters
    buf[COUNTER] = 0;
    buf[IN] = 0;
    buf[OUT] = 0;
    buf[TOTAL] = 0;
    buf[BUFINDEX] = bufSize;

    //fork
    for (int i = 0; i < numProd; i++) {
        if (fork() == 0) {
            produce(buf, i + 1, empty, full, mutex);
            return 0;
        }
    }
    for (int i = 0; i < numCon; i++) {
        if (fork() == 0) {
            consume(buf, i + 1, empty, full, mutex);
            return 0; // Child exits after consuming
        }
    }

    return 0;
}