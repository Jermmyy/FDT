#ifndef SITEMAP_CRAWLER_HPP
#define SITEMAP_CRAWLER_HPP

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <map>
#include <memory>

struct SiteNode {
    std::string name;
    // std::map keeps children sorted alphabetically
    std::map<std::string, std::shared_ptr<SiteNode>> children;
    bool isPage = false;

    void insert(const std::vector<std::string>& path, size_t index) {
        if (index == path.size()) {
            isPage = true;
            return;
        }

        const std::string& part = path[index];
        if (children.find(part) == children.end()) {
            auto newNode = std::make_shared<SiteNode>();
            newNode->name = part;
            children[part] = newNode;
        }
        children[part]->insert(path, index + 1);
    }
};

class SitemapCrawler {
public:
    SitemapCrawler() : m_isScanning(false), m_activeThreads(0) {}
    ~SitemapCrawler() = default;

    void Start(std::string rootUrl);
    
    // For your ImGui UI to access data
    std::shared_ptr<SiteNode> GetRoot() { return m_rootNode; }
    bool IsBusy() const { return m_isScanning; }
    int GetActiveThreads() const { return m_activeThreads; }

private:
    void ParseTask(std::string url);
    std::vector<std::string> tokenize(std::string url);
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    std::shared_ptr<SiteNode> m_rootNode;
    std::mutex m_treeMutex; // Critical for thread safety when writing to the tree
    
    std::atomic<bool> m_isScanning;
    std::atomic<int> m_activeThreads;
};

#endif