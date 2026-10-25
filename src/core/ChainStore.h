#ifndef CORE_CHAIN_STORE_H
#define CORE_CHAIN_STORE_H

#include <string>
#include <vector>
#include "Block.h"

namespace chainstore {
  std::vector<Block> LoadFromFile(const std::string& path);
  void SaveToFile(const std::string& path, const std::vector<Block>& chain);
}

#endif
