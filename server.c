
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>

// Only allowing for 3 maximum connections
#define MAX_CONN 3

// Player Struct
struct Player
{
    int fd;
    int score;
    char name[128];
};

// Questions Struct
struct Entry
{
    char prompt[1024];
    char options[3][50];
    int answer_idx;
};

// Function to read questions from a file
int read_questions(struct Entry *arr, char *filename)
{
    int questionCounter = 0;
    char answerStr[50];

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("Error: Can't open file");
        exit(1);
    }

    while (questionCounter < 50)
    {
        // Read the questions
        if (fgets(arr[questionCounter].prompt, 1024, fp) == NULL)
        {
            break;
        }

        arr[questionCounter].prompt[strcspn(arr[questionCounter].prompt, "\n")] = 0;

        // Read the 3 options
        if (fscanf(fp, "%49s %49s %49s\n", arr[questionCounter].options[0], arr[questionCounter].options[1], arr[questionCounter].options[2]) != 3)
            break;

        // Read the correct answer
        if (fscanf(fp, "%49s\n", answerStr) != 1)
            break;

        //  Check which option matches the answer
        for (int i = 0; i < 3; i++)
        {
            if (strcmp(arr[questionCounter].options[i], answerStr) == 0)
            {
                arr[questionCounter].answer_idx = i;
                break;
            }
        }

        questionCounter++;
    }

    fclose(fp);
    return questionCounter;
}

// Helper function to send data to all clients
void sendAll(struct Player *players, char *msg)
{
    for (int i = 0; i < MAX_CONN; i++)
    {
        if (players[i].fd != -1)
        {
            send(players[i].fd, msg, strlen(msg), 0);
        }
    }
}

// Helper function to close all connections
void close_connections(struct Player *players, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (players[i].fd != -1)
        {
            close(players[i].fd);
        }
    }
}

