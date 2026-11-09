#include "Blockchain.h"
#include "ChainStore.h"

#include <cstdlib>
#include <iostream>

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

// cache genesis so chain validation does not have to recalcuate
const Block& trueGenesis() {
  static const Block g = MakeGenesis();
  return g;
}

Blockchain::Blockchain(const std::string& chainFilePath) : ChainFilePath(chainFilePath) {
  // try loading chain from file first
  try {
    Chain = chainstore::LoadFromFile(ChainFilePath);
  }
  catch (const std::exception& e) {
    std::cerr << "fatal: cannot load chain file " << ChainFilePath
              << "': " << e.what() << "\n";
    std::exit(1);
  }

  // create chain AND store
  if (Chain.empty()) {
    Chain.push_back(MakeGenesis());

    try {
      chainstore::SaveToFile(ChainFilePath, Chain);
    }
    catch (const std::exception& e) {
      std::cerr << "fatal: cannot save chain file " << ChainFilePath
                << "': " << e.what() << "\n";
      std::exit(1);
    }
  }
}

void Blockchain::AddBlock(Block newBlock) {
  std::lock_guard<std::mutex> lock(ChainMutex);
  newBlock.prevHash = GetLastBlock().GetHash();
  newBlock.MineBlock(DIFFICULTY);
  Chain.push_back(newBlock);
  PersistLocked();
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
    PersistLocked();
    return true;
}

Block Blockchain::GetLastBlock() const {
  return Chain.back();
}

void Blockchain::PersistLocked() {
  try {
    chainstore::SaveToFile(ChainFilePath, Chain);
  }
  catch (const std::exception& e) {
    std::cerr << "warning: failed to persist chain: " << e.what() << "\n";
  }
}

ValidateResult Blockchain::ValidateChain(const std::vector<Block>& chain) {
  if (chain.empty()) {
    return { false, 0, "empty chain" };
  }
  if (chain[0].GetHash() != trueGenesis().GetHash()) {
    return {false, 0, "genesis mismatch"};
  }

  for (size_t i = 1; i < chain.size(); ++i) {
    const Block& cur = chain[i];
    const Block& prev = chain[i-1];

    // check duplicate or out of order
    if (cur.GetIndex() != prev.GetIndex() + 1) {
      return {false, i, "index gap"};
    }

    // previous block mismatch
    if (cur.prevHash != prev.GetHash()) {
      return {false, i, "prevHash mismatch"};
    }

    // current mismatch
    if (cur.CalculateHash() != cur.GetHash()) {
      return {false, i, "hash mismatch"};
    }

    // difficulty mismatch
    if (cur.GetHash().substr(0, DIFFICULTY) != std::string(DIFFICULTY, '0')) {
      return {false, i, "insufficient PoW"};
    }
  }

  return {true, 0, ""};
}

bool Blockchain::ReplaceChain(std::vector<Block> newChain) {
  if (newChain.empty()) return false;

  std::lock_guard<std::mutex> lock(ChainMutex);
  Chain = std::move(newChain);
  PersistLocked();

  return true;
}
