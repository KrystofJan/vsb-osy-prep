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
#include <semaphore.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <thread>
#include <vector>
#include <string>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"

//***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

// debug flag
int g_debug = LOG_INFO;
sem_t sem_one, sem_two, sem_three;
std::vector<std::thread> allClients;


void handleClient1(int socket);
void handleClient2(int socket);
void handleClient3(int socket);
float handleInput(int num1, int num2, char op);
int count = 1;


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

int main( int t_narg, char **t_args )
{
    if ( t_narg <= 1 ) help( t_narg, t_args );

    int l_port = 0;

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
    }

    sem_init(&sem_one, 1, 0);
    sem_init(&sem_two, 1, 0);
    sem_init(&sem_three, 1, 0);

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
        // handle the client shit here
        switch(count){
            case 1: {
                allClients.push_back(std::thread(handleClient1, l_sock_client));
                ++count;
                break;
            }case 2: {
                allClients.push_back(std::thread(handleClient2, l_sock_client));
                ++count;
                break;
            }case 3: {
                allClients.push_back(std::thread(handleClient3, l_sock_client));
                ++count;
                break;
            }
        }
                
    }
    for(std::thread &t : allClients){
        t.join();
    }
    return 0;
}


float handleInput(int num1, int num2, char op)
{
    switch(op){
        case '+': return (float) (num1 + num2);
        case '-': return (float) (num1 - num2);
        case '*': return (float) (num1 * num2);
        default: return (float) num1 / num2;
    }
}


void handleClient1(int socket)
{
    char buf[128];
    int num1 = 0, num2 = 0;
    char znamenko = '/';
    int counter = 0;

    while(1){
        if(counter > 3) {
            

            sem_post(&sem_two);
            sem_wait(&sem_one);
            log_msg(LOG_INFO, "Reached the limit");
            log_msg(LOG_INFO, "%d", sem_one);

            counter = 0;
        }
        int len = read( socket, buf, sizeof( buf ) );
        if ( len < 0 )
            log_msg( LOG_ERROR, "Unable to read from stdin." );

        buf[len] = 0;

        sscanf(buf, "%d%c%d", &num1, &znamenko, &num2);

        char result[32];
        float f_res = handleInput(num1, num2, znamenko);
        sprintf(result, "%f\n", f_res);

        write(STDOUT_FILENO, result, strlen(result) );
        ++counter;
    }
}

void handleClient2(int socket)
{
    char buf[128];
    int num1 = 0, num2 = 0;
    char znamenko = '/';
    int counter = 0;
log_msg(LOG_INFO, "Client  2\n");
    while(1){
        if(counter > 3) {
            sem_post(&sem_three);
            sem_wait(&sem_two);
        }
        int len = read( socket, buf, sizeof( buf ) );
        if ( len < 0 )
            log_msg( LOG_ERROR, "Unable to read from stdin." );

        buf[len] = 0;

        sscanf(buf, "%d%c%d", &num1, &znamenko, &num2);

        char result[32];
        float f_res = handleInput(num1, num2, znamenko);
        sprintf(result, "%f\n", f_res);

        write(STDOUT_FILENO, result, strlen(result) );
        ++counter;
    }


}

void handleClient3(int socket)
{
    char buf[128];
    int num1 = 0, num2 = 0;
    char znamenko = '/';
log_msg(LOG_INFO, "Client 3\n");


    int counter = 0;
    while(1){
        if(counter > 3) {
            sem_post(&sem_one);
            sem_wait(&sem_three);
        }
        int len = read( socket, buf, sizeof( buf ) );
        if ( len < 0 )
            log_msg( LOG_ERROR, "Unable to read from stdin." );

        buf[len] = 0;

        sscanf(buf, "%d%c%d", &num1, &znamenko, &num2);

        char result[32];
        float f_res = handleInput(num1, num2, znamenko);
        sprintf(result, "%f\n", f_res);

        write(STDOUT_FILENO, result, strlen(result) );
        ++counter;
    }


}

