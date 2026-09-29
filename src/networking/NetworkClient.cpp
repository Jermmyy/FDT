#include "NetworkClient.hpp"
#include "Message_generated.h"
#include <iostream>
#include <cstring>
#include <ctime>
#include <winsock2.h>

NetworkClient& NetworkClient::Get() {
    static NetworkClient instance;
    return instance;
}

NetworkClient::NetworkClient() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium!" << std::endl;
    }
}

NetworkClient::~NetworkClient() {
    Disconnect();
}

void NetworkClient::Connect(const std::string& host, uint16_t port) {
    if (connected_) return;

    worker_thread_ = std::thread([this, host, port]() {
        try {
            boost::asio::ip::tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host, std::to_string(port));

            socket_ = std::make_unique<boost::asio::ip::tcp::socket>(io_context_);
            boost::asio::connect(*socket_, endpoints);

            crypto_kx_keypair(client_pk_.data(), client_sk_.data());
            boost::asio::write(*socket_, boost::asio::buffer(client_pk_));

            std::array<unsigned char, crypto_kx_PUBLICKEYBYTES> server_pk;
            boost::asio::read(*socket_, boost::asio::buffer(server_pk));

            if (crypto_kx_client_session_keys(
                rx_key_.data(),
                tx_key_.data(),
                client_pk_.data(),
                client_sk_.data(),
                server_pk.data()) != 0) {
                return;
            }

            connected_ = true;
            AsyncReadHeader();
            io_context_.run();
        }
        catch (const std::exception& /*e*/) {
            connected_ = false;
        }
        });
}

void NetworkClient::SendMessagePayload(const std::string& sender, const std::string& content, const std::string& channel) {
    if (!connected_) return;

    boost::asio::post(io_context_, [this, sender, content, channel]() {
        flatbuffers::FlatBufferBuilder builder(1024);
        auto s_off = builder.CreateString(sender);
        auto c_off = builder.CreateString(content);

        // Use the generated creation helper function to avoid builder syntax issues
        auto chat_msg = SecureChat::CreateChatMessage(
            builder,
            s_off,
            c_off,
            static_cast<uint64_t>(std::time(nullptr))
        );
        builder.Finish(chat_msg);

        const char* raw_buffer = reinterpret_cast<const char*>(builder.GetBufferPointer());
        size_t raw_size = builder.GetSize();

        std::vector<unsigned char> nonce(crypto_secretbox_NONCEBYTES);
        randombytes_buf(nonce.data(), nonce.size());

        std::vector<unsigned char> ciphertext(raw_size + crypto_secretbox_MACBYTES);
        crypto_secretbox_easy(
            ciphertext.data(),
            reinterpret_cast<const unsigned char*>(raw_buffer),
            raw_size,
            nonce.data(),
            tx_key_.data()
        );

        uint32_t total_body_size = static_cast<uint32_t>(nonce.size() + ciphertext.size());
        uint32_t len_header = htonl(total_body_size);

        std::vector<char> packet(sizeof(uint32_t) + total_body_size);
        std::memcpy(packet.data(), &len_header, sizeof(uint32_t));
        std::memcpy(packet.data() + sizeof(uint32_t), nonce.data(), nonce.size());
        std::memcpy(packet.data() + sizeof(uint32_t) + nonce.size(), ciphertext.data(), ciphertext.size());

        boost::system::error_code ignored_ec;
        boost::asio::write(*socket_, boost::asio::buffer(packet), ignored_ec);
        });
}

void NetworkClient::AsyncReadHeader() {
    boost::asio::async_read(
        *socket_,
        boost::asio::buffer(&incoming_body_size_, sizeof(incoming_body_size_)),
        [this](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
            if (!ec) {
                incoming_body_size_ = ntohl(incoming_body_size_);
                AsyncReadBody();
            }
            else {
                connected_ = false;
            }
        });
}

void NetworkClient::AsyncReadBody() {
    body_buffer_.resize(incoming_body_size_);
    boost::asio::async_read(
        *socket_,
        boost::asio::buffer(body_buffer_.data(), body_buffer_.size()),
        [this](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
            if (!ec) {
                const unsigned char* nonce = reinterpret_cast<const unsigned char*>(body_buffer_.data());
                const unsigned char* ciphertext = nonce + crypto_secretbox_NONCEBYTES;
                size_t ciphertext_len = incoming_body_size_ - crypto_secretbox_NONCEBYTES;

                std::vector<char> decrypted(ciphertext_len - crypto_secretbox_MACBYTES);
                if (crypto_secretbox_open_easy(
                    reinterpret_cast<unsigned char*>(decrypted.data()),
                    ciphertext,
                    ciphertext_len,
                    nonce,
                    rx_key_.data()) != 0) {
                    return;
                }

                auto chat_msg = SecureChat::GetChatMessage(decrypted.data());

                NetworkMessage msg;
                msg.sender = chat_msg->sender()->str();
                msg.text = chat_msg->content()->str();
                msg.timestamp = "12:00:00";
                msg.channel = "main";

                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    incoming_queue_.push_back(msg);
                }

                AsyncReadHeader();
            }
            else {
                connected_ = false;
            }
        });
}

bool NetworkClient::PollIncoming(std::vector<NetworkMessage>& out_messages) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (incoming_queue_.empty()) return false;
    out_messages = std::move(incoming_queue_);
    incoming_queue_.clear();
    return true;
}

bool NetworkClient::IsConnected() const {
    return connected_;
}

void NetworkClient::Disconnect() {
    connected_ = false;
    io_context_.stop();
    if (worker_thread_.joinable()) worker_thread_.join();
}