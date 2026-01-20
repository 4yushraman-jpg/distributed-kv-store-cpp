#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>

// List of Backend Servers (The "Cluster")
std::vector<int> backend_ports = {8081, 8082};
std::atomic<int> current_server_index(0); // Round-Robin Counter

// Forward data between two sockets (Client <-> Backend)
// IMPROVED: Graceful shutdown logic
void relay_data(int source_fd, int dest_fd) {
    char buffer[1024];
    while (true) {
        int bytes_read = read(source_fd, buffer, 1024);
        
        if (bytes_read <= 0) { 
            // Connection closed or error. 
            // CRITICAL: Shut down the OTHER side to force the other thread to exit.
            shutdown(dest_fd, SHUT_RDWR); 
            break; 
        }
        
        write(dest_fd, buffer, bytes_read);
    }
}

void handle_client(int client_fd) {
    int backend_fd = -1;
    int attempts = 0;
    
    // RETRY LOGIC: Try servers until one works
    while (attempts < backend_ports.size()) {
        int backend_index = current_server_index.fetch_add(1) % backend_ports.size();
        int backend_port = backend_ports[backend_index];
        
        backend_fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in backend_addr;
        backend_addr.sin_family = AF_INET;
        backend_addr.sin_port = htons(backend_port);
        inet_pton(AF_INET, "127.0.0.1", &backend_addr.sin_addr);

        if (connect(backend_fd, (struct sockaddr*)&backend_addr, sizeof(backend_addr)) == 0) {
            std::cout << "[Load Balancer] Forwarding to Port " << backend_port << "\n";
            break; // Success!
        }
        
        std::cerr << "[Load Balancer] Port " << backend_port << " is DOWN. Retrying...\n";
        close(backend_fd); // Clean up failed socket
        backend_fd = -1;
        attempts++;
    }

    if (backend_fd < 0) {
        std::cerr << "[Load Balancer] All backends are DOWN.\n";
        close(client_fd);
        return;
    }

    // 3. Relay Traffic
    std::thread client_to_backend(relay_data, client_fd, backend_fd);
    std::thread backend_to_client(relay_data, backend_fd, client_fd);

    client_to_backend.join();
    backend_to_client.join();

    close(backend_fd);
    close(client_fd);
}

int main() {
    int lb_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(lb_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080); // LB listens on the main port 8080

    if (bind(lb_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed. Is port 8080 busy?\n";
        return -1;
    }
    listen(lb_fd, 3);

    std::cout << "Load Balancer running on port 8080...\n";
    std::cout << "Balancing traffic between ports 8081 and 8082.\n";

    int addrlen = sizeof(address);
    while(true) {
        int client_fd = accept(lb_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) continue;
        std::thread(handle_client, client_fd).detach();
    }
    return 0;
}