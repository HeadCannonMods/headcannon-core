// OpenTrack packet parsing tests.
//
// The packet is the only untrusted-input boundary in the tracking pipeline:
// the UDP socket binds INADDR_ANY, so any host on the network can send the
// 48-byte payload these functions parse. The tests below pin the parser's
// rejection of malformed input - in particular finite-but-out-of-float-range
// doubles, which used to slip past the isnan/isinf check and become +/-inf
// after the narrowing cast, poisoning the downstream view matrix with NaN.

#include "cameraunlock/protocol/opentrack_packet.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

int g_failures = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        std::cout << "  [PASS] " << name << "\n";
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++g_failures;
    }
}

// Build a 48-byte OpenTrack packet: x, y, z (cm), yaw, pitch, roll (deg).
void BuildPacket(uint8_t out[48], double x, double y, double z,
                 double yaw, double pitch, double roll) {
    std::memcpy(out + 0,  &x,     sizeof(double));
    std::memcpy(out + 8,  &y,     sizeof(double));
    std::memcpy(out + 16, &z,     sizeof(double));
    std::memcpy(out + 24, &yaw,   sizeof(double));
    std::memcpy(out + 32, &pitch, sizeof(double));
    std::memcpy(out + 40, &roll,  sizeof(double));
}

}  // namespace

int RunProtocolTests() {
    using cameraunlock::OpenTrackPacket;
    using cameraunlock::TrackingPose;
    using cameraunlock::PositionData;

    std::cout << "Protocol tests\n";

    const double kInf = std::numeric_limits<double>::infinity();
    const double kNan = std::numeric_limits<double>::quiet_NaN();

    // Valid packet: position in cm -> meters, rotation passes through.
    {
        uint8_t pkt[48];
        BuildPacket(pkt, 100.0, 200.0, -50.0, 10.0, -20.0, 5.0);
        TrackingPose pose;
        PositionData pos;
        const bool ok = OpenTrackPacket::TryParseAll(pkt, sizeof(pkt), pose, pos);
        Check(ok, "valid packet parses");
        Check(ok && std::fabs(pose.yaw - 10.0f) < 1e-4f, "yaw decoded");
        Check(ok && std::fabs(pos.x - 1.0f) < 1e-4f, "position cm->m");
    }

    // Length below the minimum is rejected.
    {
        uint8_t pkt[48] = {};
        TrackingPose pose;
        Check(!OpenTrackPacket::TryParse(pkt, 47, pose), "short packet rejected");
        Check(!OpenTrackPacket::TryParse(nullptr, 48, pose), "null data rejected");
    }

    // NaN / Inf in any field is rejected.
    {
        uint8_t pkt[48];
        TrackingPose pose;
        BuildPacket(pkt, 0, 0, 0, kNan, 0, 0);
        Check(!OpenTrackPacket::TryParse(pkt, sizeof(pkt), pose), "NaN rotation rejected");
        BuildPacket(pkt, 0, 0, 0, 0, kInf, 0);
        Check(!OpenTrackPacket::TryParse(pkt, sizeof(pkt), pose), "Inf rotation rejected");
        PositionData pos;
        BuildPacket(pkt, kInf, 0, 0, 0, 0, 0);
        Check(!OpenTrackPacket::TryParsePosition(pkt, sizeof(pkt), pos), "Inf position rejected");
    }

    // Regression: finite double that overflows float range must be rejected,
    // not silently turned into +/-inf by the narrowing cast.
    {
        uint8_t pkt[48];
        TrackingPose pose;
        PositionData pos;
        BuildPacket(pkt, 0, 0, 0, 1e300, 0, 0);
        Check(!OpenTrackPacket::TryParse(pkt, sizeof(pkt), pose),
              "finite-but-huge rotation rejected");
        BuildPacket(pkt, 1e300, 0, 0, 0, 0, 0);
        Check(!OpenTrackPacket::TryParsePosition(pkt, sizeof(pkt), pos),
              "finite-but-huge position rejected");
        BuildPacket(pkt, 0, 0, 0, 0, 0, -1e300);
        Check(!OpenTrackPacket::TryParseAll(pkt, sizeof(pkt), pose, pos),
              "finite-but-huge in TryParseAll rejected");
    }

    return g_failures;
}
