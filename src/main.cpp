#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <vector>
#include <thread>
#include <fstream>
#include <list> // NEW: For Doubly Linked List

// --- LRU CACHE DATA STRUCTURES ---
struct CacheEntry {
    std::string value;
    std::list<std::string>::iterator list_iterator; // Pointer to the linked list node
};

size_t MAX_CAPACITY = 3; // Kept small for easy testing
std::list<std::string> lru_list; // Ordered keys (Front = MRU, Back = LRU)
std::unordered_map<std::string, CacheEntry> kv_store;
std::mutex kv_mutex;

std::string db_file = "dump.data";

// --- PERSISTENCE LAYER ---
void save_to_disk() {
    std::lock_guard<std::mutex> lock(kv_mutex);
    std::ofstream outFile(db_file);
    if (outFile.is_open()) {
        // Save in LRU order (Most recently used first)
        for (const auto& key : lru_list) {
            outFile << key << " " << kv_store[key].value << "\n";
        }
        outFile.close();
        std::cout << "[Persistence] Saved to " << db_file << "\n";
    }
}

void load_from_disk() {
    std::ifstream inFile(db_file);
    std::string key, value;
    if (inFile.is_open()) {
        while (inFile >> key >> value) {
            // Loading logic mimics a "SET" to rebuild LRU order
            if (lru_list.size() >= MAX_CAPACITY) {
                std::string lru_key = lru_list.back();
                lru_list.pop_back();
                kv_store.erase(lru_key);
            }
            lru_list.push_front(key);
            kv_store[key] = {value, lru_list.begin()};
        }
        inFile.close();
        std::cout << "[Persistence] Loaded " << kv_store.size() << " keys.\n";
    }
}

// --- CORE LOGIC ---
std::string process_request(std::string request) {
    std::stringstream ss(request);
    std::string command;
    ss >> command;

    if (command == "SET") {
        std::string key, value;
        ss >> key >> value;
        
        std::lock_guard<std::mutex> lock(kv_mutex);
        
        // 1. If Key Exists: Update value & Move to Front
        if (kv_store.find(key) != kv_store.end()) {
            lru_list.erase(kv_store[key].list_iterator); // O(1) removal
            lru_list.push_front(key);
            kv_store[key] = {value, lru_list.begin()};
            return "OK (Updated)\n";
        }

        // 2. If Key is New: Check Capacity
        if (lru_list.size() >= MAX_CAPACITY) {
            // EVICTION! Remove the guy at the back
            std::string lru_key = lru_list.back();
            lru_list.pop_back();
            kv_store.erase(lru_key);
            std::cout << "[LRU] Evicted key: " << lru_key << "\n"; // Debug log
        }

        // 3. Insert New Key
        lru_list.push_front(key);
        kv_store[key] = {value, lru_list.begin()};
        return "OK\n";
    } 
    else if (command == "GET") {
        std::string key;
        ss >> key;
        
        std::lock_guard<std::mutex> lock(kv_mutex);
        if (kv_store.find(key) != kv_store.end()) {
            // "Touching" a key moves it to the front (Most Recently Used)
            lru_list.erase(kv_store[key].list_iterator);
            lru_list.push_front(key);
            kv_store[key].list_iterator = lru_list.begin();
            
            return kv_store[key].value + "\n";
        } else {
            return "(nil)\n";
        }
    }
    else if (command == "SAVE") {
        save_to_disk();
        return "OK (Saved)\n";
    }
    
    return "(error) Unknown Command\n";
}

void handle_client(int client_socket) {
    char buffer[1024] = {0};
    int valread = read(client_socket, buffer, 1024);
    
    if (valread > 0) {
        std::string request(buffer, valread);
        if (!request.empty() && request.back() == '\n') request.pop_back();
        if (!request.empty() && request.back() == '\r') request.pop_back();

        std::string response = process_request(request);
        send(client_socket, response.c_str(), response.length(), 0);
    }
    close(client_socket);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) port = std::stoi(argv[1]);

    db_file = "dump_" + std::to_string(port) + ".data";
    load_from_disk(); // Restore state

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed on port " << port << "\n";
        return -1;
    }
    listen(server_fd, 3);
    std::cout << "LRU Redis-Lite running on port " << port << " (Capacity: " << MAX_CAPACITY << ")...\n";
    
    int addrlen = sizeof(address);
    while(true) {
        int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;
        std::thread(handle_client, client_socket).detach(); 
    }
    return 0;
}