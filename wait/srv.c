#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#define BUFLEN 1024
#define LEN (BUFLEN - sizeof(off_t) - sizeof(mode_t))
#define DATA_SIZE 1024
#define PACK_SIZE (DATA_SIZE + sizeof(int))
#define PORT 4323
#define CONTROL_PORT 1150
#define GROUP_ADDR "229.1.1.0"
#define IP "10.55.125.214"
#define PATH "./uploads/"
#define TRUE 1
#define FALSE 0
#define WAITING_TIME 10
#define BLOCKSIZE 1024
#define OCTET "octet"
#define TFTP_DATA_SIZE 4

int sock;
int control_sock;
struct in_addr localInterface;
struct sockaddr_in groupSock;
char *src;

enum PROTOCOL {
  DEFAULT,
  TFTP
};

enum REQUEST {
  RRQ = 0x1,
  WRQ,
  DATA,
  ACK,
  ERR,
  ERR2
};

enum ERROR_CODES {
  NONE = 0x0,
  FILE_NOT_FOUND = 0x1,
  PERMISSION_DENIED,
  DISK_ERROR,
  UNCORRECT_OPERATION,
  UNCORRECT_ID,
  FILE_EXISTS,
  USER_NOT_EXIST,
  UNCORRECT__OPTION
};

typedef struct {
    char filename[LEN];
    off_t st_size;
    mode_t st_mode;
} Default_protocol_meta;

typedef struct {
    int num;
    char data[DATA_SIZE];
} Default_protocol_packet;

