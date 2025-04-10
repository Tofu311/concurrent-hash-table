#include <pthread.h>
#include <string.h>
#include "hashing.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <inttypes.h>
#include <unistd.h>

// Global synchronization primitives and data structures
pthread_rwlock_t rwLock; //read-write lock for list access
pthread_cond_t deleteCV; //condition variable for delete thread waiting
FILE* output;            //output log file
hashRecord* head = NULL; //head of the linked list hash table

int locksAcquired = 0;   //counts of all lock acquisitions
int locksReleased = 0;   //counts of all lock releases

//gets current timestamp in microseconds for log entries
unsigned long long getTimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)(tv.tv_sec) * 1000000ULL + (unsigned long long)(tv.tv_usec);
}

//sorts the linked list by hash value using insertion sort
hashRecord* sortList(hashRecord* head) {
    if (!head || !head->next) return head;
    
    hashRecord* sorted = NULL;
    while (head) {
        hashRecord* curr = head;
        head = head->next;

        if (!sorted || curr->hash < sorted->hash) {
            curr->next = sorted;
            sorted = curr;
        } else {
            hashRecord* temp = sorted;
            while (temp->next && temp->next->hash <= curr->hash) {
                temp = temp->next;
            }
            curr->next = temp->next;
            temp->next = curr;
        }
    }
    return sorted;
}

//inserts or updates a key-salary record in the hash table
void* insert(void* args) {
    commandStr* data = (commandStr*)args;
    uint32_t hash = jhash((const uint8_t*)data->name, strlen(data->name));

    pthread_rwlock_wrlock(&rwLock);  // Acquire write lock
    fprintf(output, "%" PRIu64 ": WRITE LOCK ACQUIRED\n", getTimestamp());
    locksAcquired++;

    //sees if the key already exists and update it
    hashRecord* curr = head;
    while (curr) {
        if (curr->hash == hash && strcmp(curr->name, data->name) == 0) {
            curr->salary = data->val;
            fprintf(output, "%" PRIu64 ": INSERT,%u,%s,%d\n", getTimestamp(), hash, data->name, data->val);
            pthread_rwlock_unlock(&rwLock);
            fprintf(output, "%" PRIu64 ": WRITE LOCK RELEASED\n", getTimestamp());
            locksReleased++;
            pthread_cond_broadcast(&deleteCV);  //signal waiting deletes
            return NULL;
        }
        curr = curr->next;
    }

    //insert a new node at the head of the list
    hashRecord* newNode = malloc(sizeof(hashRecord));
    if (!newNode) exit(EXIT_FAILURE);
    strcpy(newNode->name, data->name);
    newNode->salary = data->val;
    newNode->hash = hash;
    newNode->next = head;
    head = newNode;

    fprintf(output, "%" PRIu64 ": INSERT,%u,%s,%d\n", getTimestamp(), hash, data->name, data->val);
    pthread_rwlock_unlock(&rwLock);
    fprintf(output, "%" PRIu64 ": WRITE LOCK RELEASED\n", getTimestamp());
    locksReleased++;

    pthread_cond_broadcast(&deleteCV); //notifies & delete threads
    return NULL;
}

//dletes a key-salary record from the hash table
void* delete(void* args) {
    commandStr* data = (commandStr*)args;
    pthread_rwlock_wrlock(&rwLock);
    fprintf(output, "%" PRIu64 ": WRITE LOCK ACQUIRED\n", getTimestamp());
    locksAcquired++;

    hashRecord* result = search(data);

    //wait if key doesn't exist yet
    while (!result) {
        fprintf(output, "%" PRIu64 ": WAITING ON INSERTS\n", getTimestamp());
        pthread_cond_wait(&deleteCV, &rwLock);  //wait on condition variable
        usleep(100);  //slight pause to reduce busy waiting
        fprintf(output, "%" PRIu64 ": DELETE AWAKENED\n", getTimestamp());
        result = search(data);  //double check after waking up
    }

    fprintf(output, "%" PRIu64 ": DELETE,%s\n", getTimestamp(), data->name);

    //unlink the node from the list
    if (head == result) {
        head = result->next;
    } else {
        hashRecord* curr = head;
        while (curr && curr->next != result) {
            curr = curr->next;
        }
        if (curr) curr->next = result->next;
    }
    free(result);  //free memory of the deleted node

    pthread_rwlock_unlock(&rwLock);
    fprintf(output, "%" PRIu64 ": WRITE LOCK RELEASED\n", getTimestamp());
    locksReleased++;
    return NULL;
}

