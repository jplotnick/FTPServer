/*ftpServer.c - a server that can communicate with a client using FTP*/
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>


#include "ftpResponses.h"


/**************** Global Defines ****************/
#define BUF_SIZE ((int)1024)
typedef enum 
{
    CWD     = 0, 
    CDUP    = 1, 
    QUIT    = 2, 
    PASV    = 3, 
    EPSV    = 4, 
    PORT    = 5, 
    EPRT    = 6, 
    RETR    = 7, 
    PWD     = 8, 
    LIST    = 9, 
    HELP    = 10
} FtpCommand;

/**************** Global Variables ****************/
FILE *LogFile = NULL;
int Connfd = 0, Datafd = 0, DataConnState = 0;
char RecBuf[BUF_SIZE], MainPort[BUF_SIZE], NextDataPort[BUF_SIZE], HostIP[BUF_SIZE];
struct sockaddr HostIPinfo;

/**************** Private Function Declarations ****************/
void logMessage(char *message);
int connectToClient(char *port);
void sendMessage(char *message, int len, int isLogged);
int receiveMessage(int isLogged);
void clearMessageBuffer(void);
int parseMessage(char *message, int len);
int ftpLogin(FILE *userfile);//char *user, char *pass);
void connectToPasv(char *port);
int connectToPort(char *host, char *port);
void sendFileMessage(FILE *fname);
FILE *createFile(char *fpath);
int rcvMsg(void);
int writeListData(char *fpath);


