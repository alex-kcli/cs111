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
#include <netdb.h>

#include <fcntl.h>
#include <zlib.h>

z_stream tosocket_strm;
z_stream toterminal_strm;

struct termios saved_attributes;

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



// Reference: Section Slides Tianxiang
/* include all the headers */
int client_connect(unsigned int port)
{
    /* e.g. host_name:”google.com”, port:80, return the socket for subsequent communication */
    int sockfd;
    struct sockaddr_in serv_addr; /* server addr and port info */
    struct hostent* server;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons (port);
    server = gethostbyname("localhost"); /* convert host_name to IP addr */
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    /* copy ip address from server to serv_addr */
    memset(serv_addr.sin_zero, '\0', sizeof serv_addr.sin_zero); /* padding zeros*/
    
    int retcon = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if (retcon < 0) {
        fprintf(stderr, "Failed to make connection to server. \n");
        exit(1);
    }
    /* initiate connection to the server*/
    
    return sockfd;
}



// Reference: Section Slides Tianxiang
void initialize_compression()
{
    int ret;
    /* allocate deflate state */
    tosocket_strm.zalloc = Z_NULL; // set to Z_NULL, to request zlib use the default memory allocation routines
    tosocket_strm.zfree = Z_NULL;
    tosocket_strm.opaque = Z_NULL;
    ret = deflateInit(&tosocket_strm, Z_DEFAULT_COMPRESSION); /* pointer to the structure to be initialized, and the compression level */
    if (ret != Z_OK) // make sure that it was able to allocate memory, and the provided arguments were valid.
    {
        fprintf(stderr, "DeflateInit failed. \n");
        exit(1);
    }
    
    toterminal_strm.zalloc = Z_NULL;
    toterminal_strm.zfree = Z_NULL;
    toterminal_strm.opaque = Z_NULL;
    ret = inflateInit(&toterminal_strm);
    if (ret != Z_OK)
    {
        fprintf(stderr, "InflateInit failed. \n");
        exit(1);
    }
}



