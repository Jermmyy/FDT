#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

struct RatePoint
{
    std::string date;
    double value;
};

enum class ChartType
{
    Line,
    Bar,
    Step
};

struct RateClientConfig
{
    std::string name;
    std::string url;
    ChartType chartType = ChartType::Line;
    std::function<std::vector<RatePoint>(const nlohmann::json &)> parser;
};

class RateClient
{
public:
    explicit RateClient(RateClientConfig cfg) : m_cfg(std::move(cfg)) {}

    bool fetch();

    const std::string &name() const { return m_cfg.name; }
    ChartType chartType() const { return m_cfg.chartType; }
    const std::vector<RatePoint> &data() const { return m_data; }

private:
    RateClientConfig m_cfg;
    std::vector<RatePoint> m_data;
};