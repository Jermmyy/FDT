#include "RateConfigs.hpp"

RateClient makeEFFR(const std::string& start, const std::string& end) {
    std::string url = "https://markets.newyorkfed.org/api/rates/all/search.json"
                      "?startDate=" + start + "&endDate=" + end + "&type=rate";
    RateClientConfig cfg;
    cfg.name      = "EFFR";
    cfg.url       = url;
    cfg.chartType = ChartType::Line;
    cfg.parser    = [](const nlohmann::json& j) {
        std::vector<RatePoint> out;
        if (!j.contains("refRates")) return out;
        for (const auto& item : j["refRates"])
            if (item.value("type", "") == "EFFR")
                out.push_back({ item["effectiveDate"].get<std::string>(),
                                item["percentRate"].get<double>() });
        return out;
    };
    return RateClient(std::move(cfg));
}

RateClient makeSOFR(const std::string& start, const std::string& end) {
    std::string url = "https://markets.newyorkfed.org/api/rates/all/search.json"
                      "?startDate=" + start + "&endDate=" + end + "&type=rate";
    RateClientConfig cfg;
    cfg.name      = "SOFR";
    cfg.url       = url;
    cfg.chartType = ChartType::Step;
    cfg.parser    = [](const nlohmann::json& j) {
        std::vector<RatePoint> out;
        if (!j.contains("refRates")) return out;
        for (const auto& item : j["refRates"])
            if (item.value("type", "") == "SOFR")
                out.push_back({ item["effectiveDate"].get<std::string>(),
                                item["percentRate"].get<double>() });
        return out;
    };
    return RateClient(std::move(cfg));
}

RateClient makeRP(int numOps) {
    std::string url = "https://markets.newyorkfed.org/api/rp/repo/all/results/last/"
                      + std::to_string(numOps) + ".json";
    RateClientConfig cfg;
    cfg.name      = "RP";
    cfg.url       = url;
    cfg.chartType = ChartType::Bar;
    cfg.parser    = [](const nlohmann::json& j) {
        std::vector<RatePoint> out;
        if (!j.contains("repo") || !j["repo"].contains("operations")) return out;
        for (const auto& op : j["repo"]["operations"]) {
            if (!op.contains("operationDate") || !op.contains("totalAmtAccepted")) continue;
            out.push_back({ op["operationDate"].get<std::string>(),
                            op["totalAmtAccepted"].get<double>() / 1e9 });
        }
        return out;
    };
    return RateClient(std::move(cfg));
}

RateClient makeRRP(int numOps) {
    std::string url = "https://markets.newyorkfed.org/api/rp/reverserepo/all/results/last/"
                      + std::to_string(numOps) + ".json";
    RateClientConfig cfg;
    cfg.name      = "RRP";
    cfg.url       = url;
    cfg.chartType = ChartType::Bar;
    cfg.parser    = [](const nlohmann::json& j) {
        std::vector<RatePoint> out;
        if (!j.contains("repo") || !j["repo"].contains("operations")) return out;
        for (const auto& op : j["repo"]["operations"]) {
            if (!op.contains("operationDate") || !op.contains("totalAmtAccepted")) continue;
            out.push_back({ op["operationDate"].get<std::string>(),
                            op["totalAmtAccepted"].get<double>() / 1e9 });
        }
        return out;
    };
    return RateClient(std::move(cfg));
}