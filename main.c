
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sysexits.h>
#include <pty.h>

pid_t child = 0;

void sighandler(int signum)
{
    if(child > 0)
    {
        killpg(child, signum);
        exit(signum);
    }
}

static ssize_t readfromto(int fdfrom, int fdto, char* buf, unsigned int buf_size, bool* shouldstop, bool* wasfail)
{
    ssize_t br;
    *shouldstop = false;
    *wasfail = false;
    br = read(fdfrom, buf, buf_size);
    if(br <= 0)
    {
        *shouldstop = true;
        return br;
    }
    if(write(fdto, buf, br) != br)
    {
        *wasfail = true;
        *shouldstop = true;
    }
    return br;
}

/*
* base body derived from: https://stackoverflow.com/a/40121343/4150977
* added code to also handle stdin, stderr
*/
int main(int argc, char *argv[])
{
    const int buf_size = (1024 * 8);
    int status;
    int master;
    int fromfd;
    bool shouldstop;
    bool wasfailure;
    char buf[buf_size];
    fd_set fds;
    ssize_t bytes_read;
    if (argc < 2)
    {
        return EX_USAGE;
    }
    child = forkpty(&master, NULL, NULL, NULL);
    if (child == -1)
    {
        perror("failed to fork pty");
        return EX_OSERR;
    }
    if (child == 0)
    {
        /* we're in the child process, so replace it with the command */
        execvp(argv[1], argv + 1);
        perror("failed to execute command");
        return EX_OSERR;
    }
    else
    {
        /* back to parent */
        status = 0;
        if (waitpid(child, &status, 0) == -1)
        {
            /* handle error... however that would look like... */
        }
        /*fprintf(stderr, "back at parent, exit status was %d\n", status);*/
        if(status != 0)
        {
            status = 1;
        }
        exit(status);
    }
    /* trap kill signals and forward them to child process */
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    /* forward input and output continuously */
    while(true)
    {
        FD_ZERO(&fds);
        FD_SET(master, &fds);
        FD_SET(STDIN_FILENO, &fds);
        /* FD_SET(STDERR_FILENO, &fds); */
        if(select(master + 1, &fds, NULL, NULL, NULL) > 0)
        {
            if(FD_ISSET(master, &fds))
            {
                shouldstop = false;
                wasfailure = false;
                readfromto(master, STDOUT_FILENO, buf, buf_size, &shouldstop, &wasfailure);
                if(shouldstop)
                {
                    if(wasfailure)
                    {
                        perror("failed to write to stdout");
                        return EX_OSERR;
                    }
                    return EXIT_SUCCESS;
                }
            }
            else if(FD_ISSET(STDIN_FILENO, &fds))
            {
                readfromto(STDIN_FILENO, master, buf, buf_size, &shouldstop, &wasfailure);
            }
            /*else
            {
                fprintf(stderr, "**unexpected event**\n");
            }*/
        }
    }
    return 0;
}
