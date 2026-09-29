#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

// One weekly observation from FRED
// FRED returns values as strings; "." means missing/not reported
struct FedPoint
{
    std::string date;   // "YYYY-MM-DD"
    double      value;  // in billions USD (scaled at parse time)
    bool        missing = false;
};

// A single H.4.1 series definition
struct FedSeriesConfig
{
    std::string fredId;     // FRED series ID  e.g. "WALCL"
    std::string label;      // Display label   e.g. "Total Assets"
    std::string category;   // Section group   e.g. "Assets"
    std::string unit;       // e.g. "Bil. USD"
    double      scale = 1.0; // multiply raw FRED value by this
                              // FRED stores most H.4.1 in millions -> *0.001 for billions
};

// Runtime state + data for one series
struct FedSeries
{
    FedSeriesConfig         cfg;
    std::vector<FedPoint>   data;       // sorted ascending by date
    bool                    loaded  = false;
    bool                    error   = false;
    std::string             errorMsg;

    double LatestValue()    const { return data.empty() ? 0.0 : data.back().value; }
    double PrevValue()      const { return data.size() < 2 ? 0.0 : data[data.size()-2].value; }
    double WoWChange()      const { return LatestValue() - PrevValue(); }
    const std::string& LatestDate() const {
        static const std::string kEmpty = "---";
        return data.empty() ? kEmpty : data.back().date;
    }
};

struct FedClientConfig
{
    std::string              apiKey;
    std::vector<FedSeriesConfig> series;  // all series to fetch
    int                      observationLimit = 104; // weeks of history (2 years default)
};

class FedClient
{
public:
    explicit FedClient(FedClientConfig cfg);

    // Fetch all series sequentially (call on a background thread)
    // Returns true if every series loaded without error
    bool fetchAll();

    // Fetch a single series by index — lets you parallelise externally if desired
    bool fetchOne(size_t idx);

    const std::string&              name()   const { return m_name; }
    const std::vector<FedSeries>&   series() const { return m_series; }
    std::vector<FedSeries>&         series()       { return m_series; }

    bool allLoaded() const;
    bool anyError()  const;

private:
    static std::string buildUrl(const FedSeriesConfig& s,
                                const std::string& apiKey,
                                int limit);
    static std::vector<FedPoint> parseObservations(const nlohmann::json& j,
                                                    double scale);

    std::string             m_name = "FED H.4.1";
    FedClientConfig         m_cfg;
    std::vector<FedSeries>  m_series;
};