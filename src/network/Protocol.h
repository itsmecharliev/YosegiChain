#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include "../core/Block.h"

/*
 * Format:
 * <message type byte><payload>\n
 * Payload is pipe-delimited fields.
 * Frames are newline-terminated so the receiver can split.
 */


/*
 * Message types:
 * 'G' - an empty payload -> Request peer's full blockchain.
 * 'B' - a block payload -> One frame per block, sent in response to GetBlocks.
 * 'N' - a block payload -> Announces appended block and append block if valid.
 * 'E' - an empty payload -> Terminate a block stream
 */
enum class MessageType : uint8_t {
  GetBlocks = 'G',
  Block = 'B',
  NewBlock = 'N',
  BlocksEnd = 'E'
};

/*
 * Block Payload Layout:
 * index - block height
 * time - unix timestamp
 * nonce - mining counter
 * prevHash - hash of parent block
 * data - block payload
 * hash - current block's hash
 */
inline std::string SerializeBlock(const Block& block) {
  std::ostringstream oss;

  oss <<  block.GetIndex()  <<  '|'
      <<  block.GetTime()   <<  '|'
      <<  block.GetNonce()  <<  '|'
      <<  block.prevHash    <<  '|'
      <<  block.GetData()   <<  '|'
      <<  block.GetHash();

  return oss.str();
}

inline Block DeserializeBlock(const std::string& payload) {
  std::istringstream iss(payload);
  std::string token;
  Block block;

  // capture and return token
  auto nextField = [&](const char* name) -> std::string {
    // throw if ending early
    if (!std::getline(iss, token, '|')) {
      throw std::runtime_error(std::string("DeserializeBlock: missing field'") + name + "'");
    }
    return token;
  };

  // parse unsigned int
  auto parseU64 = [](const std::string& s, const char* name) -> uint64_t {
    try {
      size_t pos = 0;
      uint64_t v = std::stoull(s, &pos);
      // stop too early
      if (pos != s.size()) {
        throw std::runtime_error(std::string("DeserializeBlock: ") + name + " has trailng characters: '" + s + "'");
      }
      return v;
    }
    catch (const std::exception& e) {
      // throw again to avoid double wrap
      if (std::string(e.what()).find("DeserializeBlock") != std::string::npos) {
        throw;
      }
      // wrap for error detail
      throw std::runtime_error(std::string("DeserializeBlock: ") + name + " is not a number: '" + s + "'");
    }
  };

  // parse int
  auto parseInt64 = [](const std::string& s, const char* name) -> int64_t {
    try {
      size_t pos = 0;
      int64_t v = std::stoll(s, &pos);
      // stop too early
      if (pos != s.size()) {
        throw std::runtime_error(std::string("DeserializeBlock: ") + name + " has trailing characters: '" + s + "'");
      }
      return v;
    }
    catch (const std::exception& e) {
      // throw again to avoid double wrap
      if (std::string(e.what()).find("DeserializeBlock") != std::string::npos) {
        throw;
      }
      // wrap for error detail
      throw std::runtime_error(std::string("DeserializeBlock: ") + name + " is not a number: '" + s + "'");
    }
  };

  // read fields in same order as SerializeBlock
  block.SetIndex(static_cast<uint32_t>(parseU64(nextField("index"), "index")));
  block.SetTime(static_cast<time_t>(parseInt64(nextField("time"), "time")));
  block.SetNonce(parseInt64(nextField("nonce"), "nonce"));
  block.prevHash = nextField("prevHash");
  block.SetData(nextField("data"));
  block.SetHash(nextField("hash"));

  return block;
}

inline std::string MakeFrame(MessageType type, const std::string& payload) {
  std::string frame;

  // prepares room for: message type + payload + end of frame
  frame.reserve(payload.size() + 2);

  frame.push_back(static_cast<char>(type)); // message type
  frame.append(payload);                    // payload
  frame.push_back('\n');                    // end of frame

  return frame;
}

#endif
