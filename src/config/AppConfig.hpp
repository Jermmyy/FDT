#pragma once
#include <string>
#include <unordered_map>
#include <fstream>

class AppConfig {
public:
    static AppConfig& Instance() {
        static AppConfig instance;
        return instance;
    }

    bool Load(const std::string& path = "config.local") {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            m_values[key] = value;
        }
        return true;
    }

    const std::string& Get(const std::string& key) const {
        static const std::string empty;
        auto it = m_values.find(key);
        return it != m_values.end() ? it->second : empty;
    }

    bool Has(const std::string& key) const {
        return m_values.count(key) > 0;
    }

private:
    AppConfig() = default;
    std::unordered_map<std::string, std::string> m_values;
};