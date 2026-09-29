#pragma once
#include "../api/FedClient.hpp"
#include <string>

inline FedClient makeFedH41(const std::string& apiKey,
                             int historyWeeks = 104)
{
    FedClientConfig cfg;
    cfg.apiKey           = apiKey;
    cfg.observationLimit = historyWeeks;

    // scale: FRED H.4.1 units are Millions USD -> * 0.001 = Billions USD
    constexpr double M2B = 0.001;

    cfg.series = {

        // ── ASSETS ──────────────────────────────────────────────

        {
            "WALCL",
            "Total Assets",
            "Assets",
            "Bil. USD",
            M2B
        },
        {
            "WSHOGT",
            "Securities Held Outright (Total)",
            "Assets",
            "Bil. USD",
            M2B
        },
        {
            "TREAST",
            "U.S. Treasury Securities",
            "Assets",
            "Bil. USD",
            M2B
        },
        {
            "WMBSEC",
            "Mortgage-Backed Securities (MBS)",
            "Assets",
            "Bil. USD",
            M2B
        },
        {
            "WFEDSEC",
            "Federal Agency Debt Securities",
            "Assets",
            "Bil. USD",
            M2B
        },
        {
            "WLCFLL",
            "Loans (Liquidity & Credit Facilities)",
            "Assets",
            "Bil. USD",
            M2B
        },
        {
            "WORAL",
            "Repurchase Agreements (Repo)",
            "Assets",
            "Bil. USD",
            M2B
        },
        {
            "WACBS",
            "Central Bank Liquidity Swaps",
            "Assets",
            "Bil. USD",
            M2B
        },

        // ── LIABILITIES ─────────────────────────────────────────

        {
            "WTREGEN",
            "Treasury General Account (TGA)",
            "Liabilities",
            "Bil. USD",
            M2B
        },
        {
            "WLRRAFOIAL",
            "Reverse Repo (RRP) — Foreign Official",
            "Liabilities",
            "Bil. USD",
            M2B
        },
        {
            "RRPONTSYD",
            "Reverse Repo (RRP) — Overnight",
            "Liabilities",
            "Bil. USD",
            M2B
        },
        {
            "WRBWFRBL",
            "Federal Reserve Notes Outstanding",
            "Liabilities",
            "Bil. USD",
            M2B
        },

        // ── RESERVES & LIQUIDITY ────────────────────────────────

        {
            "WRESBAL",
            "Reserve Balances (Bank Reserves)",
            "Reserves",
            "Bil. USD",
            M2B
        },
        {
            "TOTRESNS",
            "Total Reserves",
            "Reserves",
            "Bil. USD",
            M2B
        },
    };

    return FedClient(std::move(cfg));
}