/****************************************************************
* Function: main()                                              *
* NOTE: command line arguments should be given as:              *
*       ftpClient <ipAddr> <logFile> (optional)<port>           *
* Summary: connects to the server, requests a username and      *
*          password to log in, and then provides the user with  *
*          ftp command options to send to the server and read   *
*          the responses. Will continue to request user input   *
*          until the user sends the QUIT command.               *
* Args: int argc: the number of command line arguments given    *
*       char **argv: the list of command line arguments given   *        
* Return: -1 for an error, 0 if the user sends the QUIT command *
****************************************************************/
int main(int argc, char **argv)
{
    char *fPath;
    char user[BUF_SIZE], pass[BUF_SIZE], MsgOut[BUF_SIZE], logPath[BUF_SIZE]; 
    int cmd, sfd, n, numLogFiles = 5;
    int ipSockLen;
    int pasv_mode = 0, port_mode = 0;
    FILE *configFile, *usernamefile;
   

    if (argc < 3)
    {
        printf("Error, not enough arguments.");
        return -1;
    } 
    else if (argc > 4)
    {
        printf("Error, too many arguments.");
        return -1;
    }

    strcpy(MainPort, argv[2]);
    strcpy(NextDataPort, "20");

    fPath = argv[1];
    strcpy(logPath, "logfiles/");

    //read from config file
    configFile = fopen("FtpServer.conf", "r");
    
    if (configFile == NULL)
    {
        printf("Bad config file\n");
        return -1;
    }
    else
    {
        while (fgets(RecBuf, BUF_SIZE-1, configFile))
        {
            n = strlen(RecBuf);
            
            //remove end of line
            if (n < BUF_SIZE)
            {
                RecBuf[n-1] = 0;
            }
            else
            {
                RecBuf[BUF_SIZE-1] = 0;  
            }
            // write data to file
            switch(RecBuf[0])
            {
                /*
                case ('#'): //same behavior as default, do nothing
                {
                }
                break;
                */
                case ((int)'l'):
                {
                    strcpy(logPath, "logdirectory=");
                    if (strncmp(RecBuf, logPath, strlen(logPath)) == 0)
                    {
                        strcpy(logPath, &RecBuf[strlen(logPath)]);
                        
                        //printf("logPath = |%s|\n", logPath);

                        //check if directory exists
                        if (opendir(logPath) == NULL)
                        {
                            printf("Using default value for logdirectory.\n");
                            strcpy(logPath, "logfiles");
                        }
                        strcat(logPath, "/");
                        strcat(logPath, fPath);
                        strcpy(fPath, logPath);
                        
                    }
                }
                break;
                case ((int)'n'):
                {
                    strcpy(MsgOut, "numlogfiles=");
                    if (strncmp(RecBuf, MsgOut, strlen(MsgOut)) == 0)
                    {
                        numLogFiles = atoi(&RecBuf[strlen(MsgOut)]);
                    }
                }
                break;
                case ((int)'p'):
                {
                    strcpy(MsgOut, "port_mode=");
                    if (strncmp(RecBuf, MsgOut, strlen(MsgOut)) == 0)
                    {
                        strcpy(MsgOut, &RecBuf[strlen(MsgOut)]);
                        if (strncmp(MsgOut, "YES", 3) == 0)
                        {
                            port_mode = 1;                        
                        }
                        else
                        {
                            port_mode = 0;
                        }
                    }
                    else
                    {
                        strcpy(MsgOut, "pasv_mode=");
                        if (strncmp(RecBuf, MsgOut, strlen(MsgOut)) == 0)
                        {
                            strcpy(MsgOut, &RecBuf[strlen(MsgOut)]);
                            if (strncmp(MsgOut, "YES", 3) == 0)
                            {
                                pasv_mode = 1;                        
                            }
                            else
                            {
                                pasv_mode = 0;
                            }
                        }
                    }
                }
                break;
                case ((int)'u'):
                {
                    strcpy(MsgOut, "usernamefile=");
                    n = strlen(MsgOut);
                    if (strncmp(RecBuf, MsgOut, strlen(MsgOut)) == 0)
                    {
                        strcpy(MsgOut, &RecBuf[n]);
                        //MsgOut[strlen(MsgOut)] = 0;
                        //try opening file to make sure directory exists
                        usernamefile = fopen(MsgOut, "r");
                        if (usernamefile == NULL)
                        {
                            printf("Bad usernamefile.\n");
                            return -1;
                        }
                        
                    }
                }
                break;
                default :
                {
                }
                break;
            }
        }
        fclose(configFile);
        if ((pasv_mode == 0) && (port_mode == 0))
        {
            printf("Bad config file, need pasv_mode or port_mode to be YES.\n");
            return -1;
        }
    }


    //allow up to numLogFiles log files
    n = 0;
    strcpy(MsgOut, fPath);
    while ((access(MsgOut, F_OK) != -1) && (n < numLogFiles))
    {
        strcpy(MsgOut, fPath);
        sprintf(MsgOut, "%s.%.3d", fPath, n);
        //printf("MsgOut = |%s|\n", MsgOut);
        n++;
    }
    if (access(MsgOut, F_OK) != -1)
    {
        strcpy(MsgOut, fPath);
        strcat(MsgOut, ".000");
    }
    if (access(fPath, F_OK) != -1)
    {
        rename(fPath, MsgOut);
    }


    LogFile = fopen(fPath, "a+");
    if (LogFile == NULL)
    {
        logMessage("bad log file\n");
        return -1;
    }



    // connect to the server
    sfd = connectToClient(MainPort);
    if (sfd == -1)
        return -1;
    if ((Connfd = accept(sfd, (struct sockaddr*)NULL, NULL)) == -1)
    {
        logMessage("connectToClient failed\n");
        return -1;
    }

    // need to get IP address to send in PASV



    if (ftpLogin(usernamefile) != 0)
    {
        return -1;
    }
    
    fclose(usernamefile);
    // loop for users to input FTP commands
    while (1)
    {
        if (receiveMessage(1) == 0)
        {
            continue;
        }
        cmd = parseMessage(RecBuf, strlen(RecBuf));
        
        // switch statement to define behavior for commands
        switch(cmd)
        {
            case CWD:
            {
                // build CWD command
                processFtpCwd(RecBuf);

                // send command
                sendMessage(RecBuf, strlen(RecBuf), 1);
            }
            break;
            case CDUP:
            {
                // build CDUP command
                processFtpCdup(RecBuf);

                // send message
                sendMessage(RecBuf, strlen(RecBuf), 1);

            }
            break;
            case QUIT:
            {
                // build QUIT command
                processFtpQuit(RecBuf);
                
                // send message
                sendMessage(RecBuf, strlen(RecBuf), 1);

                // close connection
                close(Connfd);
                // exit program
                return 0;
            }
            break;
            case PASV:
            {
                char tempPort[BUF_SIZE];
                if (pasv_mode == 1)
                {  
                    //printf("before connectToPasv NextDataPort = |%s|\n", NextDataPort);
                    connectToPasv(tempPort);
                    if (DataConnState != 0)
                    {
                        sprintf(RecBuf, "(127,0,0,1,%d,%d)", atoi(tempPort)/256, atoi(tempPort)%256);
                        processFtpPasv(RecBuf);
                        //printf("RecBuf = |%s|\n", RecBuf);
                        sendMessage(RecBuf, strlen(RecBuf), 1);
                    }
                }
                else
                {
                    logMessage("pasv_mode DISABLED");
                    processFtpInvalid(RecBuf);

                    // send command
                    sendMessage(RecBuf, strlen(RecBuf), 1);
                }
            }
            break;
            case EPSV:
            {                
                processFtpEpsv(RecBuf);
                // send command and verify response
                sendMessage(RecBuf, strlen(RecBuf), 1);
            }
            break;
            case PORT:
            {
                if (port_mode == 1)
                {   
                    // build PORT command
                    processFtpPort(RecBuf);
                    // need to create new socket connection using client input to PORT function
                    
                    // send command
                    sendMessage(RecBuf, strlen(RecBuf), 1);
                }
                else
                {
                    logMessage("port_mode DISABLED");
                    processFtpInvalid(RecBuf);

                    // send command
                    sendMessage(RecBuf, strlen(RecBuf), 1);
                }
            }
            break;
            case EPRT:
            {
                processFtpEprt(RecBuf);
                // send command and check if response is bad
                sendMessage(RecBuf, strlen(RecBuf), 1);
            }
            break;
            case RETR:
            {
                //FILE *tempFile;

                // build RETR command
                processFtpRetr(RecBuf);

                // send command and verify response
                sendMessage(RecBuf, strlen(RecBuf), 1);
            }
            break;
            case PWD:
            {
                // build PWD command
                processFtpPwd(RecBuf);

                // send command
                sendMessage(RecBuf, strlen(RecBuf), 1);            
            }
            break;
            case LIST:
            {
                char tempPath[BUF_SIZE];
                // send command
                if (DataConnState == 1)
                {
                    strcpy(tempPath, RecBuf);
                    tempPath[strlen(tempPath)-2] = 0;
                    if (tempPath[0] == 0)
                        strcpy(tempPath, ".");
                    else
                        strcpy(tempPath, &tempPath[1]);
                    // build LIST command
                    processFtpList(RecBuf);
                    sendMessage(RecBuf, strlen(RecBuf), 1);
                    //logMessage("Data transfer started.\n");
                    writeListData(tempPath);
                    
                    strcpy(RecBuf, "226 Directory send okay.\r\n");
                    sendMessage(RecBuf, strlen(RecBuf), 1);
                        // print data from server to stdout
                    //sendFileMessage(stdout);
                    //logMessage("Data transfer finished.\n");
                }
                else
                {
                    // build 425 response "Use PORT or PASV first." command
                    strcpy(RecBuf, "425 Use PORT or PASV first.\r\n");
                    sendMessage(RecBuf, strlen(RecBuf), 1);

                }
            }
            break;
            case HELP:
            {
                // build HELP command
                processFtpHelp(RecBuf);

                // send command
                sendMessage(RecBuf, strlen(RecBuf), 1);
            }
            break;
            default:
            {
                processFtpInvalid(RecBuf);

                // send command
                sendMessage(RecBuf, strlen(RecBuf), 1);
                //logMessage("Error, invalid command.\n");
            }
            break;
        }

    }

    close(Connfd);
    return 0;
}

