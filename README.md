Distributed Key-Value Store (C++)
A high-performance, distributed key-value store built from scratch in C++. It features a custom Application-Layer Load Balancer, Sharded Persistence, and an O(1) LRU Cache Eviction Policy.

Designed to handle concurrent client connections using raw POSIX sockets and multithreading, deployed via Docker for reproducibility.

Key Features
Distributed Architecture: Custom Load Balancer distributes TCP traffic across multiple backend nodes using a Round-Robin scheduling algorithm.

O(1) Memory Management: Implements a Least Recently Used (LRU) eviction policy using a custom Doubly Linked List and Hash Map combination to strictly cap memory usage.

Concurrency: Handles thousands of concurrent requests using a Thread-per-Client model with fine-grained mutex locking for thread safety.

Sharded Persistence: ACID-compliant data durability where each node manages its own disk storage, preventing race conditions and data loss.

Infrastructure as Code: Fully containerized using Docker and Docker Compose, allowing a one-click launch of the entire cluster (LB + Nodes).

Tech Stack
Language: C++ (Standard Library, POSIX API)
Networking: Raw TCP Sockets (<sys/socket.h>)
Deployment: Docker, Docker Compose
Build System: CMake, Make
Testing: Netcat, Python (Benchmarking)

Architecture
[ Client ] -> [ Load Balancer (Port 8080) ]
                      |
        +-------------+-------------+
        |                           |
[ Node A (Port 8081) ]      [ Node B (Port 8082) ]
(Sharded Data/LRU)          (Sharded Data/LRU)

Quick Start (Docker)
You can spin up the entire cluster (Load Balancer + 2 Storage Nodes) with a single command.

Prerequisites
Docker & Docker Compose

1. Launch the Cluster
   sudo docker compose up --build
   You will see logs indicating that the Load Balancer and Nodes are running.

2. Interact with the Store
   Open a new terminal to act as a client. The Load Balancer listens on port 8080.

   Set a Key (Writes are distributed round-robin):
   echo "SET my_key hello_world" | nc localhost 8080

   Get a Key:
   echo "GET my_key" | nc localhost 8080
   Output: hello_world

   Test Persistence (Data survives restart):
   echo "SAVE" | nc localhost 8080
   Restarts automatically via Docker if you stop/start

Internals & Algorithms
1. LRU Cache (Least Recently Used)
   To prevent memory exhaustion, each node enforces a maximum capacity.
   Data Structure: unordered_map + list (Doubly Linked List).

   Logic:
   Access (GET): Move the accessed node to the front of the list O(1).
   Insertion (SET): Add to front. If capacity is exceeded, remove the tail node O(1).

2. Custom Load Balancer
   The load balancer accepts connections on Port 8080 and relays traffic to backend nodes (8081, 8082).
   Algorithm: Round Robin.
   Fault Tolerance: If a node is down, the LB detects the connection failure and automatically retries the next available node.

3. Sharded Persistence
   Instead of a central database file, each node writes to its own isolated dump file (dump_8081.data, dump_8082.data). This eliminates file-lock contention and allows nodes to persist data in parallel.

Performance
Throughput: ~3,300 Requests/Second (Tested on local loopback).
Latency: Sub-millisecond for cached reads.

Future Improvements
Implement Consistent Hashing for better data distribution.
Add Master-Slave Replication for high availability.
Upgrade to Non-Blocking I/O (epoll) for C10K handling.