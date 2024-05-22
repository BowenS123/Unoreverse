#ifdef _WIN32
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <stdio.h>    //for fprintf, perror
#include <stdlib.h>   //for exit
#include <string.h>   //for memset
#include <unistd.h>   //for close
#include <winsock2.h> //for all socket programming
#include <ws2tcpip.h> //for getaddrinfo, inet_pton, inet_ntop
#include <pthread.h>
void OSInit(void) {
  WSADATA wsaData;
  int WSAError = WSAStartup(MAKEWORD(2, 0), &wsaData);
  if (WSAError != 0) {
    fprintf(stderr, "WSAStartup errno = %d\n", WSAError);
    exit(-1);
  }
}
void OSCleanup(void) { WSACleanup(); }
#define perror(string)                                                         \
  fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())
#else
#include <arpa/inet.h>  //for htons, htonl, inet_pton, inet_ntop
#include <errno.h>      //for errno
#include <netdb.h>      //for getaddrinfo
#include <netinet/in.h> //for sockaddr_in
#include <stdio.h>      //for fprintf, perror
#include <stdlib.h>     //for exit
#include <string.h>     //for memset
#include <sys/socket.h> //for sockaddr, socket, socket
#include <sys/types.h>  //for size_t
#include <unistd.h>     //for close
void OSInit(void) {}
void OSCleanup(void) {}
#endif

int initialization();
int connection(int internet_socket);
void execution(int internet_socket);
void cleanup(int internet_socket, int client_internet_socket);

int main(int argc, char *argv[]) {
  printf("start TCP server!\n");
  OSInit();
  int internet_socket = initialization();

  while (1) {
    int client_internet_socket = connection(internet_socket);
    execution(client_internet_socket);
  }

  // cleanup( internet_socket, client_internet_socket );
  // OSCleanup();
  // printf("End TCP server!");

  return 0;
}

int initialization() {
  // Get address information
  struct addrinfo internet_address_setup;
  struct addrinfo *internet_address_result;
  memset(&internet_address_setup, 0, sizeof internet_address_setup);
  internet_address_setup.ai_family = AF_UNSPEC;
  internet_address_setup.ai_socktype = SOCK_STREAM;
  internet_address_setup.ai_flags = AI_PASSIVE;
  int getaddrinfo_return = getaddrinfo(NULL, "22", &internet_address_setup,
                                       &internet_address_result);
  if (getaddrinfo_return != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_return));
    exit(1);
  }

  int internet_socket = -1;
  struct addrinfo *internet_address_result_iterator = internet_address_result;
  while (internet_address_result_iterator != NULL) {
    // Create socket
    internet_socket = socket(internet_address_result_iterator->ai_family,
                             internet_address_result_iterator->ai_socktype,
                             internet_address_result_iterator->ai_protocol);
    if (internet_socket == -1) {
      perror("socket");
    } else {
      // Bind socket to an address
      int bind_return =
          bind(internet_socket, internet_address_result_iterator->ai_addr,
               internet_address_result_iterator->ai_addrlen);
      if (bind_return == -1) {
        perror("bind");
        close(internet_socket);
      } else {
        // Listen for incoming connections
        int listen_return = listen(internet_socket, SOMAXCONN);
        if (listen_return == -1) {
          close(internet_socket);
          perror("listen");
        } else {
          break;
        }
      }
    }
    internet_address_result_iterator =
        internet_address_result_iterator->ai_next;
  }

  freeaddrinfo(internet_address_result);

  if (internet_socket == -1) {
    fprintf(stderr, "socket: no valid socket address found\n");
    exit(2);
  }

  return internet_socket;
}

char ip_address[INET6_ADDRSTRLEN];

int connection(int internet_socket) {
  // Handle new connections
  struct sockaddr_storage client_internet_address;
  socklen_t client_internet_address_length = sizeof(client_internet_address);
  int client_socket =
      accept(internet_socket, (struct sockaddr *)&client_internet_address,
             &client_internet_address_length);
  if (client_socket == -1) {
    perror("accept");
    close(internet_socket);
    exit(3);
  }
  // IP4 or IP6
  void *addr;
  if (client_internet_address.ss_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)&client_internet_address;
    addr = &(s->sin_addr);
  } else {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_internet_address;
    addr = &(s->sin6_addr);
  }

  char ip_address[INET6_ADDRSTRLEN];
  inet_ntop(client_internet_address.ss_family, addr, ip_address,
            sizeof(ip_address));

  // Logging client IP
  FILE *log_file = fopen("CLientlog.txt", "a");
  if (log_file == NULL) {
    perror("fopen");
    close(client_socket);
    exit(4);
  }

  fprintf(log_file, "Connection from %s\n", ip_address);
  fclose(log_file);

  return client_socket;
}

