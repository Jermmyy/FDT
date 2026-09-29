#pragma once
#include <string>
#include <vector>

namespace SEC
{

    struct CompanyResult
    {
        std::string name;
        std::string cik;
        std::string ticker;
        std::string exchange;

        std::string cik_padded() const
        {
            std::string padded = cik;
            while (padded.size() < 10)
                padded = "0" + padded;
            return padded;
        }
    };

    struct FilingEntry
    {
        std::string accession_number;
        std::string form_type;
        std::string filing_date;
        std::string report_date;
        std::string description;
        std::string primary_doc;
        std::string primary_doc_url;

        std::string accession_no_dashes() const
        {
            std::string s = accession_number;
            s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
            return s;
        }
    };

    enum class FetchState {
        Idle,
        Loading,
        Done,
        Error
    };

    struct FilingDocument {
        std::string raw_html;
        std::string accession_number;
        bool loaded = false;
    };
}