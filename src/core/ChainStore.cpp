#include "ChainStore.h"

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

    std::string BlockToJsonLine(const Block& b) {
      std::ostringstream oss;
      oss << "{\"index\":" << b.GetIndex()
          << ",\"time\":" << b.GetTime()
          << ",\"nonce\":" << b.GetNonce()
          << ",\"prevHash\":\"" << JsonEscape(b.prevHash) << "\""
          << ",\"data\":\"" << JsonEscape(b.GetData()) << "\""
          << ",\"hash\":\"" << JsonEscape(b.GetHash()) << "\"}";

      return oss.str();
    }

  } // end of anonynouse helper namespace

  std::vector<Block> LoadFromFile(const std::string& path) {
    (void)path;
    return {};
  }

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
