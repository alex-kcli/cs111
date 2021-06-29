#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <getopt.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>


#ifdef DUMMY
#define MRAA_GPIO_IN 0
typedef int mraa_aio_context;
typedef int mraa_gpio_context;
mraa_aio_context mraa_aio_init(int p) { return 0; }
int mraa_aio_read(mraa_aio_context c) { return 650; }
void mraa_aio_close(mraa_aio_context c) {}
mraa_gpio_context mraa_gpio_init(int p){ return 0; }
void mraa_gpio_dir(mraa_gpio_context c, int d) {}
int mraa_gpio_read(mraa_gpio_context c) { return 0; }
void mraa_gpio_close(mraa_gpio_context c) {}
#else
#include <mraa.h>
#include <mraa/aio.h>
#endif


#define TEMP_PIN 1
#define BUTTON_PIN 60
#define B 4275 // thermistor value
#define R0 100000.0 // nominal base value


char temp_scale;
int period, log_flag;
char *log_file;
int stop = 0;
int ofd = 0;
mraa_aio_context temp;
mraa_gpio_context button;


// From Section Slides
float convert_temper_reading(int reading, char scale)
{
    float R = 1023.0/((float) reading) - 1.0;
    R = R0*R;
    //C is the temperature in Celcius
    float Celcius = 1.0/(log(R/R0)/B + 1/298.15) - 273.15; //F is the temperature in Fahrenheit
    float Fahrenheit = (Celcius * 9)/5 + 32;
    if (scale == 'C')
        return Celcius;
    else
        return Fahrenheit;
}


void print_temp_report(struct timespec ts) {
//    struct timespec ts;
    struct tm *tm;
//    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&(ts.tv_sec));
    if (tm == NULL) {
        fprintf(stderr, "Localtime Failed.\n");
        exit(1);
    }
    
    int reading = mraa_aio_read(temp);
    float temperature = convert_temper_reading(reading, temp_scale);
    
    int fret = fprintf(stdout, "%02d:%02d:%02d %.1f\n",
                       tm->tm_hour, tm->tm_min, tm->tm_sec, temperature);
    if (fret < 0) {
        fprintf(stderr, "Write to stdout failed.\n");
        exit(1);
    }
    
    if (log_flag == 1) {
        int dret = dprintf(ofd, "%02d:%02d:%02d %.1f\n",
                           tm->tm_hour, tm->tm_min, tm->tm_sec, temperature);
        if (dret < 0) {
            fprintf(stderr, "Write to log file failed.\n");
            exit(1);
        }
    }
}