void default_protocol(Default_protocol_meta *data) {
  Default_protocol_packet databuf, control_databuf;
  char metabuf[BUFLEN];
  int datalen, control_datalen;
  int i;
  int packet_num = 0;

  memset(metabuf, '\0', BUFLEN);
  sprintf(metabuf, "%d %d %s \n", data->st_size, data->st_mode, data->filename);
  printf("%s\n", metabuf);
  if(sendto(sock, &metabuf, BUFLEN, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
    perror("Sending datagram message error");
  } else {
    printf("Sending datagram message...OK\n");
  }
  pid_t pid = fork();
  if (pid == 0) {
      // lost packets

      struct sockaddr_in addr, addr_s;
      int size = sizeof(addr_s);
      char cbuf[BUFLEN];
      int lost_packet_num = 0;

      control_sock = socket(AF_INET, SOCK_DGRAM, 0);
      if(control_sock < 0) {
        perror("Opening control datagram socket error");
        exit(1);
      } else {
        printf("Opening the control datagram socket...OK.\n");
      }

      memset((char *) &addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(CONTROL_PORT);
      addr.sin_addr.s_addr = htonl(INADDR_ANY);

      if (bind(control_sock, (struct sockaddr *)&addr, sizeof(addr))==-1) {
        perror("Can't bind control socket");
        goto CHILD_CLEAN_UP;
      }



      while(1) {
        if (recvfrom(control_sock, cbuf, BUFLEN, 0, (struct sockaddr *)&addr_s, &size)==-1) {
          perror("Can't recvfrom");
          goto CHILD_CLEAN_UP;
        } else {
          sscanf(cbuf, "%d", &lost_packet_num);
          memset(&control_databuf, '\0', PACK_SIZE);
          control_datalen = ((lost_packet_num - 1)*DATA_SIZE + DATA_SIZE >= data->st_size)? (data->st_size - (lost_packet_num - 1)):DATA_SIZE;
          control_databuf.num = lost_packet_num;
          memcpy(control_databuf.data, &src[i], control_datalen);
          printf("%s\n", control_databuf);
          if(sendto(control_sock, &control_databuf, PACK_SIZE, 0, (struct sockaddr*)&addr_s, sizeof(addr_s)) < 0) {
            perror("Sending datagram message error");
          } else {
            printf("Sending datagram message...OK\n");
          }
       }
     }
     //printf("Bye!\n");
     close(control_sock);
     exit(0);
CHILD_CLEAN_UP:
      close(control_sock);
      exit(1);
  }
  for (i = 0; i < data->st_size; i += DATA_SIZE) {
    packet_num++;
    memset(&databuf, '\0', PACK_SIZE);
    datalen = (i + DATA_SIZE >= data->st_size)? (data->st_size - i):DATA_SIZE;
    databuf.num = packet_num;
    memcpy(databuf.data, &src[i], datalen);
    //printf("%s\n", databuf);
    if ((packet_num == 5)||(packet_num == 3)) {
        continue;
    }
    printf("Packet number: %d\n", databuf.num);
    if(sendto(sock, &databuf, PACK_SIZE, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
      perror("Sending datagram message error");
    } else {
      printf("Sending datagram message...OK\n");
    }
  }

  sleep(WAITING_TIME);
  close(control_sock);
  kill(pid, SIGTERM);
}


typedef struct __attribute__((packed)) {
    uint16_t type;
    char filename[BUFLEN];
    char mode[BUFLEN];
} RQ;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t block;
    char data[BLOCKSIZE];
} _DATA;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t code;
    char msg[BUFLEN];
} ERROR;


void send_error(uint16_t code, char *msg) {
  ERROR err;
  memset(err.msg, '\0', BUFLEN);
  strcpy(err.msg, msg);
  err.type = ERR;
  err.code = code;
  if(sendto(sock, &err, TFTP_DATA_SIZE, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
    perror("Sending datagram message error");
  } else {
    printf("Sending datagram message...OK\n");
  }

  if(sendto(sock, err.msg, strlen(err.msg), 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
    perror("Sending datagram message error");
  } else {
    printf("Sending datagram message...OK\n");
  }

}

void tftp_protocol(void) {
  RQ request;
  _DATA databuf;
  //char block[BLOCKSIZE];
  int datalen;
  int i;
  int size;
  char c = '\0';
  int file;
  struct stat statbuf;
  char filename[BUFLEN];
  int block_num = 0;

  memset(request.filename, '\0', BUFLEN);
  memset(request.mode, '\0', BUFLEN);
  size = sizeof(groupSock);
  if(recvfrom(sock, &request.type, sizeof(request.type), 0, (struct sockaddr *)&groupSock, &size) < 0) {
    perror("Reading datagram message error");
    goto TFTP_CLEAN_UP1;
  }

  if (request.type != RRQ) {
    // uncorrect
    send_error(UNCORRECT_OPERATION, "Unknown request\n");
    goto TFTP_CLEAN_UP1;
  }

    if(recvfrom(sock, request.filename, BUFLEN,  MSG_WAITALL, (struct sockaddr *)&groupSock, &size) < 0) {
      perror("Reading datagram message error");
      goto TFTP_CLEAN_UP1;
    }

  //printf("%d %s %s\n", request.type, request.filename, request.mode);

    if(recvfrom(sock, request.mode, BUFLEN, MSG_WAITALL, (struct sockaddr *)&groupSock, &size) < 0) {
      perror("Reading datagram message error");
      goto TFTP_CLEAN_UP1;
    }

  //printf("%d %s %s\n", request.type, request.filename, request.mode);
  if (strcmp(request.mode, OCTET)) {
    // error

    send_error(UNCORRECT_OPERATION, "Unknown mode\n");
    goto TFTP_CLEAN_UP1;
  }
  printf("%d %s %s\n", request.type, request.filename, request.mode);
  strcpy(filename, PATH);
  strcat(filename, request.filename);


  if ((file = open(filename, O_RDONLY)) == -1) {
    perror("Cannot open output file\n");
    send_error(FILE_NOT_FOUND, "Cannot open output file\n");
    goto TFTP_CLEAN_UP1;
  } else {
    printf("%s opened\n", request.filename);
  }

  if (fstat(file, &statbuf) < 0) {
    perror("Fstat failed\n");
    goto TFTP_CLEAN_UP2;
  }

  if ((src = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file, 0)) == MAP_FAILED) {
      perror("Mapping failed\n");
      send_error(NONE, "Mapping failed\n");
      goto TFTP_CLEAN_UP2;
  }
  databuf.type = DATA;

  for (i = 0; i < statbuf.st_size; i += BUFLEN) {
    block_num++;
    memset(&databuf.data, '\0', BLOCKSIZE);
    datalen = (i + BLOCKSIZE >= statbuf.st_size)? (statbuf.st_size - i):BLOCKSIZE;
    databuf.block = block_num;
    printf("block num %d\n", databuf.block);
    printf("len %d\n", datalen);
    if(sendto(sock, &databuf, TFTP_DATA_SIZE, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
      perror("Sending datagram message error");
    } else {
      printf("Sending datagram message...OK\n");
    }
    memcpy(databuf.data, &src[i], datalen);
    if(sendto(sock, databuf.data, datalen, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
      perror("Sending datagram message error");
    } else {
      printf("Sending datagram message...OK\n");
    }
  }
  if (datalen == BLOCKSIZE) {
    memset(&databuf.data, '\0', sizeof(_DATA));
    databuf.block = block_num;
    if(sendto(sock, &databuf, TFTP_DATA_SIZE, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0) {
      perror("Sending datagram message error");
    } else {
      printf("Sending datagram message...OK\n");
    }
  }

  munmap(src, statbuf.st_size);
  close(file);
  return;
TFTP_CLEAN_UP2:
  close(file);
TFTP_CLEAN_UP1:
  close(sock);
  exit(1);
}

void print_usage(char *name) {
    printf("Usage: %s [options] [filename]\nOptions:\n-p\t\tport\n-BT\t\tbroadcast transfer\n-TFTP\t\tuse TFTP protocol\n-h\t\tprint this message\n", name);
}

int main (int argc, char *argv[ ])
{
  char filename[BUFLEN];
  int opt_num = 1;
  int file;
  struct stat statbuf;
  int opt;
  int port = PORT;
  int broadcast = FALSE;
  int protocol = DEFAULT;

 while ((opt = getopt(argc, argv, "p:B:T:h")) != -1) {
     switch (opt) {
         case 'p':
            if (optarg) {
              port = atoi(optarg);
              opt_num++;
            } else {
              print_usage(argv[0]);
              exit(1);
            }
             break;
         case 'B':
             broadcast = TRUE;
             break;
         case 'T':
             protocol = TFTP;
             break;
         default:
             print_usage(argv[0]);
             return 0;
     }
     opt_num++;
 }

 if ((protocol == DEFAULT)&&(opt_num >= argc)) {
   print_usage(argv[0]);
   exit(1);
 }

 printf("State: %d %d %d\n", port, broadcast, protocol);


  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock < 0) {
    perror("Opening datagram socket error");
    exit(1);
  } else {
    printf("Opening the datagram socket...OK.\n");
  }


  if (protocol == DEFAULT) {
    strcpy(filename, PATH);
    strcat(filename, argv[opt_num]);
    if ((file = open(filename, O_RDONLY)) == -1) {
      perror("Cannot open output file\n");
      goto CLEAN_UP;
    } else {
      printf("%s opened\n", argv[opt_num]);
    }

    if (fstat(file, &statbuf) < 0) {
      perror("Fstat failed\n");
      goto DEFAULT_CLEAN_UP1;
    }

    if ((src = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, file, 0)) == MAP_FAILED) {
        perror("Mapping failed\n");
        goto DEFAULT_CLEAN_UP1;
    }

    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;
    groupSock.sin_port = htons(port);
    groupSock.sin_addr.s_addr = inet_addr(GROUP_ADDR);
    localInterface.s_addr = inet_addr(IP);

    if (broadcast == FALSE) {
      if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0) {
        perror("Setting local interface error");
        goto DEFAULT_CLEAN_UP2;
      } else {
        printf("Setting the local interface...OK\n");
      }
    } else {
      groupSock.sin_addr.s_addr = htonl(INADDR_BROADCAST);

      if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&localInterface, sizeof(localInterface)) < 0) {
         perror("setsockopt (SO_BROADCAST)");
         goto DEFAULT_CLEAN_UP2;
       }  else {
         printf("Setting SO_BROADCAST...OK\n");
       }
    }

    Default_protocol_meta data;

    memset(data.filename, '\0', LEN);
    strcpy(data.filename, argv[opt_num]);
    data.st_size = statbuf.st_size;
    data.st_mode = statbuf.st_mode;

    default_protocol(&data);

    munmap(src, statbuf.st_size);
    close(file);

    printf("File %s sended successfully by %s\n", argv[opt_num], (broadcast == FALSE)? "multicast":"broadcast");
  } else if (protocol == TFTP) {

    memset((char *) &groupSock, 0, sizeof(groupSock));
    groupSock.sin_family = AF_INET;

    groupSock.sin_port = htons(port);

    groupSock.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&groupSock, sizeof(groupSock))==-1) {
      perror("Can't bind control socket");
      goto CLEAN_UP;
    }

    printf("TFTP protocol\n");
    tftp_protocol();
  }

  close(sock);
  return 0;
DEFAULT_CLEAN_UP2:
  munmap(src, statbuf.st_size);
DEFAULT_CLEAN_UP1:
  close(file);
CLEAN_UP:
  close(sock);
  return 1;
}
