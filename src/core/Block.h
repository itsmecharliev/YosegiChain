#ifndef TEST_CHAIN_BLOCK_H
#define TEST_CHAIN_BLOCK_H

#include <cstdint>    // To standardize data type sizes
#include <iostream>   // For input and output
#include <sstream>
#include <string>
#include <ctime>

class Block {
  public:
    Block() = default;
    Block(uint32_t indexIn, const std::string &dataIn);  // Constructor

    std::string prevHash;  // Store previous block's hash

    void MineBlock(uint32_t difficulty);  // Find valid Nonce and calculating Hash
    std::string CalculateHash() const;    // Calculate Hash

    // Getters
    uint32_t    GetIndex() const { return Index; }
    int64_t     GetNonce() const { return Nonce; }
    time_t      GetTime()  const { return Time; }
    std::string GetData()  const { return Data; }
    std::string GetHash()  const { return Hash; }

    // Setters
    void SetIndex(uint32_t i)          { Index = i; }
    void SetNonce(int64_t n)           { Nonce = n; }
    void SetTime(time_t t)             { Time = t; }
    void SetData(const std::string& d) { Data = d; }
    void SetHash(const std::string& h) { Hash = h; }

  private:
    uint32_t Index = 0; // Position in chain
    int64_t Nonce = 0;  // Value to further complicate hash. Essentially a salt of sorts

    std::string Data;   // Block's content
    std::string Hash;   // Block's Hash
    time_t Time = 0;    // Time of Block's Creation
};

#endif
