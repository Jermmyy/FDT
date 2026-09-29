#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <cpr/cpr.h>

enum class WirePacketType {
    Info, HeaderOut, HeaderIn, DataOut, DataIn, SslOut, SslIn, Unknown
};

struct WirePacket {
    WirePacketType type;
    std::string data;
    size_t size;
    double timestampMs;
};

class NetworkWire {
public:
    static NetworkWire& Get() {
        static NetworkWire instance;
        return instance;
    }

    void PushPacket(WirePacketType type, const std::string& data) {
        auto now = std::chrono::steady_clock::now();
        if (m_startTime.time_since_epoch().count() == 0) m_startTime = now;

        WirePacket packet;
        packet.type = type;
        packet.data = data;
        packet.size = data.size();
        packet.timestampMs = std::chrono::duration<double, std::milli>(now - m_startTime).count();

        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.push_back(std::move(packet));

        // Prevent infinite memory growth (keep last 5000 packets)
        if (m_packets.size() > 5000) {
            m_packets.pop_front();
        }
    }

    std::vector<WirePacket> GetPacketsCopy() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return { m_packets.begin(), m_packets.end() };
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packets.clear();
    }

private:
    NetworkWire() {}
    std::mutex m_mutex;
    std::deque<WirePacket> m_packets;
    std::chrono::steady_clock::time_point m_startTime;
};

void DrawWireSharkPanel(bool* p_open);