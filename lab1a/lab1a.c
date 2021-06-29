#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <getopt.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>


int fd_from_terminal[2];
int fd_to_terminal[2];
int cpid;
struct termios saved_attributes;

void signalHandler(int sig) {
    if (sig == SIGPIPE) {
        int status;
        waitpid(cpid, &status, 0);
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
    }
}


void shutdown_processing() {
    int status;
    waitpid(cpid, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
}


// Reference: https://www.gnu.org/software/libc/manual/html_node/Noncanon-Example.html
void reset_input_mode (void) {
    tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}


void set_input_mode (void)
{
    struct termios tattr;

    /* Make sure stdin is a terminal. */
    if (!isatty (STDIN_FILENO)) {
        fprintf (stderr, "Not a terminal.\n");
        exit(1);
    }

    /* Save the terminal attributes so we can restore them later. */
    tcgetattr (STDIN_FILENO, &saved_attributes);
    atexit(reset_input_mode);

    /* Set the terminal modes. */
    tcgetattr (STDIN_FILENO, &tattr);
    tattr.c_iflag = ISTRIP;
    tattr.c_oflag = 0;
    tattr.c_lflag = 0;
    
    tcsetattr (STDIN_FILENO, TCSANOW, &tattr);
}


void read_write_buffer()
{
    int eof_flag = 0;
    char buffer[16];
    
    for (;;) {
        ssize_t writeFile;
        ssize_t readFile = read(0, buffer, 16);
        
        if (readFile < 0) {                         // case error
            fprintf(stderr, "Read from buffer Failed, %s.\n", strerror(errno));
            exit(1);
        }
        
        for (int i = 0; i < readFile; i++) {
            if (*(buffer+i) == 0x0D || *(buffer+i) == 0x0A) {     // 0x0A is line feed, 0x0D is carrige return
                char cr_buffer = 0x0D;
                writeFile = write(1, &cr_buffer, 1);
                if (writeFile < 0) {
                    fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                    exit(1);
                }
                char lf_buffer = 0x0A;
                writeFile = write(1, &lf_buffer, 1);
                if (writeFile < 0) {
                    fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                    exit(1);
                }
            }
            else if (*(buffer+i) == 0x04) {      // 0x04 is eof ^D
                char eof_buf = '^';
                writeFile = write(1, &eof_buf, 1);
                if (writeFile < 0) {
                    fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                    exit(1);
                }
                eof_buf = 'D';
                writeFile = write(1, &eof_buf, 1);
                if (writeFile < 0) {
                    fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                    exit(1);
                }
                eof_flag = 1;
                break;
            }
            else {
                writeFile = write(1, (buffer+i), 1);
                if (writeFile < 0) {
                    fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                    exit(1);
                }
            }
        }
        if (eof_flag == 1)
            break;
    }
}


void read_from_terminal()
{
    char buffer[16];
    
    ssize_t writeFile;
    ssize_t readFile = read(STDIN_FILENO, buffer, 16);

    if (readFile < 0) {             // case error
        fprintf(stderr, "Read from terminal Failed, %s.\n", strerror(errno));
        exit(1);
    }

    for (int i = 0; i < readFile; i++) {
        if (*(buffer+i) == 0x0D || *(buffer+i) == 0x0A) {     // 0x0A is line feed, 0x0D is carrige return
            char cr_buffer = 0x0D;
            writeFile = write(STDOUT_FILENO, &cr_buffer, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            char lf_buffer = 0x0A;
            writeFile = write(STDOUT_FILENO, &lf_buffer, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }

            writeFile = write(fd_from_terminal[1], &lf_buffer, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to Shell Failed, %s.\n", strerror(errno));
                exit(1);
            }
        }
        else if (*(buffer+i) == 0x04) {      // 0x04 is eof ^D
            char eof_buf = '^';
            writeFile = write(STDOUT_FILENO, &eof_buf, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            eof_buf = 'D';
            writeFile = write(STDOUT_FILENO, &eof_buf, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            close(fd_from_terminal[1]);
        }
        else if (*(buffer+i) == 0x03) {      // 0x04 is control-c ^C
            char ctrlc_buf = '^';
            writeFile = write(STDOUT_FILENO, &ctrlc_buf, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            ctrlc_buf = 'C';
            writeFile = write(STDOUT_FILENO, &ctrlc_buf, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            kill(cpid, SIGINT);
        }
        else {
            writeFile = write(STDOUT_FILENO, (buffer+i), 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            writeFile = write(fd_from_terminal[1], (buffer+i), 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to Shell Failed, %s.\n", strerror(errno));
                exit(1);
            }
        }
    }
}


void read_from_shell()
{
    char buffer[256];

    ssize_t writeFile;
    ssize_t readFile = read(fd_to_terminal[0], buffer, 256);

    if (readFile < 0) {             // case error
        fprintf(stderr, "Read from shell Failed, %s.\n", strerror(errno));
        exit(1);
    }

    for (int i = 0; i < readFile; i++) {
        if (*(buffer+i) == 0x0A) {     // 0A is line feed, 0D is carrige return
            char cr_buffer = 0x0D;
            writeFile = write(STDOUT_FILENO, &cr_buffer, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            char lf_buffer = 0x0A;
            writeFile = write(STDOUT_FILENO, &lf_buffer, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
        }
        else if (*(buffer+i) == 0x04) {      // 0x04 is eof ^D
            char eof_buf = '^';
            writeFile = write(STDOUT_FILENO, &eof_buf, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            eof_buf = 'D';
            writeFile = write(STDOUT_FILENO, &eof_buf, 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
            close(fd_to_terminal[0]);
        }
        else {
            writeFile = write(STDOUT_FILENO, (buffer+i), 1);
            if (writeFile < 0) {
                fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                exit(1);
            }
        }
    }
}


// Reference: https://man7.org/linux/man-pages/man2/pipe.2.html
// Reference: Textbook
void start_fork() {
    signal(SIGPIPE, signalHandler);
    
    char *my_argv[2];
    my_argv[0] = "/bin/bash";
    my_argv[1] = NULL;
    
    if (pipe(fd_from_terminal) == -1) {
        fprintf(stderr, "Pipe from terminal failed.\n");
        exit(1);
    }
    if (pipe(fd_to_terminal) == -1) {
        fprintf(stderr, "Pipe to terminal failed.\n");
        exit(1);
    }
    
    cpid = fork();
    if (cpid == -1) {
        fprintf(stderr, "Fork failed.\n");
        exit(1);
    }
    
    if (cpid == 0) {     // child process
        close(fd_from_terminal[1]);
        close(fd_to_terminal[0]);
        
        close(0);       // stdin
        dup(fd_from_terminal[0]);
        close(fd_from_terminal[0]);
        
        close(1);       // stdout
        dup(fd_to_terminal[1]);
        
        close(2);       // stderr
        dup(fd_to_terminal[1]);
        close(fd_to_terminal[1]);
        
        execvp(my_argv[0], my_argv);
        fprintf(stderr, "Exec Shell Failed.\n");
        exit(1);
    }
    
// Reference: https://pubs.opengroup.org/onlinepubs/009695399/functions/poll.html
// Reference: http://www.unixguide.net/unix/programming/2.1.2.shtml
// Reference: Section Slides: Tianxiang
    else {              // parent process
        close(fd_from_terminal[0]);
        close(fd_to_terminal[1]);
        
        struct pollfd fds[2];
        
        fds[0].fd = STDIN_FILENO;
        fds[1].fd = fd_to_terminal[0];
        fds[0].events = POLLIN + POLLHUP + POLLERR;
        fds[1].events = POLLIN + POLLHUP + POLLERR;
        
        while (1) {
            int ret = poll(fds, 2, -1);
            if (ret < 0) {
                fprintf(stderr, "Error while polling: %s\n", strerror(errno));
                exit(1);
            }
            
            if (fds[0].revents & POLLIN) {
                read_from_terminal();
            }
            if (fds[1].revents & POLLIN) {
                read_from_shell();
            }
            if (fds[0].revents & (POLLHUP|POLLERR)) {
                fprintf(stderr, "Terminal Poll Error.\n");
                exit(1);
            }
            if (fds[1].revents & (POLLHUP|POLLERR)) {
                shutdown_processing();
                break;
            }
        }
    }
}


int main(int argc, char * argv[])
{
    int shell_flag = 0;
    
    /* options for option parse */
    static struct option long_options[] = {
        {"shell", no_argument, NULL, 's'},
        {0, 0, 0, 0}
    };
    
    int ch;
    while ((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (ch) {
            case 's':
                shell_flag = 1;
                break;
            
            default:
                fprintf(stderr, "Wrong usage of options, accepted option is [--shell]\n");
                exit(1);
        }
    }
    
    if (shell_flag == 0) {
        set_input_mode();
        read_write_buffer();
    }
    else {
        set_input_mode();
        start_fork();
    }
    
    exit(0);
}