int main(int argc, char *argv[])
{
    // Initializes the players and questions array
    struct Player players[MAX_CONN] = {{-1, 0, {0}}};
    struct Entry questions[50];

    int num_questions = read_questions(questions, "questions.txt");
    int shouldStartGame = 0;

    // Establishing the server with getopt
    int opt;
    char *quest = "questions.txt";
    char *ip = "127.0.0.1";
    int port = 25555;

    while ((opt = getopt(argc, argv, "f:i:p:h")) != -1)
    {
        switch (opt)
        {
        case 'f':
            quest = optarg;
            break;
        case 'i':
            ip = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;

        case 'h':
            printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n", argv[0]);
            printf("-f question_file Default to \"questions.txt\"\n");
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
    int server_fd;
    int client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in in_addr;
    socklen_t addr_size = sizeof(in_addr);

    // Create and set up a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    // Bind the file descriptor with address structure so that clients can find the address
    int i =
        bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr));
    if (i < 0)
    {
        perror("binding failed\n");
        exit(1);
    }

    // Listen to at most  incoming connections for simplicity
    if (listen(server_fd, MAX_CONN) == 0)
    {
        printf("Welcome to 392 Trivia\n");
        printf("Listening\n");
    }
    else
    {
        perror("listening failed\n");
        exit(1);
    }

    // Accept connections from clients to enable communication
    fd_set myset;

    // first need to monitor socket
    FD_SET(server_fd, &myset); // add specific fd to the set
    int maxfd = server_fd;     // the max fd in the set
    int n_conn = 0;            // number of connections
    int cfds[MAX_CONN];        // store all client fds
    for (int i = 0; i < MAX_CONN; i++)
    {
        cfds[i] = -1;
    }

    char *receipt = "Read\n";
    int recvbytes = 0;
    char buffer[1024];

    while (1)
    {
        // Re-initalize fd_set
        FD_SET(server_fd, &myset);
        maxfd = server_fd;
        for (int i = 0; i < MAX_CONN; i++)
        {
            if (cfds[i] != -1)
            {
                FD_SET(cfds[i], &myset);
                if (cfds[i] > maxfd)
                    maxfd = cfds[i];
            }
        }

        // Monitor all fd in the set and cccepting players logic
        select(maxfd + 1, &myset, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &myset))
        {
            client_fd = accept(server_fd,
                               (struct sockaddr *)&in_addr,
                               &addr_size);

            // check if new connectin is coming in
            if (n_conn < MAX_CONN)
            {
                printf("New connection detected!\n");

                // Greeting for the new player
                char *newPlayer = "Please type your name:\n";
                send(client_fd, newPlayer, strlen(newPlayer), 0);

                // get player name info
                char nameBuffer[128];
                int nameLen = read(client_fd, nameBuffer, 128);
                nameBuffer[nameLen] = 0;

                printf("Hi %s!\n", nameBuffer);

                // Storing player info
                players[n_conn].fd = client_fd;
                strncpy(players[n_conn].name, nameBuffer, sizeof(players[n_conn].name) - 1);
                players[n_conn].score = 0;
                n_conn++;

                for (int i = 0; i < MAX_CONN; i++)
                {
                    if (cfds[i] == -1)
                    {
                        cfds[i] = client_fd;
                        break;
                    }
                }
            }
            else
            {
                printf("Max connection reached!\n");
                close(client_fd);
            }
        }

        // Check if any client is ready
        for (int i = 0; i < MAX_CONN; i++)
        {
            if (cfds[i] != -1 && FD_ISSET(cfds[i], &myset))
            {
                recvbytes = read(cfds[i], buffer, 1024);
                if (recvbytes == 0)
                {
                    printf("Lost Connection!\n");
                    close(cfds[i]);
                    cfds[i] = -1;
                    n_conn--;
                }
                else
                {
                    buffer[recvbytes] = 0;
                    printf("[Client]: %s", buffer);
                    write(cfds[i], receipt, strlen(receipt));
                }
            }
        }

        // check if max input is reached and get keyboard input to start game
        if (n_conn == MAX_CONN && !shouldStartGame)
        {
            printf("Press ENTER to start the game...\n");
            getchar();
            shouldStartGame = 1;
            system("figlet THE GAME STARTS NOW!");
        }

        // Starting the Game
        if (shouldStartGame)
        {
            char message[1500];
            for (int i = 0; i < num_questions; i++)
            {
                // Print out Question
                sprintf(message, "Question %d: %s\n1: %s\n2: %s\n3: %s\n", i + 1, questions[i].prompt, questions[i].options[0], questions[i].options[1], questions[i].options[2]);
                printf("%s\n", message);
                sprintf(message, "Question %d: %s\nPress 1: %s\nPress 2: %s\nPress 3: %s\n", i + 1, questions[i].prompt, questions[i].options[0], questions[i].options[1], questions[i].options[2]);
                sendAll(players, message);

                // Prepare to collect answers
                fd_set read_fds;
                FD_ZERO(&read_fds);
                int max_fd = 0;

                for (int j = 0; j < MAX_CONN; j++)
                {
                    if (players[j].fd != -1)
                    {
                        FD_SET(players[j].fd, &read_fds);
                        if (players[j].fd > max_fd)
                            max_fd = players[j].fd;
                    }
                }

                if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
                {
                    perror("Error: Select");
                    exit(1);
                }

                // Read the answer from the first client
                for (int j = 0; j < MAX_CONN; j++)
                {
                    if (players[j].fd != -1 && FD_ISSET(players[j].fd, &read_fds))
                    {
                        int readBytes = read(players[j].fd, buffer, 1024);
                        if (readBytes <= 0)
                        {
                            // Handle errors or disconnections
                            continue;
                        }

                        buffer[readBytes] = '\0';
                        int answer = atoi(buffer) - 1;

                        if (answer == questions[i].answer_idx)
                        {
                            players[j].score += 1;
                        }
                        else
                        {
                            players[j].score -= 1;
                        }

                        // Notify all clients who answered and what the current score is
                        sprintf(message, "%s answered first with %s. Their score is now %d.\n", players[j].name, questions[i].options[answer], players[j].score);
                        sendAll(players, message);

                        // print each players individual score
                        for (int h = 0; h < MAX_CONN; h++)
                        {
                            sprintf(message, "%s's score: %d\n", players[h].name, players[h].score);
                            sendAll(players, message);
                        }
                        break;
                    }
                }

                // Send correct answer to all clients
                sprintf(message, "Correct answer: %s\n", questions[i].options[questions[i].answer_idx]);
                sendAll(players, message);
            }

            // once all the questions have been asnwersed, display the winner
            int maxScore = 0;
            bool isTie = false;
            int numWinners = 0;

            // First, find the max score
            for (int i = 0; i < MAX_CONN; i++)
            {
                if (players[i].score > maxScore)
                {
                    maxScore = players[i].score;
                }
            }

            // players that have that max score
            for (int i = 0; i < MAX_CONN; i++)
            {
                if (players[i].score == maxScore)
                {
                    numWinners++;
                }
            }

            if (numWinners > 1)
            {
                isTie = true;
            }

            if (!isTie)
            {
                for (int i = 0; i < MAX_CONN; i++)
                {
                    if (players[i].score == maxScore)
                    {
                        sprintf(message, "The winner is %s with %d points!\n", players[i].name, maxScore);
                        printf("%s", message);
                        sendAll(players, message);

                        system("figlet GAME OVER");
                        close_connections(players, MAX_CONN);
                        close(server_fd);
                        exit(EXIT_SUCCESS);
                    }
                }
            }
            else
            {
                for (int i = 0; i < MAX_CONN; i++)
                {
                    if (players[i].score == maxScore)
                    {
                        sprintf(message, "%s tied with %d points!\n", players[i].name, maxScore);
                        printf("%s", message);
                        sendAll(players, message);
                    }
                }
            }
            system("figlet GAME OVER");
            close_connections(players, MAX_CONN);
            close(server_fd);
            exit(EXIT_SUCCESS);
        }
    }
    return 0;
}
