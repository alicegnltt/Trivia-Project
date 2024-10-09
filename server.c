/*************************************************************************************************
 * Name        : server.c
 * Author      : Alice Agnoletto
 *************************************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

// colors
#define RED "\x1b[31m"
#define ORANGE "\x1b[33m"
#define YELLOW "\x1b[93m"
#define GREEN "\x1b[32m"
#define BLUE "\x1b[34m"
#define INDIGO "\x1b[94m"
#define VIOLET "\x1b[95m"
// normal textx color
#define COLOR_RESET "\x1b[0m"

#define MAX_QS 50
#define MAX_PROMPT_LEN 1024
#define MAX_OPTIONS_LEN 50
#define N_OPTIONS 3

#define MAX_CONN 3 // change this to change the # of ppl who can connect 
#define BUFFER_SIZE 1024

struct Entry
{
    char prompt[1024];
    char options[3][50];
    int answer_idx;
};

struct Player
{
    int fd;
    int score;
    char name[128];
};

struct Player players[MAX_CONN];

int read_questions(struct Entry *arr, char *filename)
{
    char row[1024];
    int counter = 0;
    const char delimiter[] = " ";

    FILE *textfile = fopen(filename, "r");
    if (!textfile)
    {
        fprintf(stderr, "Failed to open text file: %s\n", strerror(errno));
        return -1;
    }

    while (fgets(row, sizeof(row), textfile) && counter < MAX_QS)
    {
        row[strcspn(row, "\n")] = 0;
        // get question
        strncpy(arr[counter].prompt, row, sizeof(arr[counter].prompt));

        // get options
        if (fgets(row, sizeof(row), textfile))
        {
            row[strcspn(row, "\n")] = 0;
            char *token = strtok(row, delimiter);
            for (int i = 0; i < N_OPTIONS && token != NULL; i++)
            {
                strncpy(arr[counter].options[i], token, sizeof(arr[counter].options[i]));
                token = strtok(NULL, delimiter);
            }
        }
        // get right answer
        if (fgets(row, sizeof(row), textfile))
        {
            row[strcspn(row, "\n")] = 0;
            for (int i = 0; i < N_OPTIONS; i++)
            {
                if (strcmp(arr[counter].options[i], row) == 0)
                {
                    arr[counter].answer_idx = i;
                    break;
                }
            }
        }
        fgets(row, sizeof(row), textfile);
        counter++;
    }
    fclose(textfile);
    return counter;
}

void update_score(struct Player *player, char *buffer, int answer_id)
{
    int user_id = atoi(buffer);
    user_id--;
    if (user_id == answer_id)
    {
        player->score += 1;
    }
    else
    {
        player->score -= 1;
    }
}

// puts the index of the winner/winners in an array 
void get_winner(struct Player *players, int *winners, int *highest_score)
{
    *highest_score = -9999;
    // find highest score
    for (int i = 0; i < MAX_CONN; i++)
    {
        if (players[i].fd != -1)
        {
            if (players[i].score > *highest_score)
            {
                *highest_score = players[i].score;
            }
        }
    }

    // find all players with highest score
    int winner_count = 0;
    for (int i = 0; i < MAX_CONN; i++)
    {
        if (players[i].fd != -1 && players[i].score == *highest_score)
        {
            winners[winner_count++] = i;
        }
    }

    // Mark the end of winners
    if (winner_count < MAX_CONN)
    {
        winners[winner_count] = -1;
    }
}

int main(int argc, char *argv[])
{
    int server_fd;
    int client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in in_addr;
    socklen_t addr_size = sizeof(in_addr);

    int question_index = 0;
    bool question_sent = false;

    // Task 1
    int c;
    char *question_file = "questions.txt";
    char *ip_address = "127.0.0.1";
    int port_number = 25555;

    while (1)
    {
        c = getopt(argc, argv, "f:i:p:h");
        if (c == -1)
            break;
        switch (c)
        {
        case 'f':
            question_file = optarg;
            break;
        case 'i':
            ip_address = optarg;
            break;
        case 'p':
            port_number = atoi(optarg);
            break;
        case 'h':
            printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n", argv[0]); // argv[0]
            printf("  -f question_file    Default to \"questions.txt\";\n");
            printf("  -i IP_address       Default to \"127.0.0.1\";\n");
            printf("  -p port_number      Default to 25555;\n");
            printf("  -h                  Display this help info.\n");
            exit(0);
        default:
            fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
            exit(EXIT_FAILURE);
        }
    }

    /* STEP 1
        Create and set up a socket
    */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = inet_addr(ip_address);

    /* STEP 2
        Bind the file descriptor with address structure
        so that clients can find the address
    */
    int bnd =
        bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr));

    if (bnd < 0)
    {
        perror("bind");
        exit(1);
    }

    /* STEP 3
        Listen to at most <MAX_CONN> incoming connections
    */

    if (listen(server_fd, MAX_CONN) == 0)
    {
        printf("\n\n%s Welcome to 392 Trivia!\n\n", YELLOW);
        printf("%s waiting for all players to join...\n", COLOR_RESET);
    }
    else
    {
        perror("listen");
        exit(1);
    }

    /* STEP 4
        Accept connections from clients
        to enable communication
    */

    fd_set myset;
    FD_SET(server_fd, &myset);
    int maxfd = server_fd;
    int n_conn = 0;

    int cfds[MAX_CONN];
    for (int i = 0; i < MAX_CONN; i++)
        cfds[i] = -1;

    int recvbytes = 0;
    char buffer[1024];

    // create players array
    struct Player *players;
    players = malloc(MAX_CONN * sizeof(struct Player));

    int winners[MAX_CONN];
    int highest_score;

    // read questions from text file
    struct Entry questions[MAX_QS];
    int n_questions = read_questions(questions, question_file);
    if (n_questions < 0)
    {
        fprintf(stderr, "Error reading questions.\n");
        free(players);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        maxfd = server_fd;
        FD_SET(server_fd, &myset);
        for (int i = 0; i < MAX_CONN; i++)
        {
            if (cfds[i] != -1)
            {
                FD_SET(cfds[i], &myset);
                if (cfds[i] > maxfd)
                    maxfd = cfds[i];
            }
        }

        select(maxfd + 1, &myset, NULL, NULL, NULL);
        if (FD_ISSET(server_fd, &myset))
        {
            client_fd = accept(server_fd, (struct sockaddr *)&in_addr, &addr_size);

            // check if max number of connections is filled before connecting to new client
            if (n_conn < MAX_CONN)
            {
                printf("%s New connection detected!%s\n", GREEN, COLOR_RESET);
                n_conn++;
                char namebuff[128];
                int namebytes = read(client_fd, namebuff, sizeof(namebuff) - 1);
                namebuff[namebytes] = '\0';
                if (namebytes > 0) // && n_conn < MAX_CONN) // CHANGED - TRY
                {

                    printf("%s Hi %s!%s\n", GREEN, namebuff, COLOR_RESET);
                    int n = n_conn - 1;
                    // create player object
                    players[n].fd = client_fd;
                    players[n].score = 0;
                    snprintf(players[n].name, sizeof(players[n].name), "%s", namebuff);
                }
                else
                {
                    // Handle error or disconnect
                    close(client_fd);
                    continue;
                }

                // add new file descriptor into array.
                for (int i = 0; i < MAX_CONN; i++)
                {
                    if (cfds[i] == -1)
                    {
                        cfds[i] = client_fd;
                        break;
                    }
                }
                if (n_conn == MAX_CONN)
                {
                    system("figlet 'The game starts now!'");
                    // printf("\n\n%s The game starts now!%s\n\n", VIOLET, COLOR_RESET);
                }
            }
            else
            {
                printf("%sMax connections reached!%s \n", RED, COLOR_RESET);
                char namebuff[128];
                int namebytes = read(client_fd, namebuff, sizeof(namebuff) - 1);
                namebuff[namebytes] = '\0';
                if (namebytes > 0)
                {
                    // send message to client
                    char message[BUFFER_SIZE];
                    snprintf(message, sizeof(message), "The game is full! %s, you're unable to join!",namebuff);
                    write(client_fd, message, strlen(message));
                }
                else
                {
                    // Handle error or disconnect
                    close(client_fd);
                    continue;
                }

                close(client_fd);
            }
        }
        if (n_conn == MAX_CONN)
        {

            if (!question_sent && question_index < n_questions)
            {
                // start the game here
                // 1. print question in server
                printf("Question %d: %s\n", question_index + 1, questions[question_index].prompt);
                for (int i = 0; i < N_OPTIONS; i++)
                {
                    printf("%d: %s\n", i + 1, questions[question_index].options[i]);
                }
                printf("\n");
                // 2. create message with stuff to send question to clients
                char message[MAX_PROMPT_LEN];
                int len = snprintf(message, sizeof(message),
                                   "Question %d: %s\nPress 1: %s\nPress 2: %s\nPress 3: %s\n\n",
                                   question_index + 1, questions[question_index].prompt,
                                   questions[question_index].options[0], questions[question_index].options[1], questions[question_index].options[2]);

                if (len < 0 || len >= sizeof(message))
                {
                    fprintf(stderr, "Failed to format question for clients.\n");
                    continue;
                }

                // 3. send to clients
                for (int i = 0; i < MAX_CONN; i++)
                {
                    if (players[i].fd != -1)
                    {
                        write(players[i].fd, message, strlen(message));
                    }
                }

                question_sent = true;
            }
            if (question_sent)
            {
                for (int p = 0; p < MAX_CONN; p++)
                {

                    if (players[p].fd != -1 && FD_ISSET(players[p].fd, &myset))
                    {
                        recvbytes = read(players[p].fd, buffer, sizeof(buffer));
                        if (recvbytes == 0)
                        {
                            printf("Lost connection!\n");
                            // close file descriptor and decrement the number of connctions
                            close(cfds[p]);
                            n_conn--;
                            cfds[p] = -1;
                            goto end;
                        }
                        else
                        {
                            buffer[recvbytes] = '\0';
                            printf("\n%s answered first! %s\n", players[p].name, buffer);
                            update_score(&players[p], buffer, questions[question_index].answer_idx);
                            question_sent = false;
                            break;
                        }
                    }
                }

                if (!question_sent)
                {

                    printf("\nRIGHT ANSWER: %s\n", questions[question_index].options[questions[question_index].answer_idx]);

                    char right_answer[MAX_PROMPT_LEN];
                    int len = snprintf(right_answer, sizeof(right_answer),
                                       "Right answer: %s\n\n", questions[question_index].options[questions[question_index].answer_idx]);

                    if (len < 0 || len >= sizeof(right_answer))
                    {
                        fprintf(stderr, "Failed to format question for clients.\n");
                        continue;
                    }

                    for (int i = 0; i < MAX_CONN; i++)
                    {
                        if (players[i].fd != -1)
                        {
                            write(players[i].fd, right_answer, strlen(right_answer));
                        }
                    }

                    printf("\n%sSCOREBOARD: \n", VIOLET);
                    for (int i = 0; i < MAX_CONN; i++)
                    {
                        if (players[i].fd != -1)
                        {
                            printf("[ %s ] : %s%d%s | ", players[i].name, YELLOW, players[i].score, VIOLET);
                        }
                    }
                    printf("\n\n%s", COLOR_RESET);

                    question_index++;
                    if (question_index < n_questions)
                    {
                        // 1. print question in server
                        printf("Question %d: %s\n", question_index + 1, questions[question_index].prompt);
                        for (int i = 0; i < N_OPTIONS; i++)
                        {
                            printf("%d: %s\n", i + 1, questions[question_index].options[i]);
                        }
                        // 2. create message with stuff to send question to clients
                        char message[MAX_PROMPT_LEN];
                        int len = snprintf(message, sizeof(message),
                                           "Question %d: %s\nPress 1: %s\nPress 2: %s\nPress 3: %s\n",
                                           question_index + 1, questions[question_index].prompt,
                                           questions[question_index].options[0], questions[question_index].options[1], questions[question_index].options[2]);

                        if (len < 0 || len >= sizeof(message))
                        {
                            fprintf(stderr, "Failed to format question for clients.\n");
                            continue;
                        }

                        // 3. send to clients
                        for (int i = 0; i < MAX_CONN; i++)
                        {
                            if (players[i].fd != -1)
                            {
                                write(players[i].fd, message, strlen(message));
                            }
                        }
                        question_sent = true;
                    }
                    else
                    {
                        // printf("%sGAME OVER!\n", RED);
                        system("figlet 'GAME OVER!'");
                        get_winner(players, winners, &highest_score);
                        printf("\n%sCongrats, ", YELLOW);
                        for (int i = 0; winners[i] != -1 && i < MAX_CONN; i++)
                        {
                            printf("%s ", players[winners[i]].name);
                        }
                        printf("!\n");
                        break;
                    }
                }
            }
        }
    }

end:
    for (int i = 0; i < MAX_CONN; i++)
    {
        if (cfds[i] != -1)
        {
            close(cfds[i]);
        }
    }

    close(server_fd);
    free(players);
    exit(EXIT_SUCCESS);
}
