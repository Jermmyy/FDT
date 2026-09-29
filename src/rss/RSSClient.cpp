#include "RSSClient.hpp"
//#include <cpr/cpr.h>
#include "../logging/NetWrapper.hpp"
#include <pugixml.hpp>
#include <sstream>

std::vector<RSSPost> RSSClient::Fetch(const std::string& url, const std::string& userAgent) {
	try {
		cpr::Response r = Net::Get( // Change this back to cpr::Get after logging networking
			cpr::Url{ url },
			cpr::Header{ { "User-Agent", userAgent.empty() ? "DataTerminal/1.0" : userAgent } },
			cpr::Timeout{ 10000 }
		);

		if (r.error || r.status_code >= 400)
			return {};

		// Extract author hint from URL; e.g. nitter.net/mark/rss -> "mark"
		std::string fallbackAuthor;
		size_t rssPos = url.rfind("/rss");
		if (rssPos != std::string::npos) {
			size_t slash = url.rfind('/', rssPos - 1);
			if (slash != std::string::npos)
				fallbackAuthor = url.substr(slash + 1, rssPos - slash - 1);
		}
		return Parse(r.text, fallbackAuthor);
	}
	catch (...) {
		return {};
	}
}

std::vector<std::string> RSSClient::ExtractImageUrls(const std::string& html) {
    std::vector<std::string> urls;
    if (html.empty()) return urls;

    std::string wrapped = "<root>" + html + "</root>";
    pugi::xml_document doc;

    pugi::xml_parse_result res = doc.load_string(wrapped.c_str());
    if (!res) return urls;

    auto tools = doc.select_nodes("//img");
    for (pugi::xpath_node xpath_img : tools) {
        pugi::xml_node img = xpath_img.node();
        if (img) {
            const char* src = img.attribute("src").value();
            if (src && strlen(src) > 0) {
                urls.push_back(src);
            }
        }
    }

    return urls;
}

std::vector<RSSPost> RSSClient::Parse(const std::string& xml, const std::string& fallbackAuthor) {
    std::vector<RSSPost> posts;

    pugi::xml_document doc;
    if (!doc.load_string(xml.c_str())) return posts;

    pugi::xml_node channel = doc.child("rss").child("channel");
    bool isAtom = false;

    if (!channel) {
        channel = doc.child("feed");
        isAtom = true;
    }

    if (!channel) return posts;

    const char* itemTag = isAtom ? "entry" : "item";

    for (pugi::xml_node item : channel.children(itemTag)) {
        RSSPost post;

        if (isAtom) {
            post.title = item.child_value("title");
            post.link = item.child("link").attribute("href").as_string();
            post.guid = item.child_value("id");
            post.pubDate = item.child_value("updated");
            post.content = item.child_value("content");
            if (post.content.empty())
                post.content = item.child_value("summary");
            post.author = item.child("author").child_value("name");
        }
        else {
            post.title = item.child_value("title");
            post.link = item.child_value("link");
            post.guid = item.child_value("guid");
            post.pubDate = item.child_value("pubDate");
            post.author = item.child_value("dc:creator");
            if (post.author.empty())
                post.author = item.child_value("author");

            // Raw description HTML — extract images before pugixml strips tags
            std::string rawDesc = item.child_value("description");
            post.imageUrls = ExtractImageUrls(rawDesc);

            // Content is already clean text from pugixml parsing
            post.content = item.child("description").text().get();
        }

        if (post.guid.empty())  post.guid = post.link;
        if (post.author.empty()) post.author = fallbackAuthor;

        if (post.guid.empty() && post.content.empty())
            continue;

        posts.push_back(std::move(post));
    }

    return posts;
}