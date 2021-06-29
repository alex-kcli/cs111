#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>


void signalHandler(int sig) {
    if (sig == SIGSEGV) {
        fprintf(stderr, "Captured segmentation fault\n");
        exit(4);
    }
}


int main(int argc, char * argv[])
{
    int input = 0, output = 0, segFault = 0, catch = 0;
    char *inputFile, *outputFile;
    
    /* options for option parse */
    static struct option long_options[] = {
        {"input", required_argument, NULL, 'i'},
        {"output", required_argument, NULL, 'o'},
        {"segfault", no_argument, NULL, 's'},
        {"catch", no_argument, NULL, 'c'},
        {0, 0, 0, 0}
    };
    
    int ch;
    while ((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (ch) {
            case 'i':
                input = 1;
                inputFile = optarg;
                break;
             
            case 'o':
                output = 1;
                outputFile = optarg;
                break;
            
            case 's':
                segFault = 1;
                break;
                
            case 'c':
                catch = 1;
                break;
            
            default:
                fprintf(stderr, "Wrong usage of options, accepted options are [--input filename] [--output filename] [--segfault] [--catch]\n");
                exit(1);
        }
    }
    
    if (input == 1) {
        int ifd = open(inputFile, O_RDONLY);
        if (ifd >= 0) {
            close(0);
            dup(ifd);
            close(ifd);
        }
        else {
            fprintf(stderr, "Failed to open file %s for input, %s\n", inputFile, strerror(errno));
            exit(2);
        }
    }
    
    if (output == 1) {
        int ofd = creat(outputFile, 0666);
        if (ofd >= 0) {
            close(1);
            dup(ofd);
            close(ofd);
        }
        else {
            fprintf(stderr, "Failed to create file %s for output, %s\n", outputFile, strerror(errno));
            exit(3);
        }
    }
    
    if (catch == 1) {
        signal(SIGSEGV, signalHandler);
    }
    
    if (segFault == 1) {
        char *ch = NULL;
        *ch = 'f';
    }
    
    
    /* reading into a buffer and outputing it */
    char buffer;
    for (;;) {
        ssize_t readFile = read(0, &buffer, 1);
        if (readFile <= 0)               // case end of file
            break;
        write(1, &buffer, 1);
    }
    
    exit(0);
}

