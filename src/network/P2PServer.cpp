#include "P2PServer.h"
#include "Protocol.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <algorithm>

using namespace std;

// Initiate
P2PServer::P2PServer(Blockchain& chain, uint16_t listenPort)
  : Chain(chain),
    ListenPort(listenPort),
    ListenFd(-1),
    Running(false)
{}

// Destructor
P2PServer::~P2PServer() {
  Stop();
}

// Start server
void P2PServer::Start() {
  if (Running.load()) return; // If already running, do nothing

  Running.store(true);  // Set runnning flag
  thread([this] { ListenLoop(); }).detach();  // run accept loop in background
}

void P2PServer::Stop() {
  // If server is running, update flag to false and exit function
  if (!Running.exchange(false)) return;

  // active listening socket is >= 0
  if (ListenFd >= 0) {
    shutdown(ListenFd, SHUT_RDWR);  // close I/O
    close(ListenFd);  // return resources
    ListenFd = -1;  // reset "flag"
  }

  // shutdown peer connections
  lock_guard<mutex> lock(PeersMutex);
  for (int fd : PeerFds) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
  PeerFds.clear();
}

void P2PServer::ListenLoop() {
  // create TCP Socket
  ListenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (ListenFd < 0) {
    cerr << "p2p socket failed: " << strerror(errno) << endl;

    return;
  }

  // enable address reuse
  int opt = 1;
  setsockopt(ListenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // fill in address
  sockaddr_in addr{}; // initialize struct
  addr.sin_family = AF_INET;  // set as IPV4
  addr.sin_addr.s_addr = INADDR_ANY;  // bind to all interfaces
  addr.sin_port = htons(ListenPort);  // byte ordering

  // bind
  if (::bind(ListenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    cerr << "p2p port bind fail "
      << ListenPort << ": " << strerror(errno) << endl;

    close(ListenFd);
    ListenFd = -1;
    return;
  }

  // listen
  if (listen(ListenFd, 8) < 0) { // set at 8 for my demo. for real world, set to SOMAXCONN
    cerr << "p2p listen failed: " << strerror(errno) << endl;

    close(ListenFd);
    ListenFd = -1;
    return;
  }

  // display address and port information
  cout << "p2p listening on 0.0.0.0: " << ListenPort << endl;

  // accept loop
  while (Running.load()) {
    sockaddr_in peerAddr{}; // client address output
    socklen_t len = sizeof(peerAddr); // buffer size

    // accept
    int fd = accept(ListenFd, reinterpret_cast<sockaddr*>(&peerAddr), &len);

    // if accept fails: exit if shutting down, otherwise retry
    if (fd < 0) {
      if (!Running.load()) break;
      continue;
    }

    // register connection
    AddPeer(fd);
    cout << "p2p inbound peer from: " << inet_ntoa(peerAddr.sin_addr) << endl;

    // hand off detached worker
    thread([this, fd] { HandleConnection(fd); }).detach();
  }
}

bool P2PServer::DialPeer(const string& ip, uint16_t port) {
  // create TCP socket
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return false;

  // fill in address
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  // parse ip
  if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <=0) {
    close(fd);
    return false;
  }

  // connect to remote peer
  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return false;
  }

  // register connection
  AddPeer(fd);
  cout << "p2p outbound peer " << ip << ":" << port << "\n";

  // sync chain on connect
  SendFrame(fd, MakeFrame(MessageType::GetBlocks, ""));

  // hand off detached worker
  thread([this, fd] { HandleConnection(fd); }).detach();
  return true;
}

// broadcast to push new block
void P2PServer::BroadcastNewBlock(const Block& block) {
  BroadcastFrame(MakeFrame(MessageType::NewBlock, SerializeBlock(block)));
}

// broadcast to pull missing blocks
void P2PServer::BroadcastMissingBlocksRequest() {
  BroadcastFrame(MakeFrame(MessageType::GetBlocks, ""));
}

// send a frame to one peer
void P2PServer::SendFrame(int fd, const string& frame) {
  send(fd, frame.data(), frame.size(), 0);
}

// send a frame to every connected peer
void P2PServer::BroadcastFrame(const string& frame) {
  lock_guard<mutex> lock(PeersMutex);

  for (int fd : PeerFds) {
    send(fd, frame.data(), frame.size(), 0);
  }
}

// register new connection
void P2PServer::AddPeer(int fd) {
    lock_guard<mutex> lock(PeersMutex);
    PeerFds.push_back(fd);
}

// drop finished connection
void P2PServer::RemovePeer(int fd) {
    lock_guard<mutex> lock(PeersMutex);
    PeerFds.erase(remove(PeerFds.begin(), PeerFds.end(), fd), PeerFds.end());
}

void P2PServer::HandleConnection(int fd) {
  string buffer;
  char chunk[4096];

  // read loop: runs until peer disconnects or the server stops
  while (Running.load()) {
    ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
    if (n <= 0) break;  // at 0, closed, at < 0, error

    buffer.append(chunk, static_cast<size_t>(n));

    // extract newline-terminated frames from the buffer
    size_t pos; // to hold index of delimiter
    while ((pos = buffer.find('\n')) != string::npos) { // find delimiter and store in pos
      string frame = buffer.substr(0, pos); // extracted frame is data up to delimiter
      buffer.erase(0, pos + 1); // clear buffer
      if (!frame.empty()) ProcessFrame(fd, frame);  // skip blanks ie double newline
    }
  }

  // close connection
  RemovePeer(fd);
  close(fd);
  cout << "p2p peer disconnected (fd " << fd << ")" << endl;
}

void P2PServer::ProcessFrame(int fd, const string& frame) {
  // Payload layout
  MessageType type = static_cast<MessageType>(frame[0]);
  string payload = frame.substr(1);

  switch (type) {
    // peer push block to us
    case MessageType::NewBlock: {
      Block block = DeserializeBlock(payload);
      if (Chain.TryAddExternalBlock(block)) {
        auto snapshot = Chain.Snapshot();

        // silence duplicate rejection noise
        if (block.GetIndex() <= snapshot.back().GetIndex()) break;

        cout << "p2p accepted block " << block.GetIndex() << endl;
        cout << "chain now: " << snapshot.size() << " blocks, tip "
             << snapshot.back().GetHash().substr(0, 16) << "..." << endl;
      }
      /* NEED TO FIGURE OUT HOW TO DISPLAY NON-DUPLICATE FRAME REJECTION */
      //else cout << "p2p rejected block " << block.GetIndex() << endl;

      break;
    }
    // peer request our chain
    case MessageType::GetBlocks: {
      auto snapshot = Chain.Snapshot();
      for (const auto& block : snapshot) {
        string frame = MakeFrame(MessageType::Block, SerializeBlock(block));
        SendFrame(fd, frame);
      }
      break;
    }
    // peer send us one block in response to our request
    case MessageType::Block: {
      Block block = DeserializeBlock(payload);
      if (Chain.TryAddExternalBlock(block)) {
        cout << "p2p synced block " << block.GetIndex() << " from peer" << endl;
      }
      break;
    }
    // unknown message type byte
    default:
      cerr << "p2p unknown message type byte: " << frame[0] << endl;
      break;
  }

}
