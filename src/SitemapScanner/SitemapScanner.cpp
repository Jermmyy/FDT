#include "SitemapScanner.hpp"
#include <curl/curl.h>
#include <pugixml.hpp>
#include <sstream>

// libcurl dumps downloaded data here into a string buffer
size_t SitemapCrawler::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Splits "intel.com/en/roadmaps" into {"intel.com", "en", "roadmaps"}
std::vector<std::string> SitemapCrawler::tokenize(std::string url) {
    std::vector<std::string> tokens;

    // Strip protocol (https://)
    size_t proto = url.find("://");
    if (proto != std::string::npos) url.erase(0, proto + 3);

    // Split by '/'
    std::stringstream ss(url);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) tokens.push_back(segment);
    }
    return tokens;
}

void SitemapCrawler::Start(std::string rootUrl) {
    if (m_isScanning) return; // Prevent double-clicks
    
    m_isScanning = true;
    m_activeThreads = 0;
    
    // Reset tree
    m_rootNode = std::make_shared<SiteNode>();
    m_rootNode->name ="ROOT";

    // Launch inititial background threat
    std::thread initial(&SitemapCrawler::ParseTask, this, rootUrl);
    initial.detach();

    // Launch the first thread
    std::thread initialThread(&SitemapCrawler::ParseTask, this, rootUrl);
    initialThread.detach();
}

void SitemapCrawler::ParseTask(std::string url) {
    m_activeThreads++;
    
    CURL* curl = curl_easy_init();
    std::string buffer;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    pugi::xml_document doc;
    if (doc.load_string(buffer.c_str())) {
        // Find all <loc> tags in sitemap.xml
        pugi::xpath_node_set nodes = doc.select_nodes("//loc");

        for (pugi::xpath_node node : nodes) {
            std::string loc = node.node().child_value();

            if (loc.find(".xml") != std::string::npos) {
                // Its a sub-sitemap -> +1 thread
                std::thread subThread(&SitemapCrawler::ParseTask, this, loc);
                subThread.detach(); 
            } else {
                // Its a webpage -> Tokenize and insert tree
                std::vector<std::string> parts = tokenize(loc);

                std::lock_guard<std::mutex> lock(m_treeMutex);
                m_rootNode->insert(parts, 0);
            }
        }
    }

    m_activeThreads--;
    if (m_activeThreads == 0) m_isScanning = false;
}

SitemapCrawler::~SitemapCrawler() {
    // Basic cleanup
}