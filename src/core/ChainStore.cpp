#include "ChainStore.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace chainstore {

  // anonymous helper functions
  namespace {
    std::string JsonEscape(const std::string& s) {
      std::string out;
      out.reserve(s.size());

      for (char c : s) {

        switch (c) {
          case'"':  out += "\\\"";  break;
          case'\\': out += "\\\\";  break;
          case'\b': out += "\\b";   break;
          case'\f': out += "\\f";   break;
          case'\n': out += "\\n";   break;
          case'\r': out += "\\r";   break;
          case'\t': out += "\\t";   break;
          default:
            if (static_cast<unsigned char>(c) < 0x20) {
              // control characters: \u00XX
              char buf[7];
              std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c) & 0xFF);
              out += buf;
            }
            // non-escape keys, place directly
            else { out += c; }

        } // end switch
      } // end char for-loop

      return out;
    } // end of JsonEscape

    std::string JsonUnescape(const std::string& s) {
      std::string out;
      out.reserve(s.size());

      for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        // non-escape keys, copy directly
        if (c != '\\') { out += c; continue; }
        // edge case: deal with random trailing backslash
        if (i + 1 >= s.size()) { throw std::runtime_error("JsonUnescape: trailing backslash"); }

        // consume the char after escape backslash
        char next = s[++i];
        switch (next) {
          case '"':  out += '"';  break;
          case '\\': out += '\\'; break;
          case 'b':  out += '\b'; break;
          case 'f':  out += '\f'; break;
          case 'n':  out += '\n'; break;
          case 'r':  out += '\r'; break;
          case 't':  out += '\t'; break;
          case 'u': {
            // control characters: \u00XX
            if (i + 4 >= s.size()) {
              throw std::runtime_error("JsonUnescape: truncated \\u escape");
            }

            // 4 digits after 'u'
            std::string hex = s.substr(i + 1, 4);
            for (char h : hex) {
              if (!std::isxdigit(static_cast<unsigned char>(h))) {
                throw std::runtime_error("JsonUnescape: non-hex in \\u escape: " + hex);
              }
            }

            // security check
            unsigned code = static_cast<unsigned>(std::stoul(hex, nullptr, 16));
            if (code > 0xFF) {
              throw std::runtime_error("JsonUnescape: \\u escape >0xFF unsupported");
            }

            out += static_cast<char>(code);
            i += 4; // 4 digit consumption for outer loop
            break;
          }
          default:
            throw std::runtime_error(std::string("JsonUnescape: unknown escape \\") + next);
        }
      }

      return out;
    } // end of JsonUnescape

    /* find key's value in json object line and return substring */
    std::string NextFieldValue(const std::string& line, size_t& pos, const std::string& key) {
      // obtain key
      const std::string needle = "\"" + key + "\":";

      // address missing field
      size_t k = line.find(needle, pos);
      if (k == std::string::npos) { throw std::runtime_error("missing field \"" + key + "\""); }

      size_t v = k + needle.size();

      // string val: scan to closing quotation
      if (v < line.size() && line[v] == '"') {
        size_t start = ++v; // start after opening quotation
        while (v < line.size()) {
          if (line[v] == '\\') { v += 2; continue; } // count 2, one for \ and one for char
          if (line[v] == '"') break; // found closing quotation

          ++v; // traverse
        }

        // case no closing quotation is found
        if (v >= line.size()) { throw std::runtime_error("unterminated string for field \"" + key + "\""); }

        // store key successfully
        std::string raw = line.substr(start, v - start);
        pos = v + 1; // reposition pos past closing quotation
        return raw; // caller decides whether to unescape
      }
      // num val: scan to comma or '}'
      else {
        size_t  start = v;
        while (v < line.size() && line[v] != ',' && line[v] != '}') ++v;
        std::string raw = line.substr(start, v - start);
        pos = v;

        return raw; // caller parses as int
      }
    } // end of NextFieldValue

    Block ParseBlockLine(const std::string& line) {
      if (line.empty() || line[0] != '{') { throw std::runtime_error("line does not start with '{'"); }

      size_t pos = 0;
      Block b;

      b.SetIndex(static_cast<uint32_t>(std::stoul(NextFieldValue(line, pos, "index"))));
      b.SetTime(static_cast<time_t>(std::stol(NextFieldValue(line, pos, "time"))));
      b.SetNonce(std::stoll(NextFieldValue(line, pos, "nonce")));
      b.prevHash = NextFieldValue(line, pos, "prevHash"); // hex, no escapes
      b.SetData(JsonUnescape(NextFieldValue(line, pos, "data")));
      b.SetHash(NextFieldValue(line, pos, "hash")); // hex, no escapes

      return b;
    } // end of ParseBlockLine

    std::string BlockToJsonLine(const Block& b) {
      std::ostringstream oss;
      oss << "{\"index\":" << b.GetIndex()
          << ",\"time\":" << b.GetTime()
          << ",\"nonce\":" << b.GetNonce()
          << ",\"prevHash\":\"" << JsonEscape(b.prevHash) << "\""
          << ",\"data\":\"" << JsonEscape(b.GetData()) << "\""
          << ",\"hash\":\"" << JsonEscape(b.GetHash()) << "\"}";

      return oss.str();
    } // end of BlockToJsonLine

  } // end of anonynouse helper namespace

  std::vector<Block> LoadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
      // LEAVING EMPTY FOR NOW
      // Test harness ndjson first, then come back and test bad files

      return {};
    }

    std::vector<Block> chain;
    std::string line;
    size_t lineNum = 0;

    // traverse lines
    while (std::getline(in, line)) {
      ++lineNum;

      // deal with blank lines
      if (line.empty()) { continue; }

      try {
        chain.push_back(ParseBlockLine(line));
      }
      catch (const std::exception& e) {
        throw std::runtime_error("LoadFromFile: " + path + ":" + std::to_string(lineNum) + ": " + e.what());
      }
    }

    // bad file path
    if (in.bad()) { throw std::runtime_error("LoadFromFile: I/O error reading " + path); }

    return chain;
  } // end of LoadFromFile

  void SaveToFile(const std::string& path, const std::vector<Block>& chain) {
    const std::string tmpPath = path + ".tmp";

    {
      std::ofstream out(tmpPath, std::ios::trunc);
      if (!out) {
        throw std::runtime_error("SaveToFile: cannot open '" + tmpPath + "' for writing");
      }
      for (const Block& b : chain) {
        out << BlockToJsonLine(b) << '\n';
      }
      out.flush();
      if (!out) {
        throw std::runtime_error("SaveToFile: write failed for '" + tmpPath + "'");
      }
    } // close file before rename

    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
      throw std::runtime_error("SaveToFile: rename '" + tmpPath + "' -> '" + path + "' failed");
    }
  }

} // end of chainstore namespace
