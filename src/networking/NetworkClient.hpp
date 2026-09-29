#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <array>
#include <boost/asio.hpp>
#include <sodium.h>

struct NetworkMessage {
    std::string sender;
    std::string text;
    std::string timestamp;
    std::string channel;
};

class NetworkClient {
public:
    static NetworkClient& Get();

    void Connect(const std::string& host, uint16_t port);
    void SendMessagePayload(const std::string& sender, const std::string& content, const std::string& channel = "main");
    bool PollIncoming(std::vector<NetworkMessage>& out_messages);
    bool IsConnected() const;
    void Disconnect();

private:
    NetworkClient();
    ~NetworkClient();

    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    void AsyncReadHeader();
    void AsyncReadBody();

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::thread worker_thread_;

    bool connected_ = false;

    std::array<unsigned char, crypto_kx_PUBLICKEYBYTES> client_pk_;
    std::array<unsigned char, crypto_kx_SECRETKEYBYTES> client_sk_;
    std::array<unsigned char, crypto_kx_SESSIONKEYBYTES> rx_key_;
    std::array<unsigned char, crypto_kx_SESSIONKEYBYTES> tx_key_;

    uint32_t incoming_body_size_ = 0;
    std::vector<char> body_buffer_;

    std::mutex queue_mutex_;
    std::vector<NetworkMessage> incoming_queue_;
};