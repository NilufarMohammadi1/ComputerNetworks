#include "server.h"
#include "message.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <thread>
#include <tuple>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define MAX_PAYLOAD_LENGTH 10000

UserID ChatRoom::connect(std::string name)
{
    std::lock_guard<std::mutex> lock(mutex);

    users[next_user_id] = User{name: name};
    return next_user_id++;
}

void ChatRoom::disconnect(UserID user_id) 
{
    std::lock_guard<std::mutex> lock(mutex);

    users.erase(user_id);
}

std::vector<UserID> ChatRoom::list() 
{
    std::lock_guard<std::mutex> lock(mutex);

    std::vector<UserID> ret;
    transform(users.begin(), users.end(), std::back_inserter(ret),
              [](decltype(users)::const_reference u) { return u.first; });
    return ret;
}

User ChatRoom::info(UserID user_id) 
{
    std::lock_guard<std::mutex> lock(mutex);

    return users.at(user_id);
}

void ChatRoom::send(UserID from_id, UserID to_id, std::string message) 
{
    std::lock_guard<std::mutex> lock(mutex);

    auto& user = users[to_id];
    user.messages.push(std::make_pair(from_id, message));
}

std::pair<UserID, std::string> ChatRoom::receive(UserID user_id) 
{
    std::lock_guard<std::mutex> lock(mutex);

    auto& user = users[user_id];
    if (user.messages.empty())
        return std::make_pair(0, "");
    auto message = user.messages.front();
    user.messages.pop();
    return message;
}

