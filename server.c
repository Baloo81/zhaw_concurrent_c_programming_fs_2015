#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/mman.h>

const int MaxPlayer = 1000;
static int *playercount;
static int *startsignal;
static int *connectioncount;
static int *endsignal;
int **board;
char **playername;



void printboard(int boardsize)
{
    printf( "\n");
    for(int i=0;i<boardsize;i++)
    {
        for(int j=0;j<boardsize;j++)
        {
            printf( "%d ", board[i][j] );
        }
        printf( "\n");
    }
    printf( "\n");
}

int checkboard(int boardsize)
{
    int firstfieldvalue = board[0][0];
    for (int i=0;i<boardsize;i++)
    {
        for(int j=0;j<boardsize;j++)
        {
            if(firstfieldvalue != board[i][j])
            {
                return -1;
            }   
        }
    } 
    return firstfieldvalue;
}

int setfield(int xvalue, int yvalue, int player, int boardsize)
{
    if(xvalue >= boardsize || yvalue >= boardsize)
    {
        return -1;
    }
    else {
        board[yvalue][xvalue] = player;
        return 1;
    }
}

int getfield(int xvalue, int yvalue, int boardsize)
{
    if(xvalue >= boardsize || yvalue >= boardsize)
    {
        return -1;
    }
    else 
    {
        return board[yvalue][xvalue];
    }
}