//searches for a key in the hash table and logs the result
void* searchThread(void* args) {
    pthread_rwlock_rdlock(&rwLock);  //acquire read lock
    fprintf(output, "%" PRIu64 ": READ LOCK ACQUIRED\n", getTimestamp());
    locksAcquired++;

    commandStr* data = (commandStr*)args;
    hashRecord* result = search(data);
    if (result) {
        fprintf(output, "%" PRIu64 ": SEARCH: %u,%s,%d\n", getTimestamp(), result->hash, result->name, result->salary);
    } else {
        fprintf(output, "%" PRIu64 ": SEARCH: NOT FOUND\n", getTimestamp());
    }

    pthread_rwlock_unlock(&rwLock);
    fprintf(output, "%" PRIu64 ": READ LOCK RELEASED\n", getTimestamp());
    locksReleased++;
    return NULL;
}

int main() {
    //open input file with commands
    FILE* input = fopen("commands.txt", "r");
    if (!input) return EXIT_FAILURE;
    
    //open log output file
    output = fopen("output.txt", "w");
    if (!output) return EXIT_FAILURE;

    pthread_rwlock_init(&rwLock, NULL);
    pthread_cond_init(&deleteCV, NULL);

    //parse number of threads from the first line
    char firstLine[50];
    if (!fgets(firstLine, sizeof(firstLine), input)) return EXIT_FAILURE;
    int threadCount;
    sscanf(firstLine, "threads,%d,%*d", &threadCount);

    pthread_t threads[threadCount];
    commandStr commands[threadCount];
    int successfulThreads = 0;
    char line[100];

    //read and parse each command
    while (fgets(line, sizeof(line), input)) {
        char operation[10];
        int value = 0;
        if (sscanf(line, "%9[^,],%49[^,],%d", operation, commands[successfulThreads].name, &value) == 3) {
            commands[successfulThreads].val = value;

            //dispatch to corresponding thread function
            if (strcmp(operation, "insert") == 0) {
                pthread_create(&threads[successfulThreads], NULL, insert, &commands[successfulThreads]);
            } else if (strcmp(operation, "delete") == 0) {
                pthread_create(&threads[successfulThreads], NULL, delete, &commands[successfulThreads]);
            } else if (strcmp(operation, "search") == 0) {
                pthread_create(&threads[successfulThreads], NULL, searchThread, &commands[successfulThreads]);
            }
            successfulThreads++;
        }
    }

    //waits for all threads to complete
    for (int i = 0; i < successfulThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    //final output for stats and sorted list
    fprintf(output, "Finished all threads.\n");
    fprintf(output, "Number of lock acquisitions: %d\n", locksAcquired);
    fprintf(output, "Number of lock releases: %d\n", locksReleased);

    pthread_rwlock_rdlock(&rwLock);  //locks before reading final list
    fprintf(output, "%" PRIu64 ": READ LOCK ACQUIRED\n", getTimestamp());
    head = sortList(head);
    for (hashRecord* curr = head; curr; curr = curr->next) {
        fprintf(output, "%u,%s,%d\n", curr->hash, curr->name, curr->salary);
    }
    pthread_rwlock_unlock(&rwLock);
    fprintf(output, "%" PRIu64 ": READ LOCK RELEASED\n", getTimestamp());

    //final cleanup
    pthread_rwlock_destroy(&rwLock);
    pthread_cond_destroy(&deleteCV);
    fclose(input);
    fclose(output);
    return 0;
}
