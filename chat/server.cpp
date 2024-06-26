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
#include <thread>
#include <vector>
#include <sys/wait.h>
#include <semaphore.h>

#define STR_CLOSE   "close"
#define STR_QUIT    "quit"
#define MAX_USER    3

//***************************************************************************
// log messages

#define LOG_ERROR               0       // errors
#define LOG_INFO                1       // information and notifications
#define LOG_DEBUG               2       // debug messages

// debug flag
int g_debug = LOG_INFO;

struct Client {
    int id;
    char* ip;
    int socket;
    char name[256];
    std::thread thread;
};

int user_amount = -1;


std::vector<std::thread> allThreads;
Client allClients[MAX_USER];

sem_t my_sem;


void handle(int user_id);

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

    sem_init(&my_sem, 1, 3);

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
    std::thread t;

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
            }

            if ( l_read_poll[ 1 ].revents & POLLIN)
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
        sem_wait(&my_sem);
        user_amount++;

        allClients[user_amount].ip = inet_ntoa( l_srv_addr.sin_addr );
        allClients[user_amount].socket = l_sock_client;
        allClients[user_amount].id = user_amount;

        log_msg(LOG_INFO, "eshklere");
        log_msg(LOG_INFO, "eshklere");
        t = std::thread(handle, user_amount);
        t.join();

    } // while ( 1 )

    // for(std::thread &t : allThreads){
    //     t.join();
    // }
    t.join();

    return 0;
}

void handle(int user_id){
    char name_buf[64];
    int name_len = read(allClients[user_id].socket, name_buf, sizeof(name_buf));

    if(name_len < 0){
        log_msg(LOG_ERROR, "COULD NOT READ FROM USER!");
        exit(EXIT_FAILURE);
    }
    name_buf[name_len] = 0;

    sscanf(allClients[user_id].name, "%s", &name_buf);

    char join_msg[256];
    sprintf(join_msg, "%s just joined in!\n\0", name_buf);
    
    for(int i = 0; i < user_amount; i++){
        write(allClients[i].socket, join_msg, strlen(join_msg));
    }
    
    while(true){
        char input_buf[256];
        int input_len = read(allClients[user_id].socket, input_buf, sizeof(input_buf));
        if(input_len < 0){
            log_msg(LOG_ERROR, "COULD NOT READ FROM USER!");
            exit(EXIT_FAILURE);
        }

        // request to quit?
        if ( !strncmp( input_buf, STR_QUIT, strlen( STR_QUIT ) ) )
        {
            log_msg( LOG_INFO, "Request to 'quit' entered.");
            int closing_sock = allClients[user_amount].socket;
            for (int i = user_id; i < user_amount; i++){
                allClients[user_amount].ip = allClients[user_amount + 1].ip;
                allClients[user_amount].socket = allClients[user_amount+1].socket;
                allClients[user_amount].id = allClients[user_amount+1].id;
                sprintf(allClients[user_amount].name, "%s", allClients[user_amount+1].name);
            }
            user_amount--;
            close( closing_sock );
            log_msg(LOG_INFO, "eshklere");
            sem_post(&my_sem);
            exit( 0 );
        }

        char msg[256];
        sprintf(msg, "%s: %s\n\0", allClients[user_id].name, input_buf);

        for(int i = 0; i < user_amount; i++){
            if (i != user_id)
                write(allClients[i].socket, msg, strlen(msg));
        }

        
    }
}