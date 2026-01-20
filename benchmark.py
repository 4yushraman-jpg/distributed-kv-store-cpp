import socket
import threading
import time
import random

# CONFIGURATION
SERVER_IP = "127.0.0.1"
SERVER_PORT = 8080
NUM_THREADS = 100        # Simulate 100 concurrent users
REQUESTS_PER_THREAD = 10 # Each user sends 10 commands

success_count = 0
lock = threading.Lock()

def send_request(command):
    try:
        # Create a socket for every single request (like a real HTTP client)
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect((SERVER_IP, SERVER_PORT))
        client.send(command.encode())
        response = client.recv(1024).decode()
        client.close()
        return response
    except Exception as e:
        return None

def worker(thread_id):
    global success_count
    for i in range(REQUESTS_PER_THREAD):
        key = f"key_{thread_id}_{i}"
        value = f"value_{random.randint(1, 1000)}"
        
        # 1. SET
        resp_set = send_request(f"SET {key} {value}")
        
        # 2. GET
        resp_get = send_request(f"GET {key}")
        
        # Verify
        with lock:
            if resp_set == "OK\n" and resp_get == f"{value}\n":
                success_count += 1
            else:
                print(f"Error on Thread {thread_id}: Expected {value}, got {resp_get}")

print(f"--- Starting Stress Test ---")
print(f"Threads: {NUM_THREADS}")
print(f"Total Requests: {NUM_THREADS * REQUESTS_PER_THREAD * 2} (SET + GET)")

start_time = time.time()

threads = []
for i in range(NUM_THREADS):
    t = threading.Thread(target=worker, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

end_time = time.time()
duration = end_time - start_time

print(f"\n--- Results ---")
print(f"Successful Operations: {success_count} / {NUM_THREADS * REQUESTS_PER_THREAD}")
print(f"Time Taken: {duration:.4f} seconds")
print(f"Requests Per Second (RPS): {int((NUM_THREADS * REQUESTS_PER_THREAD * 2) / duration)}")