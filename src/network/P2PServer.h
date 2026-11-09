#ifndef NETWORK_P2P_SERVER_H
#define NETWORK_P2P_SERVER_H

#include <atomic> // Control race conditions
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <map>  // mapping fd -> ip:port
#include "../core/Block.h"
#include "../core/Blockchain.h"

class P2PServer {
  public:
    P2PServer(Blockchain& chain, uint16_t listenPort);
    ~P2PServer(); // destructor

    void Start();
    void Stop();

    // for peer reachability in main
    bool DialPeer(const std::string& ip, uint16_t port);

    void BroadcastNewBlock(const Block& block);
    void BroadcastMissingBlocksRequest();

  private:
    Blockchain& Chain;
    uint16_t ListenPort;
    int ListenFd;
    std::atomic<bool> Running;
    std::map<int, std::string> Peers;
    std::mutex PeersMutex;

    void ListenLoop();
    void HandleConnection(int fd);
    void SendFrame(int fd, const std::string& frame);
    void BroadcastFrame(const std::string& frame);
    void AddPeer(int fd, const std::string& addr);
    void RemovePeer(int fd);
    std::string PeerName(int fd);
    void ProcessFrame(int fd, const std::string& frame, std::vector<Block>& pendingBlocks);
};

#endif
