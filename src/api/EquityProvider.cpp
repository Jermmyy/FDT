#include "EquityProvider.hpp"
#include <nlohmann/json.hpp>
#include <ctime>
#include <ixwebsocket/IXNetSystem.h>

using json = nlohmann::json;

// ------------------------------------------------------------------ lifecycle

FinnhubProvider::FinnhubProvider(std::string apiKey)
    : m_apiKey(std::move(apiKey))
{
}

FinnhubProvider::~FinnhubProvider()
{
    Stop();
}

void FinnhubProvider::Start()
{
    if (m_running.exchange(true))
        return; // already running

    ix::initNetSystem();

    m_ws.setUrl("wss://ws.finnhub.io:443/?token=" + m_apiKey);
    m_ws.enableAutomaticReconnection();
    m_ws.setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr& msg) { OnMessage(msg); }
    );
    m_ws.start(); // non-blocking; spins its own thread
    std::thread([this]() {
        for (int i = 0; i < 10; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            auto state = m_ws.getReadyState();
            const char* stateStr =
                state == ix::ReadyState::Open ? "Open" :
                state == ix::ReadyState::Connecting ? "Connecting" :
                state == ix::ReadyState::Closing ? "Closing" :
                state == ix::ReadyState::Closed ? "Closed" : "Unknown";
            printf("[Finnhub] State after %dms: %s\n", (i + 1) * 500, stateStr);
        }
        }).detach();
}

void FinnhubProvider::Stop()
{
    if (!m_running.exchange(false))
        return;

    m_ws.stop();
    ix::uninitNetSystem();
}

// --------------------------------------------------------------- subscriptions

void FinnhubProvider::Subscribe(const std::string& symbol)
{
    {
        std::lock_guard lock(m_subMutex);
        if (!m_subscribed.insert(symbol).second)
            return; // already subscribed
    }

    // Pre-populate cache
    {
        std::lock_guard lock(m_quoteMutex);
        m_quotes.emplace(symbol, EquityQuote{ symbol });
    }

    // Fetch initial quote
    FetchInitialQuote(symbol);

    if (m_running)
        SendSubscribe(symbol, true);
}

void FinnhubProvider::FetchInitialQuote(const std::string& symbol)
{
    std::string url = "https://finnhub.io/api/v1/quote?symbol=" + symbol + "&token=" + m_apiKey;
    auto response = m_httpClient.get(url, m_httpClient.createRequest(url));

    if (response->statusCode == 200)
    {
        try {
            json j = json::parse(response->body);
            std::lock_guard lock(m_quoteMutex);
            auto& q = m_quotes[symbol];

            q.price = j.value("c", 0.0);
            q.prevClose = j.value("pc", 0.0);
            //printf("Previous close price67: %.2f", q.prevClose);

            CalculateChangePct(q);

            q.timestamp = "Snapshot";

            //printf("[Finnhub] %s SYNC: Price=%.2f, Anchor=%.2f (Derived from %.2f%%)\n",
            //    symbol.c_str(), q.price, q.prevClose, q.changePct);
        }
        catch (...) {
            printf("[Finnhub] Failed to parse REST quote for %s\n", symbol.c_str());
        }
    }
}

// Helper
void FinnhubProvider::CalculateChangePct(EquityQuote& q) {
    if (q.prevClose > 0.0 && q.price > 0.0) {
        q.change = q.price - q.prevClose;
        q.changePct = (q.change / q.prevClose) * 100.0;
        q.valid = true;
    }
}

void FinnhubProvider::Unsubscribe(const std::string& symbol)
{
    {
        std::lock_guard lock(m_subMutex);
        if (!m_subscribed.erase(symbol))
            return;
    }

    if (m_running)
        SendSubscribe(symbol, false);

    std::lock_guard lock(m_quoteMutex);
    m_quotes.erase(symbol);
}

EquityQuote FinnhubProvider::GetLatest(const std::string& symbol)
{
    std::lock_guard lock(m_quoteMutex);
    auto it = m_quotes.find(symbol);
    if (it != m_quotes.end())
        return it->second;

    return EquityQuote{ symbol };
}

// -------------------------------------------------------------------- internal

void FinnhubProvider::SendSubscribe(const std::string& symbol, bool subscribe)
{
    json msg = {
        { "type",   subscribe ? "subscribe" : "unsubscribe" },
        { "symbol", symbol }
    };
    std::string payload = msg.dump();

    printf("[NET OUT] %s\n", payload.c_str());
    m_ws.send(payload);
}

// Re-send all subscriptions after a reconnect
void FinnhubProvider::ResubscribeAll()
{
    std::unordered_set<std::string> copy;
    {
        std::lock_guard lock(m_subMutex);
        copy = m_subscribed;
    }
    for (const auto& sym : copy)
        SendSubscribe(sym, true);
}

void FinnhubProvider::OnMessage(const ix::WebSocketMessagePtr& msg)
{
    switch (msg->type)
    {
    case ix::WebSocketMessageType::Open:
        printf("[NET] Connected to: %s\n", m_ws.getUrl().c_str());
        ResubscribeAll();
        break;

    case ix::WebSocketMessageType::Error:
        printf("[Finnhub] Error: %s\n", msg->errorInfo.reason.c_str());
        printf("[Finnhub] HTTP status: %d\n", msg->errorInfo.http_status);
        printf("[Finnhub] Wait time: %dms\n", (int)msg->errorInfo.wait_time);
        printf("[Finnhub] Retries: %d\n", msg->errorInfo.retries);
        break;

    case ix::WebSocketMessageType::Message:
    {
        printf("[Finnhub] Raw: %s\n", msg->str.c_str());
        try
        {
            json j = json::parse(msg->str);

            if (j.value("type", "") != "trade")
                break;

            const auto& trades = j.at("data");

            std::unordered_map<std::string, const json*> latest;
            for (const auto& trade : trades)
            {
                const std::string symbol = trade.value("s", "");
                if (!symbol.empty())
                    latest[symbol] = &trade; // last one wins
            }

            auto lock = std::lock_guard(m_quoteMutex);
            for (const auto& [symbol, trade] : latest)
            {
                auto it = m_quotes.find(symbol);
                if (it == m_quotes.end())
                    continue;

                EquityQuote& q = it->second;
                q.price = trade->value("p", 0.0);

                CalculateChangePct(q);

                // Finnhub trade timestamps are in milliseconds
                long long ms = trade->value("t", (long long)0);
                std::time_t t = static_cast<std::time_t>(ms / 1000);
                char buf[16] = {};
                std::strftime(buf, sizeof(buf), "%H:%M:%S", std::gmtime(&t));
                q.timestamp = std::string(buf) + " UTC";
            }
        }
        catch (const std::exception& e) {
            printf("[NET ERR] JSON Parse Failure: %s | Data: %s\n", e.what(), msg->str.c_str());
        }
        break;
    }

    default:
        break;
    }
}