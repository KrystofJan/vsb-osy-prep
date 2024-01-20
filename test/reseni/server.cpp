//***************************************************************************
//
// Program example for labs in subject Operating Systems
//
// Petr Olivka, Dept. of Computer Science, petr.olivka@vsb.cz, 2017
//
// Example of socket server.
//
// This program is example of socket server and it allows to connect and serve
// the only one client.
// The mandatory argument of program is port number for listening.
//
//***************************************************************************

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <sys/wait.h>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"
#define WAIT_TIME   10
#define OUTPUT_PATH "out.png"
#define SEM_NAME "/my_sem"

//***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

// debug flag

sem_t *my_sem = nullptr;

int g_debug = LOG_INFO;

void log_msg( int t_log_level, const char *t_form, ... )
{
    const char *out_fmt[] = {
            "ERR: (%d-%s) %s\n",
            "INF: %s\n",
            "DEB: %s\n" };

    if ( t_log_level && t_log_level > g_debug ) return;

    char l_buf[ 1024 ];
    va_list l_arg;
    va_start( l_arg, t_form );
    vsprintf( l_buf, t_form, l_arg );
    va_end( l_arg );

    switch ( t_log_level )
    {
    case LOG_INFO:
    case LOG_DEBUG:
        fprintf( stdout, out_fmt[ t_log_level ], l_buf );
        break;

    case LOG_ERROR:
        fprintf( stderr, out_fmt[ t_log_level ], errno, strerror( errno ), l_buf );
        break;
    }
}

//***************************************************************************
// help

void handleClient(int socket);
void handleConvert(int socket, std::vector<char*> args);
std::string handleInput(int* hours, int *minutes);


void sendData(int socket);

void help( int t_narg, char **t_args )
{
    if ( t_narg <= 1 || !strcmp( t_args[ 1 ], "-h" ) )
    {
        printf(
            "\n"
            "  Socket server example.\n"
            "\n"
            "  Use: %s [-h -d] port_number\n"
            "\n"
            "    -d  debug mode \n"
            "    -h  this help\n"
            "\n", t_args[ 0 ] );

        exit( 0 );
    }

    if ( !strcmp( t_args[ 1 ], "-d" ) )
        g_debug = LOG_DEBUG;
}

//***************************************************************************
void handleClient(int socket);

int main( int t_narg, char **t_args )
{

    if ( t_narg <= 1 ) help( t_narg, t_args );

    int l_port = 0;


    my_sem = sem_open( SEM_NAME, O_RDWR );

    if(!my_sem){
        my_sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, 0660, 1);
        if ( !my_sem)
        {
            log_msg( LOG_ERROR, "Unable to create two semaphores!" );
            return 1;
        }
    }

    // parsing arguments
    for ( int i = 1; i < t_narg; i++ )
    {
        if ( !strcmp( t_args[ i ], "-d" ) )
            g_debug = LOG_DEBUG;

        if ( !strcmp( t_args[ i ], "-h" ) )
            help( t_narg, t_args );

        if ( *t_args[ i ] != '-' && !l_port )
        {
            l_port = atoi( t_args[ i ] );
            break;
        }
    
        if ( !strcmp( t_args[ i ], "-r" ) )
        {
            log_msg( LOG_INFO, "Clean semaphores." );
            sem_unlink( SEM_NAME );
            exit( 0 );
        }
    }

    if ( l_port <= 0 )
    {
        log_msg( LOG_INFO, "Bad or missing port number %d!", l_port );
        help( t_narg, t_args );
    }

    log_msg( LOG_INFO, "Server will listen on port: %d.", l_port );

    // socket creation
    int l_sock_listen = socket( AF_INET, SOCK_STREAM, 0 );
    if ( l_sock_listen == -1 )
    {
        log_msg( LOG_ERROR, "Unable to create socket.");
        exit( 1 );
    }

    in_addr l_addr_any = { INADDR_ANY };
    sockaddr_in l_srv_addr;
    l_srv_addr.sin_family = AF_INET;
    l_srv_addr.sin_port = htons( l_port );
    l_srv_addr.sin_addr = l_addr_any;

    // Enable the port number reusing
    int l_opt = 1;
    if ( setsockopt( l_sock_listen, SOL_SOCKET, SO_REUSEADDR, &l_opt, sizeof( l_opt ) ) < 0 )
      log_msg( LOG_ERROR, "Unable to set socket option!" );

    // assign port number to socket
    if ( bind( l_sock_listen, (const sockaddr * ) &l_srv_addr, sizeof( l_srv_addr ) ) < 0 )
    {
        log_msg( LOG_ERROR, "Bind failed!" );
        close( l_sock_listen );
        exit( 1 );
    }

    // listenig on set port
    if ( listen( l_sock_listen, 1 ) < 0 )
    {
        log_msg( LOG_ERROR, "Unable to listen on given port!" );
        close( l_sock_listen );
        exit( 1 );
    }

    log_msg( LOG_INFO, "Enter 'quit' to quit server." );

    // go!
    while ( 1 )
    {
        int l_sock_client = -1;

        // list of fd sources
        pollfd l_read_poll[ 2 ];

        l_read_poll[ 0 ].fd = STDIN_FILENO;
        l_read_poll[ 0 ].events = POLLIN;
        l_read_poll[ 1 ].fd = l_sock_listen;
        l_read_poll[ 1 ].events = POLLIN;

        while ( 1 ) // wait for new client
        {
            // select from fds
            int l_poll = poll( l_read_poll, 2, -1 );

            if ( l_poll < 0 )
            {
                log_msg( LOG_ERROR, "Function poll failed!" );
                exit( 1 );
            }

            if ( l_read_poll[ 0 ].revents & POLLIN )
            { // data on stdin
                char buf[ 128 ];
                int len = read( STDIN_FILENO, buf, sizeof( buf) );
                if ( len < 0 )
                {
                    log_msg( LOG_DEBUG, "Unable to read from stdin!" );
                    exit( 1 );
                }

                log_msg( LOG_DEBUG, "Read %d bytes from stdin" );
                // request to quit?
                if ( !strncmp( buf, STR_QUIT, strlen( STR_QUIT ) ) )
                {
                    log_msg( LOG_INFO, "Request to 'quit' entered.");
                    close( l_sock_listen );
                    exit( 0 );
                }
            }

            if ( l_read_poll[ 1 ].revents & POLLIN )
            { // new client?
                sockaddr_in l_rsa;
                int l_rsa_size = sizeof( l_rsa );
                // new connection
                l_sock_client = accept( l_sock_listen, ( sockaddr * ) &l_rsa, ( socklen_t * ) &l_rsa_size );
                if ( l_sock_client == -1 )
                {
                    log_msg( LOG_ERROR, "Unable to accept new client." );
                    close( l_sock_listen );
                    exit( 1 );
                }
                uint l_lsa = sizeof( l_srv_addr );
                // my IP
                getsockname( l_sock_client, ( sockaddr * ) &l_srv_addr, &l_lsa );
                log_msg( LOG_INFO, "My IP: '%s'  port: %d",
                                 inet_ntoa( l_srv_addr.sin_addr ), ntohs( l_srv_addr.sin_port ) );
                // client IP
                getpeername( l_sock_client, ( sockaddr * ) &l_srv_addr, &l_lsa );
                log_msg( LOG_INFO, "Client IP: '%s'  port: %d",
                                 inet_ntoa( l_srv_addr.sin_addr ), ntohs( l_srv_addr.sin_port ) );

                break;
            }

        } // while wait for client

        // change source from sock_listen to sock_client
        l_read_poll[ 1 ].fd = l_sock_client;

        if(fork() == 0){
            handleClient(l_sock_client);
        }
    } // while ( 1 )

    return 0;
}