void http_get() {
  int sockfd;
  struct sockaddr_in server_addr;
  char request[256];
  char response[2048];

  // new connection
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    return;
  }

  // Set up server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(80);
  server_addr.sin_addr.s_addr = inet_addr("208.95.112.1");

  // Connect to the server
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("connect");
    return;
  }

  // HTTP request with client IP
  snprintf(request, sizeof(request),
           "GET /json/%s HTTP/1.0\r\nHost: ip-api.com\r\n\r\n", ip_address);

  // Send the HTTP request
  if (send(sockfd, request, strlen(request), 0) == -1) {
    perror("send");
    return;
  }

  // Logging client location
  FILE *file = fopen("CLientlog.txt", "a");
  if (file == NULL) {
    perror("fopen");
    return;
  }

  // Receive and log the HTTP response
  while (1) {
    ssize_t bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
    if (bytes_received == -1) {
      perror("recv");
      break;
    } else if (bytes_received == 0) {
      break;
    }

    response[bytes_received] = '\0';

    // Parsing json
    char *status = strstr(response, "\"status\":\"");
    char *country = strstr(response, "\"country\":\"");
    char *regionName = strstr(response, "\"regionName\":\"");
    char *city = strstr(response, "\"city\":\"");
    char *isp = strstr(response, "\"isp\":\"");
    char *org = strstr(response, "\"org\":\"");

    if (status && country && regionName && city && isp && org) {
      fprintf(file, "Status:%.*s\n", (int)(strstr(status, "\",") - status - 10),
              status + 10);
      fprintf(file, "Country:%.*s\n",
              (int)(strstr(country, "\",") - country - 11), country + 11);
      fprintf(file, "Region Name:%.*s\n",
              (int)(strstr(regionName, "\",") - regionName - 14),
              regionName + 14);
      fprintf(file, "City:%.*s\n", (int)(strstr(city, "\",") - city - 8),
              city + 8);
      fprintf(file, "ISP:%.*s\n", (int)(strstr(isp, "\",") - isp - 7), isp + 7);
      fprintf(file, "Organization:%.*s\n", (int)(strstr(org, "\",") - org - 7),
              org + 7);
    } else {
      fprintf(stderr, "Error: Failed to parse response\n");
    }
  }

  fclose(file);
  close(sockfd);
}

int total_bytes_sent = 0;

void *BOMB(void *arg) {
  int client_internet_socket = *(int *)arg;
  const char *Explosion =
      "\nYOU GOT NUKED!\n"
      "                               ________________\n"
      "                          ____/ (  (    )   )  \\___\n"
      "                         /( (  (  )   _    ))  )   )\\\n"
      "                       ((     (   )(    )  )   (   )  )\n"
      "                     ((/  ( _(   )   (   _) ) (  () )  )\n"
      "                    ( (  ( (_)   ((    (   )  .((_ ) .  )_\n"
      "                   ( (  )    (      (  )    )   ) . ) (   )\n"
      "                  (  (   (  (   ) (  _  ( _) ).  ) . ) ) ( )\n"
      "                  ( (  (   ) (  )   (  ))     ) _)(   )  )  )\n"
      "                 ( (  ( \\ ) (    (_  ( ) ( )  )   ) )  )) ( )\n"
      "                  (  (   (  (   (_ ( ) ( _    )  ) (  )  )   )\n"
      "                 ( (  ( (  (  )     (_  )  ) )  _)   ) _( ( )\n"
      "                  ((  (   )(    (     _    )   _) _(_ (  (_ )\n"
      "                   (_((__(_(__(( ( ( |  ) ) ) )_))__))_)___)\n"
      "                   ((__)        \\\\||lll|l||///          \\_))\n"
      "                            (   /(/ (  )  ) )\\   )\n"
      "                          (    ( ( ( | | ) ) )\\   )\n"
      "                           (   /(| / ( )) ) ) )) )\n"
      "                         (     ( ((((_(|)_)))))     )\n"
      "                          (      ||\\(|(|)|/||     )\n"
      "                        (        |(||(||)||||        )\n"
      "                          (     //|/l|||)|\\\\ \\     )\n"
      "                        (/ / //  /|//||||\\\\  \\ \\  \\ _)\n"
      "------------------------------------------------------------------------"
      "-------";
  printf("\nStarted Bombing\n");
  while (1) {
    int bytes_sent =
        send(client_internet_socket, Explosion, strlen(Explosion), 0);
    if (bytes_sent == -1) {
      perror("send");
      break;
    }
    usleep(200000);
    total_bytes_sent += bytes_sent;
  }
  printf("\nFinished Bombing\n");

  return NULL;
}

void execution(int client_internet_socket) {
  printf("\nExecution Start!\n");
  http_get();
  char buffer[1000];

  // New thread for bomb
  pthread_t send_thread;
  pthread_create(&send_thread, NULL, BOMB, &client_internet_socket);

  while (1) {
    int number_of_bytes_received =
        recv(client_internet_socket, buffer, sizeof(buffer) - 1, 0);
    if (number_of_bytes_received == -1) {
      perror("recv");
      break;
    } else if (number_of_bytes_received == 0) {
      printf("Client disconnected.\n");
      break;
    }
    buffer[number_of_bytes_received] = '\0';
    printf("Received: %s\n", buffer);
  }

  pthread_join(send_thread, NULL);

  FILE *log_file = fopen("CLientlog.txt", "a");
  if (log_file == NULL) {
    perror("fopen");
    close(client_internet_socket);
    exit(4);
  }
  fprintf(log_file, "Total messages delivered: %d\n", total_bytes_sent);
  fclose(log_file);
  printf("Total messages delivered: %d\n", total_bytes_sent);

  close(client_internet_socket);
}

/*void cleanup( int internet_socket, int client_internet_socket )
{

  int shutdown_return = shutdown( client_internet_socket, SD_RECEIVE );
  if( shutdown_return == -1 )
  {
    perror( "shutdown" );
  }
  close( client_internet_socket );
  close( internet_socket );
}*/