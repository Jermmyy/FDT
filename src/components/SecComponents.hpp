#pragma once
#include <memory>

struct SecFilingPanelState;

struct SecClientComp {
    std::shared_ptr<SecFilingPanelState> state;
};