Server::Server(std::uint16_t port) 
{
    auto srv_addr = sockaddr_in{sin_family: AF_INET,
                                sin_port: htons(port),
                                sin_addr: {s_addr: htonl(INADDR_ANY)}};

    fd = socket(AF_INET, SOCK_STREAM, 0);

    try 
    {
        auto err = bind(fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
        if (err < 0)
            throw std::runtime_error("failed to bind");

        err = listen(fd, 10);
        if (err < 0)
        {
            throw std::runtime_error("failed to listen");
        }
    } 
    catch (std::exception) 
    {
        close(fd);;
        throw;
    }
}


void Server::serve() 
{
    while (true) 
    {
        sockaddr_in client_addr;
        auto client_addr_len = sizeof(client_addr);
        auto client_fd = accept(this->fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
        char client_ip[INET_ADDRSTRLEN+1];
        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, INET_ADDRSTRLEN);
        std::clog << "new client: " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

        std::thread(&Server::client, this, client_fd).detach();
    }
}


void Server::client(int client_fd) 
{
    auto client = Client(client_fd, room);
    try 
    {
        client.run();
    } 
    catch (std::runtime_error e) 
    {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}


Client::Client(int fd, ChatRoom& room): fd(fd), room(room){}


Client::~Client() 
{
    close(fd);
    room.disconnect(user_id);

    std::clog << user_id << " disconnected" << std::endl;
}


void Client::run() 
{
    while (true) 
    {
        std::clog << "waiting for message from " << (connected ? std::to_string(user_id) : "the new client") << std::endl;

        Header header;
        read((uint8_t*)&header, sizeof(header));
        auto payload_length = header.length - sizeof(Header);

        if (payload_length < 0)
        {
            throw std::runtime_error("negative payload length");
        }

        uint8_t payload[MAX_PAYLOAD_LENGTH + 1];
        read(payload, payload_length);

        switch (header.message_type) 
        {
        case CONNECT:
            if (payload_length < 1)
            {
                throw std::runtime_error("wrong message length");
            }
            {
                payload[payload_length] = 0;
                std::string user_name = std::string((char*)payload);
                connect(user_name);

                auto reply_header = Header{};
                reply_header.message_type= CONNACK;
                reply_header.message_id = header.message_id;
                reply_header.length = sizeof(Header);
                write((uint8_t*)&reply_header, sizeof(reply_header));
            }
            break;

        case LIST:
            {
                auto lst = list();
                auto reply_header = Header{};
                reply_header.message_type= LISTREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = sizeof(Header) + 2*lst.size();
                write((uint8_t*)&reply_header, sizeof(reply_header));

                for (auto user_id_: lst) 
                {
                    uint16_t buf = htons(user_id_);
                    write((uint8_t*)&buf, sizeof(buf));
                }
            }
            break;

        case INFO:
            if (payload_length != 2)
            {
                throw std::runtime_error("wrong message length");
            }
            {
                uint16_t id = ntohs(*(uint16_t*)payload);
                std::string name = "";
                try 
                {
                    name = info(id).name;
                } 
                catch (std::exception) {}
                auto reply_header = Header{};
                reply_header.message_type= INFOREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = sizeof(Header) + name.length();
                write((uint8_t*)&reply_header, sizeof(reply_header));
                write((uint8_t*)name.c_str(), name.length());
            }
            break;

        case SEND:
            if (payload_length <= 2)
            {
                throw std::runtime_error("wrong message length");
            }
            {
                auto id = ntohs(*(uint16_t*)payload);
                auto message_length = payload_length - 2;
                char* message = (char*)payload + 2;
                message[message_length] = 0;
                uint8_t* state_buff = (uint8_t*)"\xff";
                try 
                {
                    send(id, message);
                } 
                catch (std::exception) 
                {
                    state_buff = (uint8_t*)"\x00";
                }
                auto reply_header = Header{};
                reply_header.message_type= SENDREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = sizeof(Header) + 1;
                write((uint8_t*)&reply_header, sizeof(reply_header));
                write(state_buff, 1);
            }
            break;
        
        case RECEIVE:
            if (payload_length != 0)
            {
                throw std::runtime_error("wrong message length");
            }
            {
                auto pr = receive();
                auto usr_id = htons(pr.first);
                auto message = pr.second;
                auto reply_header = Header{};
                reply_header.message_type= RECEIVEREPLY;
                reply_header.message_id = header.message_id;
                reply_header.length = sizeof(Header) + sizeof(usr_id) + message.length();
                write((uint8_t*)&reply_header, sizeof(reply_header));
                write((uint8_t*)&usr_id, sizeof(usr_id));
                write((uint8_t*)message.c_str(), message.length());
            }
            break;
        }
    }
}


void Client::read(uint8_t* buffer, size_t len) 
{
    auto n = 0;
    while (n < len) 
    {
        auto ret = ::read(fd, buffer + n, len - n);
        if (ret < 0)
        {
            throw std::runtime_error("failed to read");
        }
        if (ret == 0)
        {
            throw std::runtime_error("socket closed");
        }

        n += ret;
    }
}


void Client::write(const uint8_t* buffer, size_t len) 
{
    auto n = 0;
    while (n < len) 
    {
        auto ret = ::write(fd, buffer + n, len - n);
        if (ret < 0)
        {
            throw std::runtime_error("failed to write");
        }
        if (ret == 0)
        {
            throw std::runtime_error("socket closed");
        }

        n += ret;
    }
}


void Client::connect(std::string user_name) 
{
    if (connected)
    {
        throw std::runtime_error("connected before");
    }

    user_id = room.connect(user_name);
    connected = true;

    std::clog << user_id << " [" << user_name << "]" << " connected" << std::endl;
}


std::vector<UserID> Client::list() 
{
    if (!connected)
    {
        throw std::runtime_error("not connected");
    }

    std::clog << "list from " << user_id << std::endl;
    return room.list();
}


User Client::info(UserID user_id_) 
{
    if (!connected)
    {
        throw std::runtime_error("not connected");
    }

    std::clog << "info from " << user_id << " for " << user_id_ << std::endl;
    return room.info(user_id_);
}


void Client::send(UserID id, std::string message) 
{
    if (!connected)
    {
        throw std::runtime_error("not connected");
    }

    std::clog << "send from " << user_id << " to " << id << " \"" << message << "\"" << std::endl;
    room.send(user_id, id, message);
}


std::pair<UserID, std::string> Client::receive() 
{
    if (!connected)
    {
        throw std::runtime_error("not connected");
    }

    std::clog << "receive from " << user_id << std::endl;
    return room.receive(user_id);
}
