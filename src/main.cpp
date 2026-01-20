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
#include <fstream> // NEW: File I/O

std::unordered_map<std::string, std::string> kv_store;
std::mutex kv_mutex;
const std::string DB_FILE = "dump.data"; // The file where we save data

// --- PERSISTENCE LAYER ---

void save_to_disk() {
    std::lock_guard<std::mutex> lock(kv_mutex); // Lock while saving
    std::ofstream outFile(DB_FILE);
    
    if (outFile.is_open()) {
        for (const auto& pair : kv_store) {
            outFile << pair.first << " " << pair.second << "\n";
        }
        outFile.close();
    }
}

void load_from_disk() {
    std::ifstream inFile(DB_FILE);
    std::string key, value;
    
    if (inFile.is_open()) {
        while (inFile >> key >> value) {
            kv_store[key] = value;
        }
        inFile.close();
        std::cout << "[Persistence] Loaded " << kv_store.size() << " keys from disk.\n";
    } else {
        std::cout << "[Persistence] No existing database found. Starting fresh.\n";
    }
}

// --- COMMAND PROCESSING ---

std::string process_request(std::string request) {
    std::stringstream ss(request);
    std::string command;
    ss >> command;

    if (command == "SET") {
        std::string key, value;
        ss >> key >> value;
        
        {
            std::lock_guard<std::mutex> lock(kv_mutex);
            kv_store[key] = value;
        }
        // Auto-save logic could go here, but for now we use manual SAVE
        return "OK\n";
    } 
    else if (command == "GET") {
        std::string key;
        ss >> key;
        
        std::lock_guard<std::mutex> lock(kv_mutex);
        if (kv_store.find(key) != kv_store.end()) {
            return kv_store[key] + "\n";
        } else {
            return "(nil)\n";
        }
    }
    else if (command == "DEL") {
        std::string key;
        ss >> key;
        std::lock_guard<std::mutex> lock(kv_mutex);
        kv_store.erase(key);
        return "(integer) 1\n";
    }
    // NEW COMMAND
    else if (command == "SAVE") {
        save_to_disk();
        return "OK (Saved to Disk)\n";
    }
    
    return "(error) Unknown Command\n";
}

void handle_client(int client_socket) {
    char buffer[1024] = {0};
    int valread = read(client_socket, buffer, 1024);
    
    if (valread > 0) {
        std::string request(buffer);
        std::string response = process_request(request);
        send(client_socket, response.c_str(), response.length(), 0);
    }
    close(client_socket);
}

int main() {
    // 1. Load data before starting server
    load_from_disk();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 3);

    std::cout << "Redis-Lite (Persistent) running on port 8080...\n";
    
    int addrlen = sizeof(address);
    while(true) {
        int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;
        std::thread(handle_client, client_socket).detach(); 
    }
    return 0;
}