void boardgenerator(int boardsize)
{   
    // initialisiere ein globales board 
    board = mmap(NULL, sizeof(int) * boardsize, PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(board == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(-1);
    }
    for(int i = 0; i < boardsize; i++)
    {
        board[i] = mmap(NULL, sizeof(int) * boardsize, PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(board[i] == NULL)
        {
            fprintf(stderr, "out of memory\n");
            // free(board);
            munmap(board, sizeof(int) * boardsize);
            munmap(*board, sizeof(int) * boardsize);
            exit(-1);
        }
    }
    // Inhalt des boardes wird mit Null initialisiert (Playername: Server) 
    for(int i=0;i<boardsize;i++)
    {
        for(int j=0;j<boardsize;j++)
        {
            board[i][j] = 0;
        }
    }
}

void printplayerlist()
{
    printf("Es gibt %d Spieler\n", *playercount+1);
    for (int i=0; i<=*playercount ; i++)
        printf("%d : %s \n", i, playername[i]);
    printf( "\n");
}

int setplayername(char buf[256])
{
    *playercount = *playercount+1;;
    if (*playercount > MaxPlayer) 
    {
        printf("No more Player allowed \n");
        *playercount = *playercount-1;;
        return -1;
    }
    strcpy(playername[*playercount], buf);
    // printf("Playercount ist: %d\n", *playercount);
    return *playercount;
}

int getplayerID(char buf[256])
{
    for (int i=0;i<=*playercount;i++)
    {
        if (strcmp (playername[i], buf) == 0)
            return i;        
    }
    return setplayername(buf);
}

void playerlistgenerator()
{
    // Initalisierung Spielerliste
    playername = mmap(NULL, MaxPlayer, PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(playername == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(-1);
    }
    for(int i = 0; i <= MaxPlayer; i++)
    {
        playername[i] = mmap(NULL, sizeof(char) * 256, PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(playername[i] == NULL)
        {
            fprintf(stderr, "out of memory\n");
            munmap(playername, MaxPlayer * sizeof(char) * 256);
            exit(-1);
        }
    }
    
    // Initialisierung Spielerzähler
    playercount = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *playercount = -1;
}

void clientmanager(int, int); /* function prototype */

void servermanager(int boardsize)
{
    while (*startsignal == 0)
    {
        if ((boardsize / 2) <= *connectioncount)
            { 
                *startsignal = 1;
            }
        sleep(1);
    }

    // Check Winner all few Seconds
    while (1) 
    {
        sleep(rand() % 5 + 1); // Set few Seconds 1 <= t <= 30
        printf("Spielfeld überprüfen\n");
        int i = checkboard(boardsize);
        printboard(boardsize);
        printplayerlist();
        if (i > 0) 
        {
            printf("Spieler %s hat das Spiel gewonnen\n", playername[i]);
            *endsignal = i;
            break;
        }
        else
        {
            printf("Es steht noch kein Sieger fest\n\n");
        }
    }
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}


int main(int argc, char *argv[])
{
    
    connectioncount = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *connectioncount = 0;
    startsignal = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *startsignal = 0;
    endsignal = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *endsignal = 0;

    // Boardgrösse initialisieren
    int boardsize;
    if (argc < 3)
    {
        // Falls keine Grösse übergeben wird, ist der Defaultwert 4
        boardsize = 4;         
    }
    else
    {
        boardsize = atoi(argv[2]);
    }  
    
    playerlistgenerator();
    boardgenerator(boardsize);
    setplayername("Server");

    // Kommunikation initialisieren
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        // Falls kein Port übergeben wird, ist der Defaultport 12345
        portno = 12345;
    }
    else 
    {
        portno = atoi(argv[1]);
    }

    printf("\nServerinitialisierung:\n\n");
    printf("Seitenlänge %d\n", boardsize);
    printf("Serverport %d \n\n", portno);

    // Serverfunktionenaufruf
    if (fork() == 0) 
    {
        servermanager(boardsize);
    } 
    // Clientfunktionenaufruf
    else 
    {
        
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) 
            error("ERROR opening socket");
        bzero((char *) &serv_addr, sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);
        if (bind(sockfd, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) < 0) 
                error("ERROR on binding");
        listen(sockfd,5);
        clilen = sizeof(cli_addr);

         
        while (1) {
            newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            if (newsockfd < 0) 
                error("ERROR on accept");
            pid = fork();
            if (pid < 0)
                error("ERROR on fork");
            if (pid == 0)  {
                close(sockfd);
                clientmanager(newsockfd, boardsize);
                exit(0);
            }
            else close(newsockfd);
        } 

    }

    close(sockfd);
    return 0; /* we never get here */
}

/******** clientmanager() *****************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void clientmanager (int sock, int boardsize)
{
    int n;
    char buffer[256];
    char command[256];

    // Das gehört vermutlich nicht hierher!
    int sieger = -1;
    
    // Warten auf HELLO
    bzero(buffer,256);
    // Übertragener String wird eingelesen 
    n = read(sock,buffer,255);
    if (n < 0) error("ERROR reading from socket");
        
    // printf("Here is the message: %s\n",buffer);

    if (strcmp(buffer, "HELLO\n") == 0)
    {   
        sprintf(buffer,"SIZE %d", boardsize);
        n = write(sock,buffer, sizeof(buffer)+1);
        if (n < 0) error("ERROR writing to socket");
        
        // printf("Connectioncount %d\n", *connectioncount);
        // printf("Startsignal %d\n", *startsignal);
        // printf("Endsignal %d\n", *endsignal);
        *connectioncount = *connectioncount+1;

        // printboard(boardsize); 
        // printplayerlist();
    }
    else 
    {
        n = write(sock,"NACK\n",5);
        if (n < 0) error("ERROR writing to socket");
        // TODO: Abbruch der Aktion
    }
    
    // printf("Startsignal vor Start %d\n", *startsignal);
    // printf("Connectioncount %d\n", *connectioncount);
    
    while (*startsignal <= 0)
    {
        // Warten auf Startsignal
        sleep(1);
    }
    n = write(sock,"START\n",6);
    if (n < 0) error("ERROR writing to socket");
    
    // printf("Startsignal nach Start %d\n", *startsignal);
    // printf("Connectioncount %d\n", *connectioncount);

    while(*endsignal == 0)
    {  
        bzero(buffer,256);
        bzero(command,256);
        // Übertragener String wird eingelesen 
        n = read(sock,buffer,255);
        if (n < 0) error("ERROR reading from socket");
        
        // printf("Here is the message: %s\n",buffer);
        
        char delimiter[] = " \n";
        char *ptr;
        
        // Feld-Erobern-Komando auseinanderschneiden
        ptr = strtok(buffer, delimiter);     
        // printf("Kommando: %s\n", ptr);
        command[0] = ptr[0];
        int e = 0;
        int f = 0;
        ptr = strtok(NULL, delimiter);
        int len = strlen(ptr);
        for(int i=0; i<len; i++){
            e = e * 10 + ( ptr[i] - '0');
        }
        // printf("X-Value: %d\n", e);

        ptr = strtok(NULL, delimiter);
        len = strlen(ptr);
        for(int i=0; i<len; i++){
            f = f * 10 + ( ptr[i] - '0');
        }
        // printf("Y-Value: %d\n", f);


        // TAKE Verarbeitung
        if (strcmp(&command[0], "T") == 0)
        {
            // Nur das Delimeterzeichen "\n" wurde hier verwendet, damit Clientnamen auch blanks enthalten dürfen
            ptr = strtok(NULL, "\n");
            // printf("Name: %s\n", ptr);

            if ((setfield(e,f,getplayerID(ptr),boardsize)) == 1)
            {
                n = write(sock,"TAKEN",5);
                if (n < 0) error("ERROR writing to socket");
                // printf("TAKEN Sent\n");
            } 
            else
            {
                n = write(sock,"INUSE",5);
                if (n < 0) error("ERROR writing to socket");    
            }
            // printboard(boardsize); 
            // printplayerlist();
        }


        // STATUS Verarbeitung
        else if (strcmp(&command[0], "S") == 0)
        {
            bzero(command,256);
            strcpy(command, playername[getfield(e,f,boardsize)]);
            n = write(sock,command,strlen(command));
            if (n < 0) error("ERROR writing to socket");
        }
    }
    // Endsignal auslösen
    bzero(buffer,256);
    sprintf(buffer,"END %s", playername[*endsignal]);
    n = write(sock,buffer, sizeof(buffer)+1);
    if (n < 0) error("ERROR writing to socket");
}

