#pragma once
#include <string>
#include <vector>

struct RSSPost {
	std::string author;
	std::string title;
	std::string content;
	std::string link;
	std::string pubDate;
	std::string guid;
	std::vector<std::string> imageUrls;
};

class RSSClient {
public:
	// Fetches and parses an RSS feed from given url
	// Returns empty vector on failure, never throws
	std::vector<RSSPost> Fetch(const std::string& url, const std::string& userAgent = "");

private:
	std::vector<RSSPost> Parse(const std::string& xml, const std::string& fallbackAuthor);
	std::vector<std::string> ExtractImageUrls(const std::string& html);
};