/**************** Private Function Definitions ****************/

/****************************************************************
* Function: connectToClient()                                   *
* Summary: connects to a client and returns an int that can be  *
*          used to access the client                            *
* Args: char *port: the port to use for the connection          * 
* Return: -1 for a failure, otherwise the return value to be    *
*         used to access the client                             *
****************************************************************/
int connectToClient(char *port) 
{
    char msg[BUF_SIZE];
    int cfd, s;
    struct addrinfo temp;
    struct addrinfo *info;

    // set temp to all 0's
    memset(&temp, 0, sizeof(struct addrinfo));
    //set up temp struct with family and socktype
    temp.ai_family = AF_INET;
    temp.ai_socktype = SOCK_STREAM;
    temp.ai_flags = AI_PASSIVE;

    // get address info for connecting to client
    s = getaddrinfo(NULL, port, &temp, &info);
    if (s != 0)
    {
        logMessage("Error, invalid addrinfo \n");
        return -1;
    }

    while(info != NULL)
    {
        // set up socket to communicate with client
        if ((cfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1)
        {
            continue;
            //logMessage("Error, invalid cfd \n");
        }

        // connect to the client and verify that connection is good
        if (bind(cfd, info->ai_addr, info->ai_addrlen) != -1)
        {
            //connect successful
            logMessage("good connect in connectToClient\n");
            break;
        }
        info = info->ai_next;
    }
    if (cfd == -1)
    {
        logMessage("Error, could not connect to cfd\n");
        close(cfd);
        return -1;
    }
    //printf("before listen in connectToClient\n");
    listen(cfd, 10);
    //printf("before accept in connectToClient\n");
    //cfd = accept(cfd, (struct sockaddr*)NULL, NULL);
    //printf("after accept in connectToClient\n");
    return cfd; //accept(cfd, (struct sockaddr*)NULL, NULL);
}


/****************************************************************
* Function: logMessage()                                        *
* Summary: logs a message with a timestamp                      *
* Args: char *message: the message to log                       *
****************************************************************/
void logMessage(char *message)
{
    char temp[BUF_SIZE], msString[BUF_SIZE];
    int ms;
    time_t ticks;
    struct tm *logTime;
    struct timeval msTime;

    // get date and time for timestamp
    gettimeofday(&msTime, NULL);
    ticks = msTime.tv_sec;
    ms = msTime.tv_usec/100;
    logTime = localtime(&ticks);

    // add timestamp to message
    strftime(temp, BUF_SIZE, " %x%X.", logTime);
    sprintf(msString, "%.4d ", ms);
    strcat(temp, msString);
    strcat(temp, message);

    // write to log
    fputs(temp, LogFile);
    
    // print to stdout
    printf("%s\n", message);
}

/****************************************************************
* Function: sendMessage()                                       *
* Summary: sends a message to the client                        *
* Args: char *message: the message to send                      *
*       int len: the length of the message to be sent           *
*       int isLogged: a flag to set if the message is logged    *
****************************************************************/
void sendMessage(char *message, int len, int isLogged)
{
    char msg[len];
    strncpy(msg, message, len);
    //printf("Connfd = %d, msg = %s, len = %d\n", Connfd, msg, len);
    write(Connfd, msg, len);
    strcpy(msg, "Sent: ");
    strcat(msg, message);
    if (isLogged)
    {
        logMessage(msg);
    }
}

/****************************************************************
* Function: receiveMessage()                                    *
* Summary: reads a message from the client                      *
* Args: int isLogged: a flag to set if the response is logged   *
****************************************************************/
int receiveMessage(int isLogged)
{
    char msg[BUF_SIZE];
    int n = 0;
    
    n = recv(Connfd, RecBuf, BUF_SIZE-1, 0);
    
    if (isLogged && (n > 0))
    {
        // terminate string
        RecBuf[n] = 0;
        strcpy(msg, "Received: ");
        strcat(msg, RecBuf);
        //printf("n = %d\n", n);
        logMessage(msg);
        //printf("n = %d\n", n);
    }
    return n;
}

/****************************************************************
* Function: parseMessage()                                      *
* Summary: parses a message from the client so the correct      *
*          response can be determined                           *
* Args: char *message: the message received                     *
*       int len: the length of the message received             *
* Return: the index of the message in validCommands if the      *
*       message is valid, otherwise -1                          *
****************************************************************/
int parseMessage(char *message, int len)
{
    int i, valid = -1;
    char msg[BUF_SIZE];
    char *validCommands[] = {"CWD", "CDUP", "QUIT", "PASV", "EPSV", "PORT", "EPRT", "RETR", "PWD", "LIST", "HELP"};

    for (i = 0; i < (sizeof(validCommands)/sizeof(validCommands[0])); i++)
    {
        strcpy(msg, validCommands[i]);
        if (strncmp(message, msg, strlen(msg)-1) == 0)
            valid = i;
    }
    if (valid == -1)
        return -1;
    strcpy(msg, &message[strlen(validCommands[valid])]);
    strcpy(message, msg);
    return valid;   
}

/****************************************************************
* Function: ftpLogin()                                          *
* Summary: logs a client into the FTP server with a username    *
*       and password                                            *
* Args: FILE *userfile: file which contains valid username      *
*       and password combinations                               *
* Return: 0 for a successful login, -1 for failure              *
****************************************************************/
int ftpLogin(FILE *userfile)
{
    char msg[BUF_SIZE], user[BUF_SIZE], pass[BUF_SIZE];
    int i, badUser = 1;

    strcpy(msg, "220 Welcome to cs472 FTP HW3 service.\r\n");
    sendMessage(msg, strlen(msg), 1);
    //while (receiveMessage(1) == 0);
    receiveMessage(1);
    
    while (fgets(msg, BUF_SIZE-1, userfile) && (badUser == 1))
    {
        i = 0;
        while (msg[i] != ',')
        {
            i++;
        }
        strcpy(pass, "PASS ");
        strcat(pass, &msg[i+1]);
        
        pass[strlen(pass)-1] = 0;
        msg[i] = 0;
        strcpy(user, "USER ");
        strcat(user, msg);

        if (strncmp(user, RecBuf, strlen(user)) == 0)
        {
            badUser = 0;
        }
    }


    if (badUser == 1)
    {
        logMessage("Bad username.");
        strcpy(msg, "530 Login incorrect.\r\n");
        sendMessage(msg, strlen(msg), 1);
        return -1;
    }
    strcpy(msg, "331 Please specify the password.\r\n");
    sendMessage(msg, strlen(msg), 1);
    receiveMessage(1);
    if (strncmp(pass, RecBuf, strlen(pass)) != 0)
    {
        logMessage("Bad password.");
        strcpy(msg, "530 Login incorrect.\r\n");
        sendMessage(msg, strlen(msg), 1);
        return -1;
    }
    else
    {
        strcpy(msg, "230 Login successful.\r\n");
        sendMessage(msg, strlen(msg), 1);
    }
    return 0;
}


/****************************************************************
* Function: connectToPasv()                                     *
* Summary: connects to a data port using the return value from  *
*          the PASV command and sets the Datafd variable to     *
*          reference that connection                            *
* Args: char *port: the port to connect to for the PASV command *
****************************************************************/
void connectToPasv(char *port)
{
    char msg[BUF_SIZE];
    int cfd;

    strcpy(port, NextDataPort);
    if (strncmp(port, MainPort, strlen(MainPort)) == 0)
    {
        strcpy(port, "1025");
        if (strncmp(port, MainPort, strlen(MainPort)) == 0)
            strcpy(port, "1026");
    }

  

    while ((cfd = connectToClient(port)) <= 0)
    {
        if (strncmp(port, "65535", 5) == 0)
        {
            processFtpInvalid(msg);
            sendMessage(msg, strlen(msg), 1);
            return;
        }
            
        sprintf(msg, "%d", atoi(port)+1);
        if (strncmp(msg, MainPort, strlen(MainPort)) == 0)
        {
            sprintf(msg, "%d", atoi(port)+2);
        }
        strcpy(port, msg);            
    }
    sprintf(msg, "%d", atoi(port)+1);

    if (atoi(msg) <= 1025)
        strcpy(msg, "1025");
    strcpy(NextDataPort, msg);
    DataConnState = 1;
    Datafd = cfd;   
}


/****************************************************************
* Function: writeListData()                                     *
* Summary: lists the files in a directory for the LIST command  *
* Args: char *fpath: the path for the file names to list        *
* Return: 0 for successfully writing a list, -1 for failure     *
****************************************************************/
int writeListData(char *fpath)
{
    //FILE *fname;
    char data[BUF_SIZE], perms[BUF_SIZE], dtstring[BUF_SIZE];
    //char *dtstring;
    int n = 0, i, cfd;
    DIR *dir;
    struct dirent *ent;
    struct stat finfo;
    struct tm *dt;

    cfd = accept(Datafd, (struct sockaddr*)NULL, NULL);
    if ((dir = opendir(fpath)) == NULL)
    {
        if (stat(fpath, &finfo) >= 0)
        {
            dt = localtime(&finfo.st_mtime);
            strftime(dtstring, BUF_SIZE, "%b %d %H:%M", dt);
            strcpy(perms, "----------");

            if (finfo.st_mode & S_IFDIR)
                perms[0] = 'd';
            if (finfo.st_mode & S_IRUSR)
                perms[1] = 'r';
            if (finfo.st_mode & S_IWUSR)
                perms[2] = 'w';
            if (finfo.st_mode & S_IXUSR)
                perms[3] = 'x';
            if (finfo.st_mode & S_IRGRP)
                perms[4] = 'r';
            if (finfo.st_mode & S_IWGRP)
                perms[5] = 'w';
            if (finfo.st_mode & S_IXGRP)
                perms[6] = 'x';
            if (finfo.st_mode & S_IROTH)
                perms[7] = 'r';
            if (finfo.st_mode & S_IWOTH)
                perms[8] = 'w';
            if (finfo.st_mode & S_IXOTH)
                perms[9] = 'x';
            
            sprintf(data, "%s %4lu %-8d %-8d %9jd %s %s\n", perms, finfo.st_nlink, finfo.st_uid, finfo.st_gid, finfo.st_size, dtstring, ent->d_name);
            write(cfd, data, BUF_SIZE-1);
            DataConnState = 0;
            close(cfd);
            Datafd = 0;
            return 0;
        }
        close(cfd);
        DataConnState = 0;
        Datafd = 0;
        return -1;
    }
    while ((ent = readdir(dir)) != NULL)
    {
        if (stat(ent->d_name, &finfo) < 0)
            continue;
        dt = localtime(&finfo.st_mtime);
        strftime(dtstring, BUF_SIZE, "%b %d %H:%M", dt);
        strcpy(perms, "----------");

        if (finfo.st_mode & S_IFDIR)
            perms[0] = 'd';
        if (finfo.st_mode & S_IRUSR)
            perms[1] = 'r';
        if (finfo.st_mode & S_IWUSR)
            perms[2] = 'w';
        if (finfo.st_mode & S_IXUSR)
            perms[3] = 'x';
        if (finfo.st_mode & S_IRGRP)
            perms[4] = 'r';
        if (finfo.st_mode & S_IWGRP)
            perms[5] = 'w';
        if (finfo.st_mode & S_IXGRP)
            perms[6] = 'x';
        if (finfo.st_mode & S_IROTH)
            perms[7] = 'r';
        if (finfo.st_mode & S_IWOTH)
            perms[8] = 'w';
        if (finfo.st_mode & S_IXOTH)
            perms[9] = 'x';
        
        sprintf(data, "%s %4lu %-8d %-8d %9jd %s %s\n", perms, finfo.st_nlink, finfo.st_uid, finfo.st_gid, finfo.st_size, dtstring, ent->d_name);
        //printf("writing data = |%s|\n", data);
        write(cfd, data, BUF_SIZE-1);
    }
    close(cfd);
    Datafd = 0;
    DataConnState = 0;
    return 0;
}


/********************************************************************
* Function: connectToPort()                                         *
* Summary: connects to a data port on the client using the values   *
*          sent in the PORT command and sets the Datafd variable to *
* Args: char *host: the host to connect to (can be IP or name)      *
*       char *port: the port to use for the connection              *
* Return: -1 for a failure, otherwise the return value to be        *
*         used to access the server                                 *
*********************************************************************/
int connectToPort(char *host, char *port) 
{
    char msg[BUF_SIZE];
    int cfd, s;
    struct addrinfo temp;
    struct addrinfo *info;

    // set temp to all 0's
    memset(&temp, 0, sizeof(struct addrinfo));
    //set up temp struct with family and socktype
    temp.ai_family = AF_INET;
    temp.ai_socktype = SOCK_STREAM;
    temp.ai_flags = 0;

    // get address info for connecting to server
    s = getaddrinfo(host, port, &temp, &info);
    if (s != 0)
    {
        logMessage("Error, invalid addrinfo \n");
        return -1;
    }

    while(info != NULL)
    {
        // set up socket to communicate with server
        if ((cfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1)
        {
            continue;
            //logMessage("Error, invalid cfd \n");
        }

        // connect to the client and verify that connection is good
        if ((s = connect(cfd, info->ai_addr, info->ai_addrlen)) != -1)
        {
            sprintf(msg, "Connecting to port at %s\n", host);
            logMessage(msg);
            break;
        }
        else
        {
            //printf("connect fail %s\n", gai_strerror(s));
            logMessage("Error, could not connect to cfd\n");
            //close(cfd);
            return -1;
        }
        info = info->ai_next;
    }
    if (cfd == -1)
    {
        logMessage("Error, invalid cfd \n");
        return -1;
    }
    
    return cfd;
}

/****************************************************************
* Function: sendFileMessage()                                *
* Summary: sends a data message to an ftp client over the  *
*          connection described by Datafd and prints the output  *
*          to a local copy of the file, then closes the         *
*          connection described by Datafd                        *
* Args: FILE *fname: the local file to write the data to        *
****************************************************************/
void sendFileMessage(FILE *fname)
{
    int n=BUF_SIZE;
    
    // read in BUF_SIZE characters at a time
    while ((n = read(Datafd, RecBuf, BUF_SIZE-1)) != 0)
    {
        // terminate string
        if (n < BUF_SIZE)
        {
            RecBuf[n] = 0;
        }
        else
        {
            RecBuf[BUF_SIZE-1] = 0;  
        }
        // write data to file
        fputs(RecBuf, fname);
    }
    close(Datafd);
}

/****************************************************************
* Function: createFile()                                        *
* Summary: creates a file on the FTP server                     *
* Args: char *fpath: the path for the file to be created        *
* Return: the newly created file on success, stdout on failure  *
****************************************************************/
FILE *createFile(char *fpath)
{
    int i, start = 0;
    FILE *fname;
    
    // end fpath before "\r\n"
    fpath[strlen(fpath)-2] = 0;
    if (fpath[strlen(fpath)-1] != '/')
    {
        for (i = 0; i < strlen(fpath); i++)
        {
            if (fpath[i] == '/')
                start = i+1;
        }
        fname = fopen(&fpath[start], "w");
        if (fname == NULL)
        {
            logMessage("Error, bad file open: printing to stdout instead.\n");
            fname = stdout;
        }
    }
    else
    {
        fname = stdout;
    }
    return fname;
}