void handleClient(int socket){
    while(1){
        char buf[64];
        int len = read(socket, buf, sizeof(buf));
        if (len < 0){
            log_msg(LOG_ERROR, "Nelze cist z klienta!");
        }

        int hour, minute;
        int width = -1, height = -1;

        sscanf(buf, "%d:%d %d %d", &hour, &minute, &width, &height);

        char hour_buf[4];
        int desc = ((hour * 100) + ((minute/10) * 10))%1200;

        char arg_buf[64];
        if (width > 0 && height > 0){
            sprintf(arg_buf, "convert img/ring.png img/%04d.png img/minute%d.png -layers flatten -resize %dx%d! -colorspace Gray out.png", desc, minute, width, height);
        }
        else{
            sprintf(arg_buf, "convert img/ring.png img/hour%04d.png img/minute%d.png -layers flatten -colorspace Gray out.png", desc, minute);
        }

        log_msg(LOG_INFO, arg_buf);

        std::vector<char*> args;
        char* token = strtok(arg_buf, " ");
        
        while(token != nullptr){
            args.push_back(token);
            token = strtok(NULL, " ");
        }     

        sem_wait(my_sem);

        if (fork() == 0){
            handleConvert(socket, args);
            exit(1);
        }

        wait(NULL);

        sendData(socket);

        log_msg(LOG_INFO, "post");
        sem_post(my_sem);


        if (fork() == 0){
            log_msg(LOG_INFO, "Disp the picture\n");

            execl("display", "display", "-update" , "1" , "out.png", "&", nullptr);
            // exit(2);
        }
        wait(NULL);

    }
}

void handleConvert(int socket, std::vector<char*> args){
    char **argv;
    argv = args.data();
    int arg_len = args.size();
    argv[arg_len] = nullptr;

    log_msg(LOG_INFO, "Made the picture");
    
    int convert_len = execvp(argv[0], argv);
    if ( convert_len < 0 ){
        log_msg( LOG_ERROR, "Exec failed" );
        exit(0);
    }
}

std::string handleInput(int hours, int minutes){
    int desc = (minutes/10) * 10;
    // sprintf()
    return "hey";
}

void sendData(int socket){
        
        int secs = 0;


        int out_fd = open("out.png", O_RDONLY);
        struct stat fileStat;
        if(stat("out.png", &fileStat) == -1){
            log_msg(LOG_ERROR, "SOUBOR NENI");
            exit(2);
        }
        int file_size = fileStat.st_size + 1;
        
        char bytes[file_size];
        int file_len = read(out_fd, bytes, sizeof(bytes));
        if ( file_len < 0 ){
            log_msg( LOG_ERROR, "FILE OPEN FAILED" );
            exit(0);
        }
        bytes[file_size] = 0;
        write(socket, bytes, strlen(bytes));

        // log_msg(LOG_INFO, "File len%d\n", file_len);    
        // log_msg(LOG_INFO, bytes);

//         int davka_len = (int)((float)file_len / 10);
//         int index = 0;

//         log_msg(LOG_INFO, "davka: %d", davka_len);    

    
//         while(secs < WAIT_TIME){
//             char davka[davka_len];
//             for(int i = 0; i < davka_len; i++){
//                 davka[i] = bytes[index + i];
//             }
//             davka[davka_len] = 0;
//             log_msg(LOG_INFO, davka);

//             write(socket, davka, strlen(davka));

//             index+=davka_len;

//             log_msg(LOG_INFO, "%d%\n", (secs+1) * 10);
//             secs++;
//             sleep(1);
//         }
}