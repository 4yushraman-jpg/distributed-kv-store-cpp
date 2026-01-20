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

std::unordered_map<std::string, std::string> kv_store;
std::mutex kv_mutex;

// GLOBAL VARIABLE (Accessible by all functions)
std::string db_file = "dump.data"; 

// --- PERSISTENCE LAYER ---

void save_to_disk() {
    std::lock_guard<std::mutex> lock(kv_mutex);
    std::ofstream outFile(db_file); // Uses the global variable
    
    if (outFile.is_open()) {
        for (const auto& pair : kv_store) {
            outFile << pair.first << " " << pair.second << "\n";
        }
        outFile.close();
        std::cout << "[Persistence] Saved to " << db_file << "\n"; // Debug Log
    } else {
        std::cerr << "[Error] Could not open file: " << db_file << "\n";
    }
}

void load_from_disk() {
    std::ifstream inFile(db_file); // Uses the global variable
    std::string key, value;
    
    if (inFile.is_open()) {
        while (inFile >> key >> value) {
            kv_store[key] = value;
        }
        inFile.close();
        std::cout << "[Persistence] Loaded " << kv_store.size() << " keys from " << db_file << "\n";
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
        // Trim whitespace
        if (!request.empty() && request.back() == '\n') request.pop_back();
        if (!request.empty() && request.back() == '\r') request.pop_back();

        std::string response = process_request(request);
        send(client_socket, response.c_str(), response.length(), 0);
    }
    close(client_socket);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    // UPDATE GLOBAL VARIABLE BASED ON PORT
    db_file = "dump_" + std::to_string(port) + ".data";

    // Load data specific to this port
    load_from_disk();

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

    std::cout << "Redis-Lite running on port " << port << "...\n";
    
    int addrlen = sizeof(address);
    while(true) {
        int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) continue;
        std::thread(handle_client, client_socket).detach(); 
    }
    return 0;
}