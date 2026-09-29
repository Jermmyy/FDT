#include "FocusPanel.hpp"
#include "../core/PanelRegistry.hpp"
#include "../components/FocusComponents.hpp"
#include "../components/PanelComponents.hpp"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

// FocusClient

FocusClient::FocusClient(std::shared_ptr<IEquityProvider> provider) : m_provider(std::move(provider)) {}

FocusClient::~FocusClient()
{
    // If no other client cares about it -> unsubscribe cleanly
    std::string sym;
    {
        std::lock_guard lock(m_mutex);
        sym = m_symbol;
    }
    if (!sym.empty() && m_provider)
        m_provider->Unsubscribe(sym);
}

void FocusClient::SetSymbol(const std::string& symbol)
{
    if (symbol.empty()) return;

    std::string old;
    {
        std::lock_guard lock(m_mutex);
        if (m_symbol == symbol) return;
        old = m_symbol;
        m_symbol = symbol;
    }

    // Swap subscription on the shared provider, no new sub needed
    if (m_provider)
    {
        m_provider->Subscribe(symbol);

        if (!old.empty()) m_provider->Unsubscribe(old);
    }
}

EquityQuote FocusClient::GetQuote() const
{
    std::string sym;
    {
        std::lock_guard lock(m_mutex);
        sym = m_symbol;
    }

    // internal thread-safety (surely)
    return (m_provider && !sym.empty())
        ? m_provider->GetLatest(sym)
        : EquityQuote{};
}

std::string FocusClient::GetSymbol() const
{
    std::lock_guard lock(m_mutex);
    return m_symbol;
}

// FocusPanel

namespace UI
{
    // Defined FIRST so SpawnFocusPanel can see it
    void DrawFocusPanel(entt::registry& registry, entt::entity entity)
    {
        auto& clientComp = registry.get<FocusClientComp>(entity);
        auto& flashComp = registry.get<FocusRenderStateComp>(entity);
        auto& tickerComp = registry.get<TickerComp>(entity);

        if (clientComp.client->GetSymbol() != tickerComp.symbol) {
            clientComp.client->SetSymbol(tickerComp.symbol);
        }

        const std::string symbol = clientComp.client->GetSymbol();
        const EquityQuote quote = clientComp.client->GetQuote();

        if (quote.valid)
        {
            float dt = ImGui::GetIO().DeltaTime;
            if (flashComp.lastPrice != 0.0 && quote.price != flashComp.lastPrice) {
                flashComp.flashDir = quote.price > flashComp.lastPrice ? 1 : -1;
                flashComp.flashTimer = 0.4f;
            }
            flashComp.lastPrice = quote.price;
            if (flashComp.flashTimer > 0.0f) flashComp.flashTimer -= dt;

            ImVec4 baseColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (quote.changePct > 0.01f)       baseColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            else if (quote.changePct < -0.01f) baseColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

            ImVec4 flashColor = flashComp.flashDir == 1 ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            ImVec4 currentColor = baseColor;

            if (flashComp.flashTimer > 0.0f) {
                float t = flashComp.flashTimer / 0.4f;
                currentColor.x = baseColor.x + (flashColor.x - baseColor.x) * t;
                currentColor.y = baseColor.y + (flashColor.y - baseColor.y) * t;
                currentColor.z = baseColor.z + (flashColor.z - baseColor.z) * t;
            }

            float windowWidth = ImGui::GetContentRegionAvail().x;
            float windowHeight = ImGui::GetContentRegionAvail().y;

            float fontScale = std::max(1.0f, windowHeight / 40.0f);
            ImGui::SetWindowFontScale(fontScale);

            float lineSpacing = ImGui::GetTextLineHeightWithSpacing();
            float totalRightHeight = lineSpacing * 2.0f;

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (windowHeight - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::Text("%s", symbol.c_str());

            char priceBuf[32], pctBuf[32];
            const char* sign = (quote.changePct >= 0) ? "+" : "";
            std::snprintf(priceBuf, sizeof(priceBuf), "$%.2f", quote.price);
            std::snprintf(pctBuf, sizeof(pctBuf), "%s%.2f%%", sign, quote.changePct);

            float priceWidth = ImGui::CalcTextSize(priceBuf).x;
            float pctWidth = ImGui::CalcTextSize(pctBuf).x;
            float rightBlockWidth = std::max(priceWidth, pctWidth);

            ImGui::SameLine(ImGui::GetWindowWidth() - rightBlockWidth - ImGui::GetStyle().WindowPadding.x);
            ImGui::SetCursorPosY((windowHeight - totalRightHeight) * 0.5f + ImGui::GetStyle().FramePadding.y + 20.0f);

            ImGui::BeginGroup();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (rightBlockWidth - priceWidth));
            ImGui::TextColored(currentColor, "%s", priceBuf);

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (rightBlockWidth - pctWidth));
            ImGui::TextColored(currentColor, "%s", pctBuf);
            ImGui::EndGroup();

            ImGui::SetWindowFontScale(1.0f);
        }
        else
        {
            ImGui::TextDisabled("Waiting...");
        }
    }

    // Defined SECOND so it can reference DrawFocusPanel cleanly
    entt::entity SpawnFocusPanel(const std::string& symbol, std::shared_ptr<IEquityProvider> provider)
    {
        auto& registry = PanelRegistry::Get().GetRegistry();
        std::string uniqueTitle = symbol + "##focus";

        // 1. TOGGLE CHECK: If this panel is already open, destroy it and exit
        auto view = registry.view<TickerComp, WindowComp>();
        for (auto [entity, ticker, win] : view.each()) {
            if (ticker.symbol == symbol && win.title == uniqueTitle) {
                registry.destroy(entity);
                return entt::null; // Toggled off!
            }
        }

        // 2. Otherwise, create it fresh as an EnTT entity
        auto entity = registry.create();

        registry.emplace<WindowComp>(entity, uniqueTitle, true);
        registry.emplace<PanelMeta>(entity, PanelMeta{ "FOCUS", symbol });
        registry.emplace<TickerComp>(entity, symbol);
        registry.emplace<LinkGroupComp>(entity, 0);

        auto client = std::make_shared<FocusClient>(provider);
        client->SetSymbol(symbol);

        registry.emplace<FocusClientComp>(entity, client);
        registry.emplace<FocusRenderStateComp>(entity);

        registry.emplace<PanelRenderFn>(entity, [](entt::registry& reg, entt::entity ent) {
            UI::DrawFocusPanel(reg, ent);
            });

        return entity;
    }
}