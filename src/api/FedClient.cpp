#include "FedClient.hpp"
#include "cpr/cpr.h"
#include <iostream>
#include <stdexcept>

// ----------------------------------------------------------------
//  FedClient
// ----------------------------------------------------------------

FedClient::FedClient(FedClientConfig cfg)
    : m_cfg(std::move(cfg))
{
    m_series.reserve(m_cfg.series.size());
    for (auto& sc : m_cfg.series)
    {
        FedSeries s;
        s.cfg = sc;
        m_series.push_back(std::move(s));
    }
}

bool FedClient::fetchAll()
{
    bool ok = true;
    for (size_t i = 0; i < m_series.size(); ++i)
        ok &= fetchOne(i);
    return ok;
}

bool FedClient::fetchOne(size_t idx)
{
    if (idx >= m_series.size()) return false;

    FedSeries& s = m_series[idx];
    s.data.clear();
    s.loaded  = false;
    s.error   = false;
    s.errorMsg.clear();

    const std::string url = buildUrl(s.cfg, m_cfg.apiKey, m_cfg.observationLimit);

    auto r = cpr::Get(cpr::Url{ url },
                      cpr::Header{ {"accept", "application/json"} });

    if (r.status_code != 200)
    {
        s.error    = true;
        s.errorMsg = "HTTP " + std::to_string(r.status_code);
        std::cerr << "[FED/" << s.cfg.fredId << "] " << s.errorMsg << "\n";
        return false;
    }

    try
    {
        auto j   = nlohmann::json::parse(r.text);
        s.data   = parseObservations(j, s.cfg.scale);
        s.loaded = true;
    }
    catch (const std::exception& e)
    {
        s.error    = true;
        s.errorMsg = std::string("Parse error: ") + e.what();
        std::cerr << "[FED/" << s.cfg.fredId << "] " << s.errorMsg << "\n";
        return false;
    }

    if (s.data.empty())
    {
        // Not necessarily an error — could be a genuine gap — but worth logging
        std::cerr << "[FED/" << s.cfg.fredId << "] Warning: 0 observations returned\n";
    }

    return true;
}

// ----------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------

std::string FedClient::buildUrl(const FedSeriesConfig& s,
                                 const std::string& apiKey,
                                 int limit)
{
    // FRED observations endpoint
    // Docs: https://fred.stlouisfed.org/docs/api/fred/series_observations.html
    //
    // sort_order=desc  -> newest first, then we reverse on parse so storage is asc
    // limit            -> number of observations (weeks)
    return
        "https://api.stlouisfed.org/fred/series/observations"
        "?series_id="  + s.fredId +
        "&api_key="    + apiKey +
        "&file_type=json"
        "&sort_order=desc"
        "&limit="      + std::to_string(limit);
}

std::vector<FedPoint> FedClient::parseObservations(const nlohmann::json& j,
                                                     double scale)
{
    std::vector<FedPoint> out;

    if (!j.contains("observations"))
        throw std::runtime_error("Missing 'observations' key in FRED response");

    const auto& obs = j["observations"];
    out.reserve(obs.size());

    for (const auto& item : obs)
    {
        FedPoint pt;
        pt.date = item.value("date", "");

        const std::string valStr = item.value("value", ".");
        if (valStr == ".")
        {
            pt.missing = true;
            pt.value   = 0.0;
        }
        else
        {
            try   { pt.value = std::stod(valStr) * scale; }
            catch (...) { pt.missing = true; pt.value = 0.0; }
        }

        out.push_back(std::move(pt));
    }

    // FRED returned desc; reverse so index 0 = oldest, back() = latest
    std::reverse(out.begin(), out.end());

    return out;
}

bool FedClient::allLoaded() const
{
    for (const auto& s : m_series)
        if (!s.loaded) return false;
    return !m_series.empty();
}

bool FedClient::anyError() const
{
    for (const auto& s : m_series)
        if (s.error) return true;
    return false;
}