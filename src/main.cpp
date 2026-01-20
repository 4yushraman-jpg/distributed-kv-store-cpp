#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <vector>
#include <thread> // NEW: Required for threading

std::unordered_map<std::string, std::string> kv_store;
std::mutex kv_mutex;

std::string process_request(std::string request) {
    std::stringstream ss(request);
    std::string command;
    ss >> command;

    if (command == "SET") {
        std::string key, value;
        ss >> key >> value;
        std::lock_guard<std::mutex> lock(kv_mutex);
        kv_store[key] = value;
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
    return "(error) Unknown Command\n";
}

// NEW: This function runs in a separate thread for EACH client
void handle_client(int client_socket) {
    char buffer[1024] = {0};
    int valread = read(client_socket, buffer, 1024);
    
    if (valread > 0) {
        std::string request(buffer);
        std::string response = process_request(request);
        send(client_socket, response.c_str(), response.length(), 0);
    }
    
    close(client_socket); // Close ONLY this client's connection
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 3);

    std::cout << "Redis-Lite (Multi-Threaded) running on port 8080...\n";
    
    int addrlen = sizeof(address);
    while(true) {
        // Main thread is BLOCKED here until a client connects
        int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        
        if (client_socket < 0) continue;

        // NEW: Spin up a thread immediately!
        // The main loop goes back to 'accept' instantly.
        std::thread client_thread(handle_client, client_socket);
        
        // 'detach' lets the thread run independently. 
        // We don't need to wait for it to finish.
        client_thread.detach(); 
    }
    return 0;
}