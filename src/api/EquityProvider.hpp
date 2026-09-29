#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXHttpClient.h>

// DATA

struct EquityQuote
{
	std::string symbol;
	double price = 0.0f;
	double change = 0.0f;
	double changePct = 0.0f;
    double prevClose = 0.0f;
	std::string timestamp;
	bool valid = false;
};

// INTERFACE

class IEquityProvider
{
public:
    virtual ~IEquityProvider() = default;
	virtual void Subscribe(const std::string& symbol) = 0;
	virtual void Unsubscribe(const std::string& symbol) = 0;

	virtual EquityQuote GetLatest(const std::string& symbol) = 0;


	virtual void Start() = 0;
	virtual void Stop() = 0;

	virtual const char* Name() const = 0;
};

// FINNHUB

class FinnhubProvider : public IEquityProvider
{
public:
    explicit FinnhubProvider(std::string apiKey);
    ~FinnhubProvider() override;

    void Subscribe(const std::string& symbol)   override;
    void Unsubscribe(const std::string& symbol) override;

    EquityQuote GetLatest(const std::string& symbol) override;

    void Start() override;
    void Stop()  override;

    const char* Name() const override { return "Finnhub"; }

private:
    void OnMessage(const ix::WebSocketMessagePtr& msg);
    void SendSubscribe(const std::string& symbol, bool subscribe);
    void ResubscribeAll();
    void FetchInitialQuote(const std::string& symbol);
    void CalculateChangePct(EquityQuote& q);

    ix::HttpClient m_httpClient;

    std::string              m_apiKey;
    ix::WebSocket            m_ws;

    mutable std::mutex                          m_quoteMutex;
    std::unordered_map<std::string, EquityQuote> m_quotes;

    mutable std::mutex               m_subMutex;
    std::unordered_set<std::string>  m_subscribed;

    std::atomic<bool> m_running{ false };
};
