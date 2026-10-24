#include "Blockchain.h"

using namespace std;

namespace {
  // anonymous namespace to protect fixed genesis
  Block MakeGenesis() {
    Block genesis(0, "Genesis Block");
    genesis.SetTime(1787529600); // First day of capstone class
    genesis.SetNonce(0);
    genesis.prevHash = "";
    genesis.SetHash(genesis.CalculateHash());
    return genesis;
  }
}

Blockchain::Blockchain() {
  Chain.push_back(MakeGenesis());
  Difficulty = 3;   // 3 for test, 5-6 for demo
}

void Blockchain::AddBlock(Block newBlock) {
  std::lock_guard<std::mutex> lock(ChainMutex);
  newBlock.prevHash = GetLastBlock().GetHash();
  newBlock.MineBlock(Difficulty);
  Chain.push_back(newBlock);
}

std::vector<Block> Blockchain::Snapshot() const {
    std::lock_guard<std::mutex> lock(ChainMutex);
    return Chain;
}

bool Blockchain::TryAddExternalBlock(const Block& block) {
    std::lock_guard<std::mutex> lock(ChainMutex);

    // Reject duplicates / stale blocks
    if (block.GetIndex() <= Chain.back().GetIndex()) return false;

    // Reject blocks that don't link to our tip
    if (block.GetIndex() != Chain.back().GetIndex() + 1) return false;
    if (block.prevHash != Chain.back().GetHash()) return false;

    // Recompute the hash and check it matches what the peer sent —
    // otherwise a peer could claim any hash it wanted.
    if (block.CalculateHash() != block.GetHash()) return false;

    Chain.push_back(block);
    return true;
}

Block Blockchain::GetLastBlock() const {
  return Chain.back();
}
