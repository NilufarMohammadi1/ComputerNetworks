#include "client.h"

//Client side
int main(int argc, char *argv[])
{
    if(argc != 3)
    {
       std::cerr << "Usage: ip_address port" << std::endl; 
       exit(0);
    }

    const char *serverIp = strtok(argv[1], ":");
    int serverPort = atoi((const char*)strtok(0, ""));
    const char* userName = &argv[2][0];
   
    std::cout << "serverIp: " << serverIp << std::endl;
    std::cout << "serverPort: " << serverPort << std::endl;
    std::cout << "userName: " << userName << std::endl;

    auto clientSd = setup(serverIp, serverPort);
    connect_socket(clientSd, userName);
    
    struct timeval start1, end1;
    gettimeofday(&start1, NULL);
    std::vector<uint16_t> users;
    std::vector<std::string> user_names;
    uint16_t id;

    while(true) 
    {
        fd_set read_fds;
        FD_ZERO (&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        auto tv = timeval{5, 0};
        auto ret = select(2, &read_fds, NULL, NULL, &tv);

        if (ret < 0)
        {
            throw std::runtime_error("selct error");
        }

        if (ret == 0) 
        {
            auto tv_read = timeval{1, 0};
            setsockopt(clientSd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_read, sizeof tv_read);
            recieve_message(clientSd, users, user_names, id);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) 
        {
            std::cout << ">> ";
            std::string line;

            if (!getline(std::cin, line) || line == "exit" || line == "Exit") 
            {
                std::clog<<"Exit!"<<std::endl; 
                break;
            }
            std::string input_temp, input_array[5];  
            std::stringstream input_data_stream(line);
            int i = 0;

            while (getline(input_data_stream, input_temp, ' ')) 
            {
                input_array[i] = input_temp;
                i++;

                if(i>3) 
                {
                    break;
                }
            }
            
            if(input_array[0] == "list"|| input_array[0] == "List") 
            {
                std::cout << "Users:" << std::endl;
                list_users(clientSd,users,user_names,id);
                
                for (int i = 0; i<user_names.size(); i++) 
                {
                    std::cout << "\t-" << user_names[i] << std::endl;
                }
            }
            else if (input_array[0] == "send"|| input_array[0] == "Send") 
            {
                send_message(user_names,input_array,users,clientSd);
            }
            else 
            {
                std::cout<<"Invalid Operation!"<<std::endl; 
            }
        }
    }

    gettimeofday(&end1, NULL);
    std::cout << "********Session********" << std::endl;
    std::cout << "Elapsed time: " << (end1.tv_sec- start1.tv_sec) << " secs" << std::endl;
    std::cout << "Connection closed" << std::endl;
    return EXIT_SUCCESS;    
}