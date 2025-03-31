#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define MAX_NAME_LENGTH 50
#define HASH_SIZE 100
#define MAX_COMMANDS 1024

// Hash table record
typedef struct hash_struct {
    uint32_t hash;
    char name[MAX_NAME_LENGTH];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

// Hash table and lock
hashRecord *hashTable[HASH_SIZE];
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

// Jenkins one-at-a-time hash
uint32_t jenkins_hash(const char *key) {
    size_t i = 0;
    uint32_t hash = 0;
    while (key[i] != '\0') {
        hash += key[i++];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % HASH_SIZE;
}

// Insert
void insert(const char *name, uint32_t salary) {
    uint32_t h = jenkins_hash(name);

    pthread_rwlock_wrlock(&rwlock);
    hashRecord *curr = hashTable[h];

    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            curr->salary = salary;
            pthread_rwlock_unlock(&rwlock);
            return;
        }
        curr = curr->next;
    }

    hashRecord *newNode = malloc(sizeof(hashRecord));
    newNode->hash = h;
    strncpy(newNode->name, name, MAX_NAME_LENGTH);
    newNode->salary = salary;
    newNode->next = hashTable[h];
    hashTable[h] = newNode;
    pthread_rwlock_unlock(&rwlock);
}

// Search
void search(const char *name) {
    uint32_t h = jenkins_hash(name);

    pthread_rwlock_rdlock(&rwlock);
    hashRecord *curr = hashTable[h];

    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            printf("%s %u\n", curr->name, curr->salary);
            pthread_rwlock_unlock(&rwlock);
            return;
        }
        curr = curr->next;
    }

    printf("No Record Found\n");
    pthread_rwlock_unlock(&rwlock);
}

// TODO: Delete
void delete(const char *name){

}

// TODO: Print
void print() {

}

// TODO: Thread Data Structure
typedef struct {
    char command[10];
    char name[MAX_NAME_LENGTH];
    uint32_t value;
} thread_data;

// TODO: Thread Functions
void *thread_func(void *arg){

}


// Main
int main() {
    FILE *inputFile = fopen("commands.txt", "r");
    if (!inputFile) {
        perror("Failed to open commands.txt");
        return 1;
    }

    int total_commands;
    fscanf(inputFile, "threads,%d,0\n", &total_commands);

    thread_data *insert_cmds[MAX_COMMANDS];
    thread_data *other_cmds[MAX_COMMANDS];
    int insert_count = 0, other_count = 0;

    char line[128];
    while (fgets(line, sizeof(line), inputFile)) {
        thread_data *data = malloc(sizeof(thread_data));
        char *cmd = strtok(line, ",");
        char *arg1 = strtok(NULL, ",");
        char *arg2 = strtok(NULL, ",");

        if (!cmd || !arg1 || !data) continue;

        strcpy(data->command, cmd);
        strncpy(data->name, arg1, MAX_NAME_LENGTH);
        data->value = arg2 ? atoi(arg2) : 0;

        if (strcmp(data->command, "insert") == 0) {
            insert_cmds[insert_count++] = data;
        } else {
            other_cmds[other_count++] = data;
        }
    }

    fclose(inputFile);

    // Run INSERT commands first
    pthread_t insert_threads[insert_count];
    for (int i = 0; i < insert_count; i++)
        pthread_create(&insert_threads[i], NULL, thread_func, insert_cmds[i]);
    for (int i = 0; i < insert_count; i++)
        pthread_join(insert_threads[i], NULL);

    // Run DELETE, SEARCH, PRINT commands after
    pthread_t other_threads[other_count];
    for (int i = 0; i < other_count; i++)
        pthread_create(&other_threads[i], NULL, thread_func, other_cmds[i]);
    for (int i = 0; i < other_count; i++)
        pthread_join(other_threads[i], NULL);


    return 0;
}
