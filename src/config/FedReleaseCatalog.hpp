#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

class FedReleaseCatalog {
public:
    static FedReleaseCatalog& Get() {
        static FedReleaseCatalog instance;
        return instance;
    }

    bool LoadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[FedCatalog] Failed to open mappings file: " << filepath << "\n";
            return false;
        }

        try {
            nlohmann::json j;
            file >> j;

            m_aliases.clear();
            for (auto& [key, val] : j.items()) {
                if (val.is_number_integer()) {
                    m_aliases[Sanitize(key)] = val.get<int>();
                }
            }
            std::cout << "[FedCatalog] Loaded " << m_aliases.size() << " release mappings.\n";
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "[FedCatalog] JSON parse error in " << filepath << ": " << e.what() << "\n";
            return false;
        }
    }

    int ResolveReleaseId(const std::string& input) const {
        std::string clean = Sanitize(input);
        if (clean.empty()) return -1;

        // Direct numeric input fallback (e.g., user typed "FED 20")
        if (std::all_of(clean.begin(), clean.end(), ::isdigit)) {
            return std::stoi(clean);
        }

        auto it = m_aliases.find(clean);
        return (it != m_aliases.end()) ? it->second : -1;
    }

private:
    FedReleaseCatalog() = default;

    static std::string Sanitize(std::string str) {
        str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());
        str.erase(std::remove(str.begin(), str.end(), '\''), str.end());
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return str;
    }

    std::unordered_map<std::string, int> m_aliases;
};