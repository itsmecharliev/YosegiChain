#include "./core/Blockchain.h"
#include "./network/P2PServer.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <stdexcept>

using namespace std;

// prototypes
static uint16_t ParsePort(const char* s);

int main(int argc, char** argv) {
  if (argc != 5) {
    cerr << "Usage: " << argv[0]
         << " <your_ip> <your_port> <peer_ip> <peer_port>\n"
         << "Example (Laptop A): " << argv[0] << " 192.168.1.10 6001 192.168.1.11 6002\n"
         << "Example (Laptop B): " << argv[0] << " 192.168.1.11 6002 192.168.1.10 6001\n";
    return 1;
  }

  string myIp = argv[1];
  uint16_t myPort;
  string peerIp = argv[3];
  uint16_t peerPort;

  try {
    myPort = ParsePort(argv[2]);
    peerPort = ParsePort(argv[4]);
  }
  catch (const exception& e) {
    cerr << "error: " << e.what() << "\n";
    return 1;
  }
  (void)myIp;

  std::string chainPath = "chain_" + std::to_string(myPort) + ".ndjson";
  Blockchain chain(chainPath);

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
  }

  p2p.Stop();
  return 0;
}

// helper to deal with improper characters in arguments
static uint16_t ParsePort(const char* s) {
  size_t pos = 0;
  unsigned long v;
  try {
    v = stoul(s, &pos);
  }
  catch (const out_of_range&) { throw runtime_error(string("too large: '") + s + "'"); }
  catch (const exception&) { throw runtime_error(string("not a number: '") + s + "'"); }

  if (pos != strlen(s)) throw runtime_error(string("trailing characters: '") + s + "'");
  if (v == 0 || v > 65535) throw runtime_error(string("out of range (1-65535): '") + s + "'");

  return static_cast<uint16_t>(v);
}
