#pragma once
#include "../Models/Sec.hpp"
#include <string>
#include <vector>
#include <functional>

namespace SEC {

    class SECApi {
    public:
        SECApi();
        ~SECApi() = default;

        void SetUserAgent(const std::string& user_agent);

        std::vector<CompanyResult> SearchCompany(const std::string& query);
        std::vector<FilingEntry> GetFilings(const std::string& cik, const std::string& form_type = "", int max_results = 50);
        std::string FetchDocument(const std::string& url);
        std::string ResolveDocumentUrl(const std::string& cik, const FilingEntry& entry) const;

    private:
        std::string m_user_agent;
        std::string m_tickers_cache;

        std::string HttpGet(const std::string& url);
        std::vector<CompanyResult> ParseSearchResults(const std::string& body, const std::string& query);
        std::vector<FilingEntry> ParseSubmissionsJson(const std::string& body, const std::string& cik, const std::string& form_filter, int max_results);
    };
}
