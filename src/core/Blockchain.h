#ifndef TEST_CHAIN_BLOCKCHAIN_H
#define TEST_CHAIN_BLOCKCHAIN_H

#include <cstdint>
#include <vector>
#include <mutex>
#include "Block.h"

class Blockchain {
  public:
    static constexpr uint32_t DIFFICULTY = 3; // Desired Blockchain difficulty

    Blockchain();                   // Constructor
    void AddBlock(Block newBlock);  // Adding new Block to Blockchain
    bool TryAddExternalBlock(const Block& block);
    std::vector<Block> Snapshot() const;

  private:
    std::vector<Block> Chain;       // Chain of Blocks
    mutable std::mutex ChainMutex;
    Block GetLastBlock() const;     // Obtain last block
};

#endif
