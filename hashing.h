#ifndef HASHING_H
#define HASHING_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Structure to store a hash record
typedef struct hashRecord {
    uint32_t hash;          // Hash value for the record
    char name[50];          // Name associated with the record
    int salary;             // Salary of the individual
    struct hashRecord* next; // Pointer for handling collisions (linked list)
} hashRecord;

// Structure to store a command and its value
typedef struct commandStr {
    char name[50];  // Command name
    int val;        // Value associated with the command
} commandStr;

extern hashRecord* head;  // Head pointer to the hash record linked list

// Hash function (Jenkins' hash)
uint32_t jhash(const uint8_t* key, size_t length) {
    uint32_t hash = 0;
    for (size_t i = 0; i < length; ++i) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

// Search for a command record in the hash table
hashRecord* search(commandStr* data) {
    uint32_t hash = jhash((const uint8_t*)data->name, strlen(data->name));  // Compute hash for the command name
    hashRecord* curr = head;  // Start at the head of the linked list
    while (curr) {
        if (curr->hash == hash && strcmp(curr->name, data->name) == 0) {  // Check if hash and name match
            return curr;  // Record found
        }
        curr = curr->next;  // Move to next record in case of collision
    }
    return NULL;  // Return NULL if not found
}

#endif
