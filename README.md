# CSCI 490 Capstone: Yosegi Chain
A minimal peer-to-peer blockchain implemented in C++17.
This program is designed to demonstrate cross-host block propagation and chain synchronization over a real network.

The name is based on yosegi-zaiku, a Japanese mosaic woodwork technique developed in Hakone, Japan during the Edo period. The technique utilizes the natural colors of various trees to create complex patterns.

# How to compile and run:
This program is currently primarily supporting Linux only.
### Dependencies needed:
g++, make
## To make:
Make sure your current directory is inside the 'src/' folder.
```cd src/```
### Makefile
Make sure you have the 'make' dependency.
Run ```make```
### One liner compile without 'make':
```g++ -std=c++17 -pthread -O2 -Wall -Wextra main.cpp core/*.cpp cryptography/*.cpp network/*.cpp -o TestChain```

### How to Run:
./TestChain <my_p2p_port> <peer_ip> <peer_p2p_port>

### Laptop A
./TestChain 6001 192.168.1.11 6001

### Laptop B
./TestChain 6001 192.168.1.10 6001

# References
1. The initial creation of this code started off as a tutorial by Dave Nash
in his [BUILD A BLOCKCHAIN WITH C++](https://davenash.com/2017/10/build-a-blockchain-with-c/) tutorial.

2. The [SHA256](https://www.zedwood.com/article/cpp-sha256-function) function that I imported is from zedwood.
