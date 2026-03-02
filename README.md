# Multi-Threaded Web Server with IPC and Semaphores

**Made by:** Bernardo Reis 126004 | Fernando Santos 124808

## Overview

A HTTP/1.1. web server made purely in C. In this concrete implementation, as an example, we created a website simulating a fridge store.

## Features

- **LRU Cache with DLL:** To achieve a high performance state and almost instantaneous responses, we used LRU Cache, using the Doubly Linked List (DLL) structure and hashing.
- **HTTPS:** This project has a self-signed SSL Certificate, enabling the usage of HTTPS.
- **Statistics Dashboard:** The project contains a statistics API in order to assure a real-time dashboard.
- **Short Keep-Alive:** The project contains a short keep-alive implementation, ensuring all the information requested is sent in parallel processing to the user.
- **Highly customizable:** This project contemplates a [config file](server.conf) containing all the relevant variables that the users can change. All the methods work accordigly to the configuration file.
- **Log Rotation with Compression:** The access.log (server.log) is limited to 10MB, rotating and being compressed after reaching this quota. The compressed files are stored in old_logs folder. The compression only happens in logs that are not the less older log in old_logs folder.
- **Different Status Implementation:** Our implementation can handle multiple event types accordingly, handling all in a clean, user-friendly way.
- **Multi-Process Architecture:** The project implements a master-worker pattern, containing process hierarchy, a thread dispatcher, a statistics thread, and inter-process comunication with the application of Unix Domain Sockets (UDS) and a shared memory.
- **Thread-Safe and Stable:** The implementation uses mutexes and semaphores to prevent race conditions and memory leaks, ensuring stable operation under high load. Verified with Valgrind Helgrind for thread safety.
- **MIME Type Detection:** The implementation supports multiple MIME types: `.html`, `.css`, `.js`, `.jpg`, `.pdf`, `.png`.
- **Graceful Shutdown:** The server can be shutted down with a single CTRL+C on the terminal.

## Requirements

- **Operating System:** Linux (Ubuntu 20.04+ or similar)
- **Compiler:** GCC 9.0+ with C11 support
- **Libraries:**
  - POSIX threads (pthread)
  - POSIX real-time extensions (rt)
  - Standard C library
- **Needed Linux Commands (some Distros don't have):**
  - GNU make
  - logrotate
  - apachebench (for debugging)
  - Valgrind
*Installation may vary from distro to distro.*

## Usage

To run the server use **`./server`** in the project folder. There are also other important commands:

- `ab -n 1000 -c 10 -k https://localhost:8080/` (Debugging, low-load)
- `ab -n 10000 -c 100 -k https://localhost:8080/` (Debugging, high-load)
- `valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./server` (Debugging, memory leaks)
- `valgrind --tool=helgrind --log-file=helgrind.log ./server` (Debugging, race conditions)
- `time curl -s https://localhost:8080/ > /dev/null` (For cache miss testing)
- For cache hit testing do:

```bash
for i in {1..10}; do
    time curl -s https://localhost:8080/ > /dev/null
done
```

The -k in ApacheBench commands is optional.

## Known Issues

- Without the fullfillment of all the requirements, functions like log rotation may not work accordingly.
- The project cannot regenerate [server.conf](server.conf) in case of file corruption and has to be manually added again.

## References

[1] AI Assistance: Gemini 3 Thinking Pro, **in Oriented Learning mode**. Oriented Learning mode doesn't provide answers, yet, it teaches the user how to do it.
[2] The OpenSSL Project, "OpenSSL Documentation," [Online]. Available: <https://docs.openssl.org/>. [Accessed: Dec. 7, 2025].
[3] A. Isaiah, "A Complete Guide to Managing Log Files with Logrotate," Better Stack Community, Jan. 13, 2025. [Online]. Available: <https://betterstack.com/community/guides/logging/how-to-manage-log-files-with-logrotate-on-ubuntu-20-04/>. [Accessed: Dec. 7, 2025].
[4] All documents provided by the professor.
