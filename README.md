# Virtual File System (VFS) Driver

## Overview
This project implements a user-space virtual file system (VFS) driver in C++. It simulates a FAT-like architecture over a virtual block device, managing low-level data serialization, cluster allocation, and directory entries without relying on the host OS file system abstractions. The driver is designed to efficiently map high-level file I/O requests to fixed-size sector operations.

## Architecture & Core Features
* **Block-Level I/O:** Interfaces directly with a simulated disk block device, executing read and write operations strictly in 512-byte sector granularity.
* **FAT-Based Cluster Management:** Implements a custom File Allocation Table (FAT) to track free space, manage cluster chains, and handle dynamic file resizing and sector relocation on a disk capacity up to 1 GiB.
* **File Descriptor Management:** Safely manages up to 8 concurrently open files, tracking individual state parameters such as read/write offsets, current cluster indices, and file truncation modes.
* **Directory Subsystem:** Maintains a robust root directory structure capable of storing up to 128 file entries, complete with custom `findFirst` and `findNext` APIs for directory traversal.
* **Data Serialization:** Handles seamless byte-to-sector assembly and disassembly, allowing arbitrary byte-length reads and writes across non-contiguous disk sectors.

## Technical Stack
* **Language:** C++
* **Concepts:** File Systems, Block Device Management, Memory Management, Data Serialization, OS Storage Mechanisms

## How to Build and Run
The project includes an embedded test suite (`simple_test.inc`) and can be compiled and executed directly.

* Compile the source code with standard strict flags: g++ -std=c++20 -Wall -pedantic main.cpp -o vfs_test
* Run the compiled test suite: ./vfs_test

## Disclaimer
*This project was developed as part of the Operating Systems course at the Faculty of Information Technology, CTU in Prague. The code demonstrates low-level system optimization and storage management.*
