#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/select.h>

void parse_connect(int argc, char **argv, int *server_fd)
{
  int opt;
  char *ip = "127.0.0.1";
  int port = 25555;

  while ((opt = getopt(argc, argv, "i:p:h")) != -1)
  {
    switch (opt)
    {
    case 'i':
      ip = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'h':
      printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", argv[0]);
      printf("-i IP_address Default to \"127.0.0.1\"\n");
      printf("-p port_number Default to 25555\n");
      printf("-h Display this help info.\n");
      exit(EXIT_SUCCESS);

    default:
      fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
      exit(EXIT_FAILURE);
    }
  }

  // Setting up the server connection
  struct sockaddr_in server_addr;
  socklen_t addr_size = sizeof(server_addr);
  *server_fd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  if (connect(*server_fd, (struct sockaddr *)&server_addr, addr_size) < 0)
  {
    perror("Error: Server cant establish connection.");
    exit(1);
  }
}

int main(int argc, char *argv[])
{
  int server_fd;
  fd_set read_fds;
  int max_fd;
  parse_connect(argc, argv, &server_fd);
  char buffer[1024];

  while (1)
  {
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(server_fd, &read_fds);

    if (server_fd > STDIN_FILENO)
    {
      max_fd = server_fd;
    }
    else
    {
      max_fd = STDIN_FILENO;
    }

    // Wait for input from either stdin or the server
    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
    {
      perror("Error: Select");
      exit(1);
    }

    if (FD_ISSET(STDIN_FILENO, &read_fds))
    {
      // Handle input from the terminal
      if (fgets(buffer, sizeof(buffer), stdin) == NULL)
      {
        break;
      }
      buffer[strcspn(buffer, "\n")] = 0;
      send(server_fd, buffer, strlen(buffer), 0);
    }

    if (FD_ISSET(server_fd, &read_fds))
    {
      // Handle input from the server
      int recvbytes = recv(server_fd, buffer, sizeof(buffer), 0);
      if (recvbytes <= 0)
      {
        printf("Server connection terminated.\n");
        break;
      }
      buffer[recvbytes] = 0;
      printf("%s\n", buffer);
    }
  }
  close(server_fd);
  return 0;
}
