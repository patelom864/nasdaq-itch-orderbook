#include "itch/framer.hpp"

namespace itch {

const char* describe(FrameError error) noexcept {
    switch (error) {
    case FrameError::TruncatedPrefix: return "truncated length prefix";
    case FrameError::ZeroLength:      return "zero-length record";
    case FrameError::TruncatedRecord: return "record runs past end of file";
    case FrameError::UnknownType:     return "message type not defined by ITCH 5.0";
    case FrameError::LengthMismatch:  return "declared length disagrees with specification";
    }
    return "unknown framing error";
}

}  // namespace itch