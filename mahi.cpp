class MemorySimulator {

    Block* head;
    int totalSize;
    int nextId;

    // Create a free block. Keeping this setup in one place avoids repeated code.
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

    // Resize an allocation. Existing bytes are preserved if it can be resized.
    int reallocate(int id, int newSize) {
        if (newSize < 1 || newSize > totalSize)
            return -1;

        Block* current = findAllocation(id);
        if (current == nullptr)
            return -1;

        int oldSize = current->size;

        if (newSize == oldSize)
            return id;

        // Case 1: Shrink the allocation and return the released space to the list.
        if (newSize < oldSize) {
            char* newData = new (nothrow) char[newSize];
            if (newData == nullptr)
                return -1;

            vector<bool> newInitialization;
            try {
                newInitialization.assign(
                    current->initialized.begin(),
                    current->initialized.begin() + newSize
                );
            }
            catch (...) {
                delete[] newData;
                return -1;
            }

            Block* freePart = createFreeBlock(
                current->start + newSize,
                oldSize - newSize,
                current->next
            );

            if (freePart == nullptr) {
                delete[] newData;
                return -1;
            }

            memcpy(newData, current->data, newSize);
            delete[] current->data;

            current->data = newData;
            current->initialized.swap(newInitialization);
            current->size = newSize;
            current->reallocated = true;
            current->next = freePart;

            mergeBlocks();
            return id;
        }

        // Case 2: Grow into the free block directly after this allocation.
        int extraSize = newSize - oldSize;
        Block* nextBlock = current->next;

        if (nextBlock != nullptr && nextBlock->free && nextBlock->size >= extraSize) {
            char* newData = new (nothrow) char[newSize];
            if (newData == nullptr)
                return -1;

            vector<bool> newInitialization;
            try {
                newInitialization = current->initialized;
                newInitialization.resize(newSize, false);
            }
            catch (...) {
                delete[] newData;
                return -1;
            }

            memcpy(newData, current->data, oldSize);
            delete[] current->data;
            current->data = newData;
            current->initialized.swap(newInitialization);

            if (nextBlock->size == extraSize) {
                current->next = nextBlock->next;
                delete nextBlock;
            }
            else {
                nextBlock->start += extraSize;
                nextBlock->size -= extraSize;
            }

            current->size = newSize;
            current->reallocated = true;
            return id;
        }

        // Case 3: The next block is not large enough, so move to another free block.
        // Allocate first. If this fails, the original allocation remains untouched.
        string originalType = current->type;
        int newId = allocate(newSize, "first", originalType);

        if (newId == -1)
            return -1;

        Block* newBlock = findAllocation(newId);
        if (newBlock == nullptr) {
            deallocate(newId);
            return -1;
        }

        memcpy(newBlock->data, current->data, oldSize);

        try {
            for (int index = 0; index < oldSize; index++)
                newBlock->initialized[index] = current->initialized[index];

            for (int index = oldSize; index < newSize; index++)
                newBlock->initialized[index] = false;
        }
        catch (...) {
            deallocate(newId);
            return -1;
        }

        newBlock->reallocated = true;
        deallocate(id);

        return newId;
    }

    // Release an allocation and merge adjacent free blocks.
    bool deallocate(int id) {
        Block* current = findAllocation(id);
        if (current == nullptr)
            return false;

        delete[] current->data;
        current->data = nullptr;
        current->initialized.clear();
        current->free = true;
        current->reallocated = false;
        current->id = 0;
        current->type = "free";

        mergeBlocks();
        return true;
    }

    // Join neighbouring free blocks into one larger free block.
    void mergeBlocks() {
        Block* current = head;

        while (current != nullptr && current->next != nullptr) {
            if (current->free && current->next->free) {
                Block* blockToDelete = current->next;
                current->size += blockToDelete->size;
                current->next = blockToDelete->next;
                delete blockToDelete;
            }
            else {
                current = current->next;
            }
        }
    }

    // Return every block as JSON for the memory-map display.
    string getMemory() const {
        string result = "[";
        Block* current = head;

        while (current != nullptr) {
            result += "{\"id\":" + to_string(current->id);
            result += ",\"start\":" + to_string(current->start);
            result += ",\"size\":" + to_string(current->size);
            result += ",\"end\":" + to_string(current->start + current->size - 1);
            result += ",\"free\":" + string(current->free ? "true" : "false");
            result += ",\"type\":\"" + current->type + "\"";
            result += ",\"reallocated\":" + string(current->reallocated ? "true" : "false");
            result += "}";

            if (current->next != nullptr)
                result += ",";

            current = current->next;
        }

        result += "]";
        return result;
    }

    // Return byte values for the inspection window. Uninitialized bytes are null.
    string getInspection(int id) const {
        Block* current = head;

        while (current != nullptr) {
            if (!current->free && current->id == id) {
                const int previewSize = min(current->size, 24);
                string result = "{\"success\":true";
                result += ",\"id\":" + to_string(current->id);
                result += ",\"start\":" + to_string(current->start);
                result += ",\"size\":" + to_string(current->size);
                result += ",\"type\":\"" + current->type + "\"";
                result += ",\"reallocated\":" + string(current->reallocated ? "true" : "false");
                result += ",\"bytes\":[";

                for (int index = 0; index < previewSize; index++) {
                    if (current->initialized[index]) {
                        result += to_string(static_cast<unsigned char>(current->data[index]));
                    }
                    else {
                        result += "null";
                    }

                    if (index + 1 < previewSize)
                        result += ",";
                }

                result += "]}";
                return result;
            }

            current = current->next;
        }

        return "{\"success\":false,\"message\":\"Allocation ID not found\"}";
    }

    // Calculate total, used and free memory.
    string getStats() const {
        int used = 0;
        int freeMemory = 0;
        Block* current = head;

        while (current != nullptr) {
            if (current->free)
                freeMemory += current->size;
            else
                used += current->size;

            current = current->next;
        }

        return "{\"total\":" + to_string(totalSize) +
               ",\"used\":" + to_string(used) +
               ",\"free\":" + to_string(freeMemory) + "}";
    }

    int getTotalSize() const {
        return totalSize;
    }

    // Delete every linked-list node and its data.
    ~MemorySimulator() {
        Block* current = head;

        while (current != nullptr) {
            Block* blockToDelete = current;
            current = current->next;
            delete[] blockToDelete->data;
            delete blockToDelete;
        }
    }
};
int main() {

    // The pool size is selected by the user in the frontend.
    MemorySimulator* memory = new MemorySimulator(2048);

    httplib::Server server;

    // Objects used through the common MemoryOperation parent class.
    MallocOperation mallocOperation;
    CallocOperation callocOperation;
    FreeOperation freeOperation;

    // CORS
    auto cors = [](httplib::Response& res) {

        res.set_header("Access-Control-Allow-Origin", "*");

        res.set_header("Access-Control-Allow-Methods",
                       "GET, POST, OPTIONS");

        res.set_header("Access-Control-Allow-Headers",
                       "Content-Type");
    };


    // OPTIONS API
    server.Options(R"(/.*)", [&](const httplib::Request&,
                                 httplib::Response& res) {

        cors(res);

        res.status = 204;
    });


    // RESET API: create a fresh memory pool with the requested size.
    server.Post("/reset", [&](const httplib::Request& req,
                               httplib::Response& res) {

        cors(res);

        int newSize = getNumber(req.body, "size");

        if (newSize < 1 || newSize > 102400) {
            res.status = 400;
            res.set_content(
                "{\"success\":false,\"message\":\"Pool size must be between 1 and 102400 bytes\"}",
                "application/json"
            );
            return;
        }

        // Delete the old linked list and initialize a new pool.
        delete memory;
        memory = new MemorySimulator(newSize);

        res.set_content(
            "{\"success\":true,\"total\":" + to_string(newSize) +
            ",\"memory\":" + memory->getMemory() + "}",
            "application/json"
        );
    });


    // ALLOCATION API
    server.Post("/allocate", [&](const httplib::Request& req,
                                 httplib::Response& res) {

        cors(res);

        int size = getNumber(req.body, "size");

        string strategy = getString(req.body, "strategy");

        string type = getString(req.body, "type");

        if (strategy != "first" &&
            strategy != "best" &&
            strategy != "worst") {

            res.status = 400;

            res.set_content(
                "{\"error\":\"Invalid strategy\"}",
                "application/json"
            );

            return;
        }

        if (type != "malloc" && type != "calloc") {

            res.status = 400;

            res.set_content(
                "{\"error\":\"Invalid allocation type\"}",
                "application/json"
            );

            return;
        }

        // Runtime polymorphism: choose the correct child operation.
        const MemoryOperation* operation;
        if (type == "malloc")
            operation = &mallocOperation;
        else
            operation = &callocOperation;

        int id = operation->execute(*memory, size, strategy);

        if (id == -1) {

            res.set_content(
                "{\"success\":false,\"message\":\"Allocation failed\"}",
                "application/json"
            );

            return;
        }

        string response =
            "{\"success\":true,\"id\":" +
            to_string(id) +
            ",\"memory\":" +
            memory->getMemory() + "}";

        res.set_content(response, "application/json");
    });


    // REALLOCATION API
    server.Post("/realloc", [&](const httplib::Request& req,
                                httplib::Response& res) {

        cors(res);

        int id = getNumber(req.body, "id");

        int size = getNumber(req.body, "size");

        int newId = memory->reallocate(id, size);

        if (newId == -1) {

            res.set_content(
                "{\"success\":false,\"message\":\"Reallocation failed\"}",
                "application/json"
            );

            return;
        }

        string response =
            "{\"success\":true,\"id\":" +
            to_string(newId) +
            ",\"memory\":" +
            memory->getMemory() + "}";

        res.set_content(response, "application/json");
    });


    // FREE API
    server.Post("/free", [&](const httplib::Request& req,
                             httplib::Response& res) {

        cors(res);

        int id = getNumber(req.body, "id");

        // The FreeOperation object performs the release.
        bool success = freeOperation.execute(*memory, id, "") != -1;

        string response =
            success ?
            "{\"success\":true,\"memory\":" +
            memory->getMemory() + "}" :
            "{\"success\":false,\"message\":\"Invalid ID\"}";

        res.set_content(response, "application/json");
    });


    // DISPLAY MEMORY API
    server.Get("/memory", [&](const httplib::Request&,
                              httplib::Response& res) {

        cors(res);

        res.set_content(
            memory->getMemory(),
            "application/json"
        );
    });


    // STATISTICS API
    server.Get("/stats", [&](const httplib::Request&,
                             httplib::Response& res) {

        cors(res);

        res.set_content(
            memory->getStats(),
            "application/json"
        );
    });


    // Serve index.html, app.js and style.css from the frontend folder.
    server.set_mount_point("/", "./frontend");

    cout << "\nDMA Simulator Started!" << endl;
    cout << "Open: http://localhost:8080" << endl;

    server.listen("127.0.0.1", 8080);

    delete memory;
    return 0;
}