void do_when_pushed() {
    struct timespec ts;
    struct tm *tm;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&(ts.tv_sec));
    
    fprintf(stdout, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
    
    if (log_flag == 1) {
        dprintf(ofd, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
    }
    
    exit(0);
}


void process_command(char *newline_buf) {
    if (newline_buf[0] == ' ' || newline_buf[0] == '\t') {
        while (newline_buf[0] == ' ' || newline_buf[0] == '\t') {
            newline_buf++;
        }
    }
    
    if (strcmp(newline_buf, "SCALE=F") == 0) {
        temp_scale = 'F';
        if (log_flag == 1)
            dprintf(ofd, "SCALE=F\n");
    }
    else if (strcmp(newline_buf, "SCALE=C") == 0) {
        temp_scale = 'C';
        if (log_flag == 1)
            dprintf(ofd, "SCALE=C\n");
    }
    else if (strcmp(newline_buf, "STOP") == 0) {
        stop = 1;
        if (log_flag == 1)
            dprintf(ofd, "STOP\n");
    }
    else if (strcmp(newline_buf, "START") == 0) {
        stop = 0;
        if (log_flag == 1)
            dprintf(ofd, "START\n");
    }
    else if (strcmp(newline_buf, "OFF") == 0) {
        if (log_flag == 1)
            dprintf(ofd, "OFF\n");
        do_when_pushed();
    }
    else if (strncmp(newline_buf, "PERIOD=", 7) == 0) {
        period = atoi(newline_buf + 7);
        if (log_flag == 1) {
            int i = 0;
            while (newline_buf[i] != '\0') {
                dprintf(ofd, "%c", newline_buf[i]);
                i++;
            }
            dprintf(ofd, "\n");
        }
    }
    else if(strncmp(newline_buf, "LOG", 3) == 0) {
        if (log_flag == 1) {
            int i = 0;
            while (newline_buf[i] != '\0') {
                dprintf(ofd, "%c", newline_buf[i]);
                i++;
            }
            dprintf(ofd, "\n");
        }
    }
    else {
        if (log_flag == 1) {
            int i = 0;
            while (newline_buf[i] != '\0') {
                dprintf(ofd, "%c", newline_buf[i]);
                i++;
            }
            dprintf(ofd, "\n");
        }
    }
}


int main(int argc, char * argv[])
{
    period = 1;
    log_flag = 0;
    temp_scale = 'F';
    
    /* options for option parse */
    static struct option long_options[] = {
        {"scale", required_argument, NULL, 's'},
        {"period", required_argument, NULL, 'p'},
        {"log", required_argument, NULL, 'l'},
        {0, 0, 0, 0}
    };
    
    int ch;
    while ((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (ch) {
            case 's':
                if (strcmp(optarg, "C") == 0)
                    temp_scale = 'C';
                else if (strcmp(optarg, "F") == 0)
                    temp_scale = 'F';
                else {
                    fprintf(stderr, "Wrong usage of --scale, accepted option is [--scale=F] or [--scale=C]\n");
                    exit(1);
                }
                break;
            
            case 'p':
                period = atoi(optarg);
                break;
            
            case 'l':
                log_flag = 1;
                log_file = optarg;
                break;
            
            default:
                fprintf(stderr, "Wrong usage of options, accepted option is [--scale=t] [--period=s] [--log=filename]\n");
                exit(1);
        }
    }
    
    
    if (log_flag == 1) {
        ofd = creat(log_file, 0666);
        if (ofd < 0) {
            fprintf(stderr, "Failed to create file %s for output, %s.\n", log_file, strerror(errno));
            exit(1);
        }
    }
    
    temp = mraa_aio_init(TEMP_PIN);
    if (temp == NULL) {
        fprintf(stderr, "Failed to initialize temperature sensor.\n");
        exit(1);
    }

    button = mraa_gpio_init(BUTTON_PIN);
    if (button == NULL){
        fprintf(stderr, "Failed to initialize button.\n");
        exit(1);
    }
    
    mraa_gpio_dir(button, MRAA_GPIO_IN);
    mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &do_when_pushed, NULL);
    
    struct pollfd pollStdin = {0, POLLIN, 0};
    
    time_t temp_report_time = 0;
    while(1) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        
        if (stop == 0 && ts.tv_sec >= temp_report_time) {
            print_temp_report(ts);
            temp_report_time = ts.tv_sec + period;
        }
        
        int ret = poll(&pollStdin, 1, 1000);
        if (ret < 0) {
            fprintf(stderr, "Failed to poll.\n");
        }
        else if (ret > 0) {
            char buffer[128];
            char newline_buf[128];
            ssize_t readFile = read(0, buffer, 128);
            if (readFile < 0) {
                fprintf(stderr, "Read from standard input Failed, %s.\n", strerror(errno));
                exit(1);
            }
            int i = 0;
            int j = 0;
            for (i = 0; i < readFile; i++) {
                if (buffer[i] == '\n') {
                    newline_buf[j] = '\0';
                    process_command(newline_buf);
                    j = 0;
                }
                else {
                    newline_buf[j] = buffer[i];
                    j++;
                }
            }
        }
    }
    
    mraa_aio_close(temp);
    mraa_gpio_close(button);
    
    exit(0);
}



