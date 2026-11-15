#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <regex>
#include <vector>
#include <algorithm>
#include <new>

#ifndef DMA_SIMULATOR_TEST
#include "httplib.h"
#endif

using namespace std;

// One node in the singly linked list that represents the memory pool.
struct Block {
    int id;
    int start;
    int size;
    bool free;
    bool reallocated;
    char* data;
    string type;
    vector<bool> initialized;
    Block* next;
};

class MemorySimulator {

    Block* head;
    int totalSize;
    int nextId;

    Block* createFreeBlock(int start, int size, Block* nextBlock) {
        Block* block = new (nothrow) Block{};

        if (block == nullptr)
            return nullptr;

        block->id = 0;
        block->start = start;
        block->size = size;
        block->free = true;
        block->reallocated = false;
        block->data = nullptr;
        block->type = "free";
        block->next = nextBlock;

        return block;
    }

    Block* findAllocation(int id) {
        Block* current = head;

        while (current != nullptr) {
            if (!current->free && current->id == id)
                return current;

            current = current->next;
        }

        return nullptr;
    }

public:
    MemorySimulator(int size) {
        totalSize = size;
        nextId = 1;
        head = createFreeBlock(0, size, nullptr);

        if (head == nullptr)
            throw bad_alloc();

        cout << "Memory Initialized: " << size << " Bytes" << endl;
    }

    // Find a free block using First Fit, Best Fit or Worst Fit.
    Block* findBlock(int size, const string& strategy) {
        Block* current = head;
        Block* selected = nullptr;

        while (current != nullptr) {
            if (current->free && current->size >= size) {

                if (strategy == "first")
                    return current;

                if (selected == nullptr) {
                    selected = current;
                }
                else if (strategy == "best" && current->size < selected->size) {
                    selected = current;
                }
                else if (strategy == "worst" && current->size > selected->size) {
                    selected = current;
                }
            }

            current = current->next;
        }

        return selected;
    }

    // Allocate memory and split a free block when the request is smaller.
    int allocate(int size, const string& strategy, const string& type) {
        if (size < 1 || size > totalSize)
            return -1;

        if (type != "malloc" && type != "calloc")
            return -1;

        if (strategy != "first" && strategy != "best" && strategy != "worst")
            return -1;

        Block* selected = findBlock(size, strategy);

        if (selected == nullptr)
            return -1;

        // Prepare the data and initialization information before changing the list.
        char* newData = new (nothrow) char[size];
        if (newData == nullptr)
            return -1;

        vector<bool> newInitialization;
        try {
            newInitialization.assign(size, type == "calloc");
        }
        catch (...) {
            delete[] newData;
            return -1;
        }

        Block* remainingBlock = nullptr;
        if (selected->size > size) {
            remainingBlock = createFreeBlock(
                selected->start + size,
                selected->size - size,
                selected->next
            );

            if (remainingBlock == nullptr) {
                delete[] newData;
                return -1;
            }
        }

        if (type == "calloc")
            memset(newData, 0, size);

        if (remainingBlock != nullptr)
            selected->next = remainingBlock;

        selected->size = size;
        selected->free = false;
        selected->reallocated = false;
        selected->id = nextId++;
        selected->type = type;
        selected->data = newData;
        selected->initialized.swap(newInitialization);

        return selected->id;
    }
