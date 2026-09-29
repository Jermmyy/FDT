#pragma once
#include <string>
#include <vector>

struct FedSeriesRow {
	std::string seriesId;
	std::string title;
	std::string frequency;
	std::string units;
};

struct FedReleasePanelComp {
	int releaseId = -1;
	std::string releaseCode;
	bool isLoading = true;
	std::string errorMessage;
	std::vector<FedSeriesRow> rows;
	char filterBuf[128] = "";
};