int main(int argc, char * argv[])
{
    int port_flag = 0;
    int log_flag = 0;
    char *log_file;
    int compress_flag = 0;
    
    int port_num = 0;
    int socket_fd = 0;
    int ofd = 0;
    
    /* options for option parse */
    static struct option long_options[] = {
        {"port", required_argument, NULL, 'p'},
        {"log", required_argument, NULL, 'l'},
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
            
            case 'l':
                log_flag = 1;
                log_file = optarg;
                break;
                
            case 'c':
                compress_flag = 1;
                break;
                
            default:
                fprintf(stderr, "Wrong usage of options, accepted option are [--port=] [--log=] [--compress]\n");
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
    if (log_flag == 1) {
        ofd = creat(log_file, 0666);
        if (ofd < 0) {
            fprintf(stderr, "Failed to create file %s for output, %s\n", log_file, strerror(errno));
            exit(1);
        }
    }
    
    socket_fd = client_connect(port_num);
    set_input_mode();
    
    struct pollfd fds[2];
    
    fds[0].fd = STDIN_FILENO;
    fds[1].fd = socket_fd;
    fds[0].events = POLLIN + POLLHUP + POLLERR;
    fds[1].events = POLLIN + POLLHUP + POLLERR;
    
    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            fprintf(stderr, "Error while polling: %s\n", strerror(errno));
            exit(1);
        }
        if (fds[0].revents & POLLIN) {
            char buffer[512];
            ssize_t writeFile;
            ssize_t readFile = read(STDIN_FILENO, buffer, 512);
            
            if (readFile < 0) {             // case error
                fprintf(stderr, "Read from STDIN Failed, %s.\n", strerror(errno));
                exit(1);
            }
            
            /* Echo to stdout */
            for (int i = 0; i < readFile; i++) {
                if (*(buffer+i) == 0x0A || *(buffer+i) == 0x0D) {     // 0A is line feed, 0D is carrige return
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
                else {
                    writeFile = write(STDOUT_FILENO, (buffer+i), 1);
                    if (writeFile < 0) {
                        fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                }
            }
            
            /* Pass to socket */
            /* Compressed Transfer */
            if (compress_flag == 1) {
                unsigned char outbuf[512];
                tosocket_strm.avail_in = readFile; /* number of bytes available at next_in */
                tosocket_strm.next_in = (unsigned char *) buffer; /* next input byte*/
                tosocket_strm.avail_out = sizeof outbuf; /* remaining free space at next_out */
                tosocket_strm.next_out = outbuf; /* next output byte */
                do {
                    deflate(&tosocket_strm, Z_SYNC_FLUSH); //use Z_SYNC_FLUSH for independent msgs
                /* deflate(). It takes as many of the avail_in bytes at next_in as it can process, and writes as many as avail_out bytes to next_out.
                deflate will update next_in, avail_in, next_out, avail_out accordingly.*/
                } while (tosocket_strm.avail_in > 0);
                
                int size_after_compress = 512 - tosocket_strm.avail_out;
                writeFile = write(socket_fd, outbuf, size_after_compress);
                if (writeFile < 0) {
                    fprintf(stderr, "Compressed write to socket Failed, %s.\n", strerror(errno));
                    exit(1);
                }
                
                if (log_flag == 1) {
                    int m_size = writeFile;
                    int dpret = dprintf(ofd, "SENT %d bytes: ", m_size);
                    if (dpret < 0) {
                        fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                    
                    writeFile = write(ofd, outbuf, m_size);
                    if (writeFile < 0) {
                        fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                    
                    char m_arr_newline[1] = "\n";
                    writeFile = write(ofd, m_arr_newline, 1);
                    if (writeFile < 0) {
                        fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                    
                }
            }
            
            /* Uncompressed Transfer */
            else {
                writeFile = write(socket_fd, buffer, readFile);
                if (writeFile < 0) {
                    fprintf(stderr, "No compress write to socket Failed, %s.\n", strerror(errno));
                    exit(1);
                }
                
                if (log_flag == 1) {
                    int m_size = writeFile;
                    int dpret = dprintf(ofd, "SENT %d bytes: ", m_size);
                    if (dpret < 0) {
                        fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                    
                    writeFile = write(ofd, buffer, m_size);
                    if (writeFile < 0) {
                        fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                    
                    char m_arr_newline[1] = "\n";
                    writeFile = write(ofd, m_arr_newline, 1);
                    if (writeFile < 0) {
                        fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                        exit(1);
                    }
                }
            }
        }
        
        
        /* Input from the socket */
        if (fds[1].revents & POLLIN) {
            char buffer[512];
            ssize_t writeFile;
            ssize_t readFile = read(socket_fd, buffer, 512);
            
            if (readFile < 0) {             // case error
                fprintf(stderr, "Read from socket Failed, %s.\n", strerror(errno));
                exit(1);
            }
            if (readFile == 0)
                break;
            
            if (log_flag == 1) {
                int m_size = readFile;
                int dpret = dprintf(ofd, "RECEIVED %d bytes: ", m_size);
                if (dpret < 0) {
                    fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                    exit(1);
                }
                
                writeFile = write(ofd, buffer, m_size);
                if (writeFile < 0) {
                    fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                    exit(1);
                }
                
                char m_arr_newline[1] = "\n";
                writeFile = write(ofd, m_arr_newline, 1);
                if (writeFile < 0) {
                    fprintf(stderr, "Write to log failed, %s.\n", strerror(errno));
                    exit(1);
                }
            }
            
            if (compress_flag == 1) {
                unsigned char outbuf[1024];
                toterminal_strm.avail_in = readFile; /* number of bytes available at next_in */
                toterminal_strm.next_in = (unsigned char *) buffer; /* next input byte*/
                toterminal_strm.avail_out = sizeof outbuf; /* remaining free space at next_out */
                toterminal_strm.next_out = outbuf; /* next output byte */
                do {
                    inflate(&toterminal_strm, Z_SYNC_FLUSH); //use Z_SYNC_FLUSH for independent msgs
                /* deflate(). It takes as many of the avail_in bytes at next_in as it can process, and writes as many as avail_out bytes to next_out.
                deflate will update next_in, avail_in, next_out, avail_out accordingly.*/
                } while (toterminal_strm.avail_in > 0);
                
                int size_after_decompress = 1024 - toterminal_strm.avail_out;
                
                for (int i = 0; i < size_after_decompress; i++) {
                    if (*(outbuf+i) == 0x0A) {     // 0A is line feed, 0D is carrige return
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
                    else {
                        writeFile = write(STDOUT_FILENO, (outbuf+i), 1);
                        if (writeFile < 0) {
                            fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
                            exit(1);
                        }
                    }
                }
            }
            else {
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
                    else {
                        writeFile = write(STDOUT_FILENO, (buffer+i), 1);
                        if (writeFile < 0) {
                            fprintf(stderr, "Write to stdout Failed, %s.\n", strerror(errno));
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
            break;
        }
    }
    
    deflateEnd(&tosocket_strm);
    inflateEnd(&toterminal_strm);
    
    exit(0);
}


