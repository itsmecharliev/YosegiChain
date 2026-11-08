#ifndef TEST_CHAIN_BLOCKCHAIN_H
#define TEST_CHAIN_BLOCKCHAIN_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <string>

#include "Block.h"

class Blockchain {
  public:
    static constexpr uint32_t DIFFICULTY = 3; // Desired Blockchain difficulty

    explicit Blockchain(const std::string& chainFilePath);
    void AddBlock(Block newBlock);  // Adding new Block to Blockchain
    bool TryAddExternalBlock(const Block& block);
    std::vector<Block> Snapshot() const;

  private:
    std::string ChainFilePath;
    std::vector<Block> Chain;       // Chain of Blocks
    mutable std::mutex ChainMutex;
    Block GetLastBlock() const;     // Obtain last block
    void PersistLocked();           // hold ChainMutex
};

#endif
