/*************************************************************************************************
 * Name        : client.c
 * Author      : Alice Agnoletto
 *************************************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdbool.h>

struct Player
{
    int fd;
    int score;
    char name[128];
};

void parse_connect(int argc, char **argv, int *server_fd)
{
    int c;
    char *ip_address = "127.0.0.1";
    int port_number = 25555;

    while (1)
    {
        c = getopt(argc, argv, "i:p:h");
        if (c == -1)
            break;
        switch (c)
        {
        case 'i':
            ip_address = optarg;
            break;
        case 'p':
            port_number = atoi(optarg);
            break;
        case 'h':
            printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", argv[0]);
            printf("  -i IP_address       Default to \"127.0.0.1\";\n");
            printf("  -p port_number      Default to 25555;\n");
            printf("  -h                  Display this help info.\n");
            exit(0);
        default:
            fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
            exit(EXIT_FAILURE);
        }
    }
    struct sockaddr_in server_addr;
    socklen_t addr_size = sizeof(server_addr);

    /* STEP 1:
    Create a socket to talk to the server;
    */
    *server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = inet_addr(ip_address);

    /* STEP 2:
    Try to connect to the server.
    */
    connect(*server_fd, (struct sockaddr *)&server_addr, addr_size);
}

void get_name(int fd, struct Player *player)
{
    char buffer[128];
    printf("Please type your name: ");
    fflush(stdout);
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    strncpy(player->name, buffer, sizeof(player->name));
    player->name[sizeof(player->name) - 1] = '\0';
    send(fd, buffer, strlen(buffer), 0);
}

int main(int argc, char *argv[])
{
    int server_fd;
    parse_connect(argc, argv, &server_fd);

    fd_set read_fds;
    int max_fd = server_fd + 1;

    char buffer[1024];

    struct Player player;

    get_name(server_fd, &player);

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(server_fd, &read_fds);

        max_fd = (server_fd > STDIN_FILENO ? server_fd : STDIN_FILENO) + 1; // RE,M]]MOVE
        if (select(max_fd, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select error");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(server_fd, &read_fds))
        {
            // Handle incoming messages from server
            int recv_bytes = recv(server_fd, buffer, sizeof(buffer) - 1, 0);
            if (recv_bytes > 0)
            {
                buffer[recv_bytes] = '\0';
                printf("%s", buffer);
                fflush(stdout);
            }
            else
            {
                printf("\n\nGAME OVER!!\n");
                break;
            }
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            // Handle user input
            if (fgets(buffer, sizeof(buffer), stdin) == NULL)
                continue;
            buffer[strcspn(buffer, "\n")] = 0;
            send(server_fd, buffer, strlen(buffer), 0);
        }
    }

    close(server_fd);

}
