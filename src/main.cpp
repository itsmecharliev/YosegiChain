#include "./core/Blockchain.h"
#include "./network/P2PServer.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

int main(int argc, char** argv) {
  if (argc < 4) {
    cerr << "Usage: " << argv[0]
         << " <my_p2p_port> <peer_ip> <peer_p2p_port>\n"
         << "Example (Laptop A): " << argv[0] << " 6001 192.168.1.11 6001\n"
         << "Example (Laptop B): " << argv[0] << " 6001 192.168.1.10 6001\n";
    return 1;
  }

  uint16_t myPort = static_cast<uint16_t>(stoul(argv[1]));
  string peerIp = argv[2];
  uint16_t peerPort = static_cast<uint16_t>(stoul(argv[3]));

  Blockchain chain;
  P2PServer p2p(chain, myPort);
  p2p.Start();

  // Retry connecting to the seed peer until it's reachable.
  // On startup, one laptop may bind its listener before the other,
  thread dialer([&p2p, peerIp, peerPort] {
    bool connected = false;
    while (!connected) {
      connected = p2p.DialPeer(peerIp, peerPort);
      if (!connected) this_thread::sleep_for(chrono::seconds(2));
    }
  });
  dialer.detach();

  // Simple CLI mining loop for the demo.
  string input;
  while (true) {
    cout << "\nPress ENTER to mine a block ('q' to quit): ";
    if (!getline(cin, input)) break;
    if (input == "q") break;

    // update local blockchain
    auto snapshot = chain.Snapshot();
    uint32_t nextIndex = snapshot.empty() ? 1 : snapshot.back().GetIndex() + 1;

    Block block(nextIndex, "Block " + to_string(nextIndex) + " Data");
    chain.AddBlock(block);

    Block tip = chain.Snapshot().back();
    p2p.BroadcastNewBlock(tip);

    cout << "[node] mined block " << nextIndex << endl;
    cout << "this Hash: " << tip.GetHash().substr(0, 16) << "..." << endl;
    cout << "prevHash:  " << tip.prevHash.substr(0, 16) << "..." << endl;
    nextIndex++;
  }

  p2p.Stop();
  return 0;
}
