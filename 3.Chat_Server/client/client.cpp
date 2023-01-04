#include "client.h"

void send_message(std::vector<std::string> user_names,std::string input_array[5],std::vector<uint16_t> users,int clientSd)
{
    char* destUserName = &input_array[1][0];
    uint16_t id = -1;
    int flag = 0;

    for (int i = 0; i<users.size(); i++)
    {
        if (user_names[i]==destUserName)
        {
            id = users[i];
            flag = 1;
            break;
        }
    }

    if (flag == 0)
    {
        std::cout<<"User not found"<<std::endl<<std::endl; 
        return;
    }

    char* sendMessage = strcpy(new char[input_array[2].length() + 1], input_array[2].c_str());
    if (destUserName != "")
    {
        Header send_header, sendack_header;
        send_header.message_type = SEND;
        send_header.message_id = 15;
        send_header.length = 4 + strlen(sendMessage);
        write(clientSd, (uint8_t*)&send_header, sizeof(Header));
        write(clientSd, (uint8_t*)&id, sizeof(id));
        write(clientSd, sendMessage, strlen(sendMessage));
        read(clientSd, (uint8_t*)&sendack_header, sizeof(Header));
        std::cout<<"Sent"<<std::endl<<std::endl; 
    }
}

void list_users(int clientSd,std::vector<uint16_t>& users,std::vector<std::string>& user_names,uint16_t& id)
{
    users.clear();
    user_names.clear();

    Header list_send_header, list_reply_header;
    list_send_header.message_type = LIST;
    list_send_header.message_id = 1;
    list_send_header.length = 2;
    write(clientSd, (uint8_t*)&list_send_header, sizeof(list_send_header));
    read(clientSd, (uint8_t*)&list_reply_header, sizeof(Header));

    int num = (list_reply_header.length-2)/2;
    
    for (int i = 0; i < num; i++) 
    {
        read(clientSd, (uint8_t*)&id, sizeof(id));
        users.push_back(id);
    }
    
    if (list_reply_header.message_type == LISTREPLY)
    {
        uint8_t payload[101];
        std::string user_name;

        for (int i = 0; i < num; i++)
        {
            Header info_send_header, info_ack_header;
            info_send_header.message_type = INFO;
            info_send_header.message_id = 1;
            info_send_header.length = 4;
            write(clientSd, (uint8_t*)&info_send_header, sizeof(Header));

            write(clientSd, (uint8_t*)&users[i], sizeof(users[i]));
            read(clientSd, (uint8_t*)&info_ack_header, sizeof(Header));
            auto user_len = info_ack_header.length - 2;
            read(clientSd, (uint8_t*)&payload, user_len);
            payload[user_len] = 0;
            user_name = std::string((char*)payload);
            user_names.push_back(user_name);
        }
    }
}

void recieve_message(int clientSd, std::vector<uint16_t>& users, std::vector<std::string>& user_names, uint16_t& id)
{
    char send_buffer[100];
    char reply_buffer[100];
    auto recieve_send_header = Header();
    recieve_send_header.message_type = RECEIVE;
    recieve_send_header.message_id = 11;
    recieve_send_header.length = 2;
    memset(&send_buffer, 0, sizeof(send_buffer));
    memcpy(send_buffer, &recieve_send_header, sizeof(recieve_send_header));
    send(clientSd, send_buffer, strlen(send_buffer), 0);

    memset(&reply_buffer, 0, sizeof(reply_buffer));
    int read_flag = recv(clientSd, (char*)&reply_buffer, sizeof(reply_buffer), 0);
    char msg[100]; 
    char senderID[2]; 

    if(read_flag>0){
        auto recieve_reply_header = Header();
        memcpy(&recieve_reply_header, reply_buffer, 2);

        if (recieve_reply_header.message_type == RECEIVEREPLY)
        {
            memset(&senderID, 0, sizeof(senderID));
            memcpy(senderID, reply_buffer+2, 2);

            if(senderID[1] != 0)
            {
                memcpy(msg, reply_buffer+4, recieve_reply_header.length-4);
                list_users(clientSd,users,user_names,id);

                for (int i = 0; i< users.size(); i++)
                {
                    if (users[i] == senderID[1]*256)
                    {
                        std::cout << "<<" << user_names[i] << ": ";
                        break;
                    }
                }

                if(msg != NULL)
                {
                    msg[recieve_reply_header.length-4] = 0;
                    std::cout << msg << std::endl;
                }
            }
        }
    }
}

void connect_socket(int clientSd, const char * userName)
{
    char send_buffer[1000];
    char reply_buffer[1000];
    auto connect_send_header = Header();
    connect_send_header.message_type = CONNECT;
    connect_send_header.message_id = 11;
    connect_send_header.length = 2 + strlen(userName);
    memset(&send_buffer, 0, sizeof(send_buffer));//clear the buffer
    memcpy(send_buffer, &connect_send_header, sizeof(connect_send_header));
    memcpy(send_buffer + 2, userName, strlen(userName));
    send(clientSd, send_buffer, strlen(send_buffer), 0);

    memset(&reply_buffer, 0, sizeof(reply_buffer));//clear the buffer
    auto ret = read(clientSd, reply_buffer, 2);
   

    if (ret < 0)
    {
        throw std::runtime_error("failed to read");
    }

    if (ret == 0)
    {
        throw std::runtime_error("socket closed");
    }

    auto connect_reply_header = Header();
    memcpy(&connect_reply_header, reply_buffer, 2);

    if (connect_reply_header.message_type == CONNACK)
    {
        std::cout << "Connected to the server!" << std::endl;
    }
    else
    {
        std::cout << "Error connecting to server" << std::endl;
    }   

}

int setup(const char *serverIp, int serverPort) 
{
    int exit = -1;
    struct hostent* host = gethostbyname(serverIp); 
    sockaddr_in sendSockAddr;   
    bzero((char*)&sendSockAddr, sizeof(sendSockAddr)); 
    sendSockAddr.sin_family = AF_INET; 
    sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(serverPort);

    int clientSd = socket(AF_INET, SOCK_STREAM, 0);

    if(clientSd <0) 
    {
        std::cout<<"Error creating socket!"<<std::endl; 
        return exit;
    }
    int status = connect(clientSd, (sockaddr*) &sendSockAddr, sizeof(sendSockAddr));
    if(status < 0) 
    {
        std::cout<<"Error connecting to socket!"<<std::endl; 
        close(clientSd);
        return exit;
    }
    return clientSd;
}
