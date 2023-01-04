#include <stdio.h>
#include <sys/types.h>  // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h> // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h> // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>

#define BUFF_SIZE 256

void error(char *msg)
{
  perror(msg);
  exit(1);
}

char *getContentType(char *path_token)
{
  char *exts[] = {".html", ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".mp3", ".pdf", ".ico"};
  char *content_types[] = {"text/html",
                           "image/jpeg",
                           "image/jpeg",
                           "image/png",
                           "image/bmp",
                           "image/gif",
                           "audio/mp3",
                           "application/pdf",
                           "image/x-icon"};
  char *result = content_types[0];
  int len_exts = (int)(sizeof(exts) / sizeof(exts[0]));

  for (int i = 0; i < len_exts; i++)
  {
    // Match path and content-type
    if (strstr(path_token, exts[i]) != NULL)
    {
      result = (char *)malloc(strlen(content_types[i]) + 1);
      // Find correct content type
      strcpy(result, content_types[i]);
      break;
    }
  }

  printf("content-type: %s\n", result);
  return result;
}

int getFileSize(int file)
{
  int size = lseek(file, 0, SEEK_END); // Size of file
  lseek(file, 0, SEEK_SET);            // Initialize pointer of file

  return size;
}

int main(int argc, char **argv)
{
  int socket_fd, client_fd;
  int port = 8080;
  char buffer[BUFF_SIZE];
  char response[BUFF_SIZE];

  struct sockaddr_in server_addr, client_addr;

  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    error("Failed to open socket");
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    error("Failed to binding");
  }
    
  listen(socket_fd, 5); // 5 is count of Backlog queue

  int clilen = sizeof(client_addr);

  while (1)
  {
    // Make child Process
    int pid = fork();
    int status;

    if (pid < 0)
    {
      error("Failed to fork");
    }

    if (pid > 0)
    {
      wait(&status);
    }

    client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &clilen);

    if (client_fd < 0)
    {
      error("Failed to accept");
    }

    memset(buffer, 0, BUFF_SIZE);
    memset(response, 0, BUFF_SIZE);

    if (read(client_fd, buffer, BUFF_SIZE) < 0)
    {
      error("ERROR on read");
    }

    printf("Request message\n");
    printf("%s", buffer);

    char *token = strtok(buffer, " ");
    char *raw_path = strtok(NULL, " "); // get raw file path like /sample.jpeg
    char *path;

    printf("\n\n** raw_path: %s\n", raw_path);

    // Make raw_path to real file path like ./sample.jpeg
    if (strcmp(raw_path, "/") == 0)
    {
      path = (char *)malloc(strlen("./index.html") + 1);
      strcpy(path, "./index.html");
    }
    else
    {
      path = (char *)malloc(strlen(raw_path) + 1);
      sprintf(path, ".%s", raw_path);
    }

    char *content_type = getContentType(path);

    printf("\n\n** path: %s\n", path);
    printf("** content-type%s\n", getContentType(path));

    int file = open(path, O_RDONLY);

    if (file < 0) // when fails to open file
    {
      file = open("./404.html", O_RDONLY);
      content_type = (char *)malloc(strlen("text/html") + 1);
      strcpy(content_type, "text/html");
    }

    // Get FileSize
    int file_size = getFileSize(file);
    // Make HTTP Response String
    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: Keep - Alive\r\n\r\n", content_type, file_size);
    printf("\nResponse message\n");
    printf("%s\n", response);
    write(client_fd, response, strlen(response));
    memset(buffer, 0, BUFF_SIZE);

    // Write File during sending whole file bytes
    while ((file_size = read(file, buffer, BUFF_SIZE)) > 0)
    {
      write(client_fd, buffer, BUFF_SIZE);
    }
    close(file);
    printf("########################\n\n");
  }
  close(client_fd);
  close(socket_fd);

  return 0;
}