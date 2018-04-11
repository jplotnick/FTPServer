/*ftpCommands.h - declares functions for building ftp commands*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "ftpResponses.h"

/*Global Defines*/
#define BUF_SIZE ((int)1024)


/*************************************************************
* FTP message functions:                                     *
* Summary: Each function builds a string for an FTP command  *
*          and sets a given char* to that string             *
* Args: char *msg: the char* that is pointed to the command  *
*                  string to be used by the calling function *
*************************************************************/
void processFtpCwd(char *msg)
{
    // remove trailing \r\n
    msg[strlen(msg)-2] = 0;
    if (msg[0] == 0)
    {
        strcpy(msg, "550 Failed to change directory.\r\n");
        return;
    }
    else 
        strcpy(msg, &msg[1]);

    if (chdir(msg) == 0)
    {
        strcpy(msg, "250 Directory successfully changed.\r\n");
    } 
    else
    {
        /* could not open directory */
        strcpy(msg, "550 Failed to change directory.\r\n");
    }


}

void processFtpCdup(char *msg)
{
    if (chdir("..") == 0)
    {
        strcpy(msg, "250 Directory successfully changed.\r\n");
    } 
    else
    {
        // Error opening directory
        strcpy(msg, "550 Failed to change directory.\r\n");
    }
}

void processFtpQuit(char *msg)
{
    strcpy(msg, "221 Goodbye.\r\n");
}

void processFtpInvalid(char *msg)
{
    //char 
    strcpy(msg, "500 Unrecognized command.\r\n");
}
void processFtpPasv(char *msg)
{
    char temp[BUF_SIZE];
    strcpy(temp, "227 Entering passive mode. ");
    strcat(temp, msg);
    strcat(temp, "\r\n");
    strcpy(msg, temp);
}

void processFtpEpsv(char *msg)
{
    strcpy(msg, "200 Testing response.\r\n");

}

void processFtpPort(char *msg)
{

    strcpy(msg, "200 Testing response.\r\n");

}

void processFtpEprt(char *msg)
{
    strcpy(msg, "200 Testing response.\r\n");

}

void processFtpRetr(char *msg)
{
    strcpy(msg, "200 Testing response.\r\n");

}

void processFtpPwd(char *msg)
{
    char path[BUF_SIZE];
    getcwd(path, BUF_SIZE);
    sprintf(msg, "257 \"%s\" is the current directory.\r\n", path);
}

void processFtpList(char *msg)
{

    msg[strlen(msg)-2] = 0;

    if (msg[0] == 0)
        strcpy(msg, ".");
    else
        strcpy(msg, &msg[1]);

    if (opendir (msg) != NULL)
    {
        strcpy(msg, "150 About to open data connection.\r\n");
    } 
    else
    {
        /* could not open directory */
        strcpy(msg, "550 Failed to list directory.\r\n");
    }
}

void processFtpHelp(char *msg)
{
    strcpy(msg, "214-The following commands are recognized.\nCDUP CWD EPRT EPSV HELP LIST PASV PORT PWD QUIT RETR\n214 Help OK.\r\n");
}

