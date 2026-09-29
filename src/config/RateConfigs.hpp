#pragma once
#include "../api/RateClient.hpp"
#include <string>

RateClient makeEFFR(const std::string& start, const std::string& end);
RateClient makeSOFR(const std::string& start, const std::string& end);
RateClient makeRP(int numOps);
RateClient makeRRP(int numOps);