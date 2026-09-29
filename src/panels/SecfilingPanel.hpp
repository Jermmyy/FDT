#pragma once
#include "../api/SecClient.hpp"
#include "../rendering/filing_renderer.h"
#include <future>

struct SecFilingPanelState {

    char search_buf[256] = {};

    std::vector<SEC::CompanyResult> companies;
    std::vector<SEC::FilingEntry> filings;
    int selected_company = -1;
    int selected_filing = -1;

    int form_filter_idx = 0;

    SEC::FetchState company_state = SEC::FetchState::Idle;
    SEC::FetchState filings_state = SEC::FetchState::Idle;
    SEC::FetchState doc_state = SEC::FetchState::Idle;

    std::future<std::vector<SEC::CompanyResult>> company_future;
    std::future<std::vector<SEC::FilingEntry>> filings_future;
    std::future<void> doc_future;

    std::string error_msg;

    Rendering::FilingRenderer renderer;

    SEC::SECApi api;
};

void InitSecFilingPanel(SecFilingPanelState& state, const std::string& user_agent);
void RenderSecFilingPanel(SecFilingPanelState& state);