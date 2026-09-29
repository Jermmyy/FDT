#pragma once
#include <cpr/cpr.h>
#include "NetworkWire.hpp"

namespace Net {

    // The callback that CPR/curl will fire for EVERY byte of data
    inline void InternalDebugCallback(cpr::DebugCallback::InfoType type, std::string data, intptr_t userdata) {
        WirePacketType pType = WirePacketType::Unknown;

        // Map CPR/Curl types to our internal types
        switch (type) {
        case cpr::DebugCallback::InfoType::TEXT:         pType = WirePacketType::Info; break;
        case cpr::DebugCallback::InfoType::HEADER_IN:    pType = WirePacketType::HeaderIn; break;
        case cpr::DebugCallback::InfoType::HEADER_OUT:   pType = WirePacketType::HeaderOut; break;
        case cpr::DebugCallback::InfoType::DATA_IN:      pType = WirePacketType::DataIn; break;
        case cpr::DebugCallback::InfoType::DATA_OUT:     pType = WirePacketType::DataOut; break;
        case cpr::DebugCallback::InfoType::SSL_DATA_IN:  pType = WirePacketType::SslIn; break;
        case cpr::DebugCallback::InfoType::SSL_DATA_OUT: pType = WirePacketType::SslOut; break;
        }

        NetworkWire::Get().PushPacket(pType, data);
    }

    // Variadic template to wrap CPR calls
    template <typename... Args>
    cpr::Response Get(Args&&... args) {
        return cpr::Get(std::forward<Args>(args)...,
            cpr::DebugCallback{ InternalDebugCallback });
    }
}