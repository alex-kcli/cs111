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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <zlib.h>

z_stream tosocket_strm;
z_stream toterminal_strm;

int fd_from_terminal[2];
int fd_to_terminal[2];
int cpid;


void signalHandler(int sig) {
    if (sig == SIGPIPE) {
        int status;
        waitpid(cpid, &status, 0);
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
    }
}



void shutdown_processing() {
    inflateEnd(&tosocket_strm);
    deflateEnd(&toterminal_strm);
    
    int status;
    waitpid(cpid, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
}



// Reference: Section Slide Tianxiang
int server_connect(unsigned int port_num) /* port_num: which port to listen, return socket for Subsequent communication*/
{
    int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_in my_addr; /* my address */
    struct sockaddr_in their_addr; /* connector addr */
    unsigned int sin_size;
    /* create a socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    /* set the address info */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port_num); /* short, network byte order */
    my_addr.sin_addr.s_addr = INADDR_ANY;
    /* INADDR_ANY allows clients to connect to any one of the host’s IP address. */
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero)); //padding zeros
    /* bind the socket to the IP address and port number */
    bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr));
    
    listen(sockfd, 5); /* maximum 5 pending connections */
    sin_size = sizeof(struct sockaddr_in);
    /* wait for client’s connection, their_addr stores client’s address */
    new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
    
    return new_fd; /* new_fd is returned not sock_fd*/
}



// Reference: Section Slides Tianxiang
void initialize_compression()
{
    int ret;
    /* allocate deflate state */
    toterminal_strm.zalloc = Z_NULL; // set to Z_NULL, to request zlib use the default memory allocation routines
    toterminal_strm.zfree = Z_NULL;
    toterminal_strm.opaque = Z_NULL;
    ret = deflateInit(&toterminal_strm, Z_DEFAULT_COMPRESSION); /* pointer to the structure to be initialized, and the compression level */
    if (ret != Z_OK) // make sure that it was able to allocate memory, and the provided arguments were valid.
    {
        fprintf(stderr, "DeflateInit failed. \n");
        exit(1);
    }
    
    tosocket_strm.zalloc = Z_NULL;
    tosocket_strm.zfree = Z_NULL;
    tosocket_strm.opaque = Z_NULL;
    ret = inflateInit(&tosocket_strm);
    if (ret != Z_OK)
    {
        fprintf(stderr, "InflateInit failed. \n");
        exit(1);
    }
}


int main(int argc, char * argv[])
{
    int port_flag = 0;
    int compress_flag = 0;
    
    int port_num = 0;
    int socket_fd = 0;
    
    /* options for option parse */
    static struct option long_options[] = {
        {"port", required_argument, NULL, 'p'},
        {"compress", no_argument, NULL, 'c'},
        {0, 0, 0, 0}
    };
    
    int ch;
    while ((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (ch) {
            case 'p':
                port_flag = 1;
                port_num = atoi(optarg);
                break;
                
            case 'c':
                compress_flag = 1;
                break;
                
            default:
                fprintf(stderr, "Wrong usage of options, accepted option are [--port=] [--compress]\n");
                exit(1);
        }
    }
    
    if (port_flag == 0) {
        fprintf(stderr, "Must provide the --port= option.\n");
        exit(1);
    }
    if (compress_flag == 1) {
        initialize_compression();
    }
    
    socket_fd = server_connect(port_num);
    //port number obtained from cmd args
    //Register SIGPIPE handler, Create PIPE, fork the new process, perform stdin/stdout redirection,
    
    /* start fork */
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
    else {              // parent process
        close(fd_from_terminal[0]);
        close(fd_to_terminal[1]);
            
        struct pollfd fds[2];
            
        fds[0].fd = socket_fd;
        fds[1].fd = fd_to_terminal[0];
        fds[0].events = POLLIN + POLLHUP + POLLERR;
        fds[1].events = POLLIN + POLLHUP + POLLERR;
            
        while (1) {
            int ret = poll(fds, 2, -1);
            if (ret < 0) {
                fprintf(stderr, "Error while polling: %s\n", strerror(errno));
                exit(1);
            }
            
            /* Input from the socket */
            if (fds[0].revents & POLLIN) {
                char buffer[512];
                ssize_t writeFile;
                ssize_t readFile = read(socket_fd, buffer, 512);
                
                if (readFile < 0) {             // case error
                    fprintf(stderr, "Read from socket Failed, %s.\n", strerror(errno));
                    exit(1);
                }
                
                /* Compressed transfer */
                if (compress_flag == 1) {
                    unsigned char outbuf[1024];
                    tosocket_strm.avail_in = readFile; /* number of bytes available at next_in */
                    tosocket_strm.next_in = (unsigned char *) buffer; /* next input byte*/
                    tosocket_strm.avail_out = sizeof outbuf; /* remaining free space at next_out */
                    tosocket_strm.next_out = outbuf; /* next output byte */
                    do {
                        inflate(&tosocket_strm, Z_SYNC_FLUSH); //use Z_SYNC_FLUSH for independent msgs
                    /* deflate(). It takes as many of the avail_in bytes at next_in as it can process, and writes as many as avail_out bytes to next_out.
                    deflate will update next_in, avail_in, next_out, avail_out accordingly.*/
                    } while (tosocket_strm.avail_in > 0);
                    
                    int size_after_decompress = 1024 - tosocket_strm.avail_out;
                    for (int i = 0; i < size_after_decompress; i++) {
                        if (*(outbuf+i) == 0x0A || *(outbuf+i) == 0x0D) {     // 0A is line feed, 0D is carrige return
                            char lf_buffer = 0x0A;
                            writeFile = write(fd_from_terminal[1], &lf_buffer, 1);
                            if (writeFile < 0) {
                                fprintf(stderr, "Decompressed write to Shell Failed, %s.\n", strerror(errno));
                                exit(1);
                            }
                        }
                        else if (*(outbuf+i) == 0x04) {      // 0x04 is eof ^D
                            close(fd_from_terminal[1]);
                        }
                        else if (*(outbuf+i) == 0x03) {      // 0x04 is control-c ^C
                            kill(cpid, SIGINT);
                        }
                        else {
                            writeFile = write(fd_from_terminal[1], (outbuf+i), 1);
                            if (writeFile < 0) {
                                fprintf(stderr, "Decompressed write to Shell Failed, %s.\n", strerror(errno));
                                exit(1);
                            }
                        }
                    }
                }
                
                /* Uncompressed Transfer */
                else {
                    for (int i = 0; i < readFile; i++) {
                        if (*(buffer+i) == 0x0A || *(buffer+i) == 0x0D) {     // 0A is line feed, 0D is carrige return
                            char lf_buffer = 0x0A;
                            writeFile = write(fd_from_terminal[1], &lf_buffer, 1);
                            if (writeFile < 0) {
                                fprintf(stderr, "Decompressed write to Shell Failed, %s.\n", strerror(errno));
                                exit(1);
                            }
                        }
                        else if (*(buffer+i) == 0x04) {      // 0x04 is eof ^D
                            close(fd_from_terminal[1]);
                        }
                        else if (*(buffer+i) == 0x03) {      // 0x04 is control-c ^C
                            kill(cpid, SIGINT);
                        }
                        else {
                            writeFile = write(fd_from_terminal[1], (buffer+i), 1);
                            if (writeFile < 0) {
                                fprintf(stderr, "Decompressed write to Shell Failed, %s.\n", strerror(errno));
                                exit(1);
                            }
                        }
                    }
                }
            }
            
            
            /* Input from Shell */
            if (fds[1].revents & POLLIN) {
                char buffer[512];
                ssize_t writeFile;
                ssize_t readFile = read(fd_to_terminal[0], buffer, 512);
                
                if (readFile < 0) {             // case error
                    fprintf(stderr, "Read from Shell Failed, %s.\n", strerror(errno));
                    exit(1);
                }
                
                /* Compressed Transfer */
                if (compress_flag == 1) {
                    unsigned char outbuf[512];
                    toterminal_strm.avail_in = readFile; /* number of bytes available at next_in */
                    toterminal_strm.next_in = (unsigned char *) buffer; /* next input byte*/
                    toterminal_strm.avail_out = sizeof outbuf; /* remaining free space at next_out */
                    toterminal_strm.next_out = outbuf; /* next output byte */
                    do {
                        deflate(&toterminal_strm, Z_SYNC_FLUSH); //use Z_SYNC_FLUSH for independent msgs
                        /* deflate(). It takes as many of the avail_in bytes at next_in as it can process, and writes as many as avail_out bytes to next_out.
                        deflate will update next_in, avail_in, next_out, avail_out accordingly.*/
                    } while (toterminal_strm.avail_in > 0);
                                
                    int size_after_compress = 512 - toterminal_strm.avail_out;
                    writeFile = write(socket_fd, outbuf, size_after_compress);
                    if (writeFile < 0) {
                        fprintf(stderr, "Compressed write to socket Failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                    for (int i = 0; i < readFile; i++) {
                        if (*(buffer+i) == 0x04) {
                            close(fd_to_terminal[0]);
                        }
                    }
                }
                
                /* Uncompressed Transfer */
                else {
                    for (int i = 0; i < readFile; i++) {
                        if (*(buffer+i) == 0x04) {
                            writeFile = write(socket_fd, (buffer+i), 1);
                            if (writeFile < 0) {
                                fprintf(stderr, "Write to socket Failed, %s.\n", strerror(errno));
                                exit(1);
                            }
                            close(fd_to_terminal[0]);
                        }
                        else {
                            writeFile = write(socket_fd, (buffer+i), 1);
                            if (writeFile < 0) {
                                fprintf(stderr, "Write to socket Failed, %s.\n", strerror(errno));
                                exit(1);
                            }
                        }
                    }
                }
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
    
    inflateEnd(&tosocket_strm);
    deflateEnd(&toterminal_strm);
    
    exit(0);
}
