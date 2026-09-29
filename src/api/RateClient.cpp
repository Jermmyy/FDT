#include "RateClient.hpp"
#include "cpr/cpr.h"
#include <iostream>

bool RateClient::fetch() {
    m_data.clear();
    auto r = cpr::Get(cpr::Url{ m_cfg.url },
                      cpr::Header{ {"accept", "application/json"} });

    if (r.status_code != 200) {
        std::cerr << "[" << m_cfg.name << "] HTTP " << r.status_code << "\n";
        return false;
    }

    try {
        auto j = nlohmann::json::parse(r.text);
        m_data = m_cfg.parser(j);
    } catch (const std::exception& e) {
        std::cerr << "[" << m_cfg.name << "] Parse error: " << e.what() << "\n";
        return false;
    }

    return !m_data.empty();
}