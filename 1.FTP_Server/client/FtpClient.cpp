#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fstream>

#define PORT 8080

int main()
{	
	struct sockaddr_in address;
	int socket_id;
	int valueread;
	struct sockaddr_in serv_addr;
	int addrlen = sizeof(address);
	char message[] = "";
	char buffer_ftp[10000] = {0};

	int socket_status;
	char choice[1];
    socket_id = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_id == -1)
	{
		std::cout << "Error while setting socket... \n";
	} 
	memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	int connect_status = connect(socket_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if(connect_status == -1)
	{
		std::cout << "Error while connecting from client side... \n"; 
	}
	std::cout << "Do you want to download a file(1) or upload a file(2) ?: ";
	std::cin >> choice;
	std::cout<< "You choose: " << choice[0] << std::endl;
	write(socket_id, choice, 1);
	if(choice[0]=='1')
	{
		std::cout<<"File Download requested"<< std::endl;
		std::ofstream test_file;
		test_file.open("client_download.txt", std::ofstream::out | std::ofstream::trunc);
		test_file.close();
		test_file.open("client_download.txt", std::ios_base::app);
		char filebuf[1024];
		while(true)
		{
			memset(&filebuf, '\0', sizeof(filebuf));
			valueread = read(socket_id, buffer_ftp, 1024);
			if(valueread<= 0)
			{
				std::cout<< "Ending" << std::endl;
				break;	
			}
			test_file << buffer_ftp;
		}
		test_file.close();

		std::cout << "File successfully downloaded. " << std::endl;	
	}
	else if(choice[0]=='2')
	{
		std::cout<< "File Upload requested" << std::endl;
		std::ifstream test_file ("client_upload.txt");
		char filebuf[1024];
		while(true)
		{
			memset(&filebuf, '\0', sizeof(filebuf));
			test_file.read(filebuf, 1024);
			write(socket_id, filebuf, 1024);
			if(test_file.eof())
			{
				break;
			}			
		}
		test_file.close();

		std::cout << "File successfully uploaded." << std::endl;
	}
	close(socket_id); 
	return 0;
}