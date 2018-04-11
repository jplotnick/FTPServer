/*ftpCommands.h - declares functions for building ftp commands*/

/*************************************************************
* FTP message functions:                                     *
* Summary: Each function builds a string for an FTP command  *
*          and sets a given char* to that string             *
* Args: char *msg: the char* that is pointed to the command  *
*                  string to be used by the calling function *
*************************************************************/
void processFtpCwd(char *msg);
void processFtpCdup(char *msg);
void processFtpQuit(char *msg);
void processFtpInvalid(char *msg);
void processFtpPasv(char *msg);
void processFtpEpsv(char *msg);
void processFtpPort(char *msg);
void processFtpEprt(char *msg);
void processFtpRetr(char *msg);
void processFtpPwd(char *msg);
void processFtpList(char *msg);
void processFtpHelp(char *msg);