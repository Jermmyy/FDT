#include "SecClient.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

namespace SEC {
    SECApi::SECApi() = default;

    void SECApi::SetUserAgent(const std::string& user_agent) {
        m_user_agent = user_agent;
    }

    std::string SECApi::HttpGet(const std::string& url) {
        std::string ua = m_user_agent.empty()
            ? "DataTerminal jeremy.feytens@gmail.com"
            : m_user_agent;

            cpr::Response r = cpr::Get(
                cpr::Url{url},
                cpr::Header{{"User-Agent", ua}},
                cpr::Timeout{15000}
            );

            if (r.error)
                throw std::runtime_error("HTTP error: " + r.error.message);
            if (r.status_code == 429)
                throw std::runtime_error("EDGAR rate limit hit (429) — slow down requests");
            if (r.status_code >= 400)
                throw std::runtime_error("HTTP " + std::to_string(r.status_code) + " for " + url);

            return r.text;
    }


    // Search companies
    std::vector<CompanyResult> SECApi::SearchCompany(const std::string& query) {
        // fetch once, cache forever
        if (m_tickers_cache.empty())
            m_tickers_cache = HttpGet("https://www.sec.gov/files/company_tickers.json");

        return ParseSearchResults(m_tickers_cache, query);
    }

    std::vector<CompanyResult> SECApi::ParseSearchResults(const std::string& body, const std::string& query) {
        std::vector<CompanyResult> results;
        try {
            auto j = json::parse(body);

            // lowercase query for case-insensitive match
            std::string q = query;
            std::transform(q.begin(), q.end(), q.begin(), ::tolower);

            for (auto& [key, val] : j.items()) {
                std::string name   = val["title"].get<std::string>();
                std::string ticker = val["ticker"].get<std::string>();
                std::string cik    = std::to_string(val["cik_str"].get<int>());

                std::string name_lower   = name;
                std::string ticker_lower = ticker;
                std::transform(name_lower.begin(),   name_lower.end(),   name_lower.begin(),   ::tolower);
                std::transform(ticker_lower.begin(), ticker_lower.end(), ticker_lower.begin(), ::tolower);

                if (name_lower.find(q) != std::string::npos || ticker_lower.find(q) != std::string::npos) {
                    CompanyResult r;
                    r.name   = name;
                    r.ticker = ticker;
                    r.cik    = cik;
                    results.push_back(r);
                    if (results.size() >= 20) break;
                }
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to parse company tickers: ") + e.what());
        }
        return results;
    }

    // Get filings for a CIK
    std::vector<FilingEntry> SECApi::GetFilings(const std::string& cik,
                                                const std::string& form_type,
                                                int max_results) {
        std::string padded = cik;
        while (padded.size() < 10) padded = "0" + padded;

        std::string url = "https://data.sec.gov/submissions/CIK" + padded + ".json";
        std::string body = HttpGet(url);
        return ParseSubmissionsJson(body, cik, form_type, max_results);
    }

    std::vector<FilingEntry> SECApi::ParseSubmissionsJson(const std::string& body,
                                                        const std::string& cik,
                                                        const std::string& form_filter,
                                                        int max_results) {
        std::vector<FilingEntry> entries;
        try {
            auto j = json::parse(body);

            if (!j.contains("filings") || !j["filings"].contains("recent"))
                return entries;

            auto& recent       = j["filings"]["recent"];
            auto& forms        = recent["form"];
            auto& dates        = recent["filingDate"];
            auto& accessions   = recent["accessionNumber"];
            auto& primary_docs = recent["primaryDocument"];
            auto& descriptions = recent["primaryDocDescription"];
            auto& report_dates = recent["reportDate"];

            int count = 0;
            for (size_t i = 0; i < forms.size() && count < max_results; ++i) {
                std::string form = forms[i].get<std::string>();

                if (!form_filter.empty() && form != form_filter)
                    continue;

                FilingEntry entry;
                entry.form_type        = form;
                entry.filing_date      = dates[i].get<std::string>();
                entry.accession_number = accessions[i].get<std::string>();
                entry.primary_doc      = primary_docs[i].get<std::string>();
                entry.report_date      = report_dates[i].is_null() ? "" : report_dates[i].get<std::string>();
                entry.description      = descriptions[i].is_null() ? "" : descriptions[i].get<std::string>();
                entry.primary_doc_url  = ResolveDocumentUrl(cik, entry);

                entries.push_back(entry);
                count++;
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to parse submissions JSON: ") + e.what());
        }
        return entries;
    }

    // Fetch document
    std::string SECApi::FetchDocument(const std::string& url) {
        return HttpGet(url);
    }

    // Util: resolve primary document URL
    std::string SECApi::ResolveDocumentUrl(const std::string& cik,
                                            const FilingEntry& entry) const {
        std::string padded = cik;
        while (padded.size() < 10) padded = "0" + padded;

        std::string acc_no_dashes = entry.accession_number;
        acc_no_dashes.erase(
            std::remove(acc_no_dashes.begin(), acc_no_dashes.end(), '-'),
            acc_no_dashes.end()
        );

        return "https://www.sec.gov/Archives/edgar/data/"
            + padded + "/"
            + acc_no_dashes + "/"
            + entry.primary_doc;
    }
}