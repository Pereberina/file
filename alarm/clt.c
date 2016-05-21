//
//  clt.c
//
//  Created on April 2016
//  Copyright © 2016 com.mipt. All rights reserved.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define BUFLEN 1024 // size of transferred metadata buffer
#define LEN (BUFLEN - sizeof(off_t) - sizeof(mode_t))
#define DATA_SIZE 1024  // size of transferred data buffer
#define PACK_SIZE (DATA_SIZE + sizeof(int))
#define PORT 4322
#define CONTROL_PORT 5312 // port for the lost packets to be resent
#define GROUP_ADDR "229.1.1.0" // address of the multicast group
#define IP "192.168.1.62" // address of the current client machine
#define SRV_IP "192.168.1.1" // address of the server
#define PATH "./to/"  // to save files
#define TRUE 1
#define FALSE 0
#define WINDOW 3 // the window to check whether there are lost packets (each 3 packets received)
#define OCTET "octet"
#define BLOCKSIZE 1024 // size of TFTP data
#define TFTP_DATA_SIZE 4

int sock;
struct sockaddr_in localSock;
struct ip_mreq group;


void print_usage(char *name) {
    printf("Usage: %s [options]\nOptions:\n-p\t\tport\n-BT\t\tbroadcast transfer\n-T\t\tuse TFTP protocol\n-h\t\tprint this message\n", name);
}



enum PROTOCOL {
  DEFAULT,
  TFTP
};


// DEFAULT PROTOCOL

typedef struct {
    char filename[LEN];
    off_t st_size;
    mode_t st_mode;
} Default_protocol_meta;

typedef struct {
    int num;
    char data[DATA_SIZE];
} Default_protocol_packet;



void default_protocol(void) {
  int datalen, size;
  Default_protocol_packet databuf;
  char metabuf[BUFLEN];
  Default_protocol_meta data;
  memset(data.filename, '\0', LEN);
  data.st_size = 0;
  data.st_mode = 0;
  char filepath[LEN + sizeof(PATH)];
  int curr_size = 0;
  int file;
  char *dst;
  size = sizeof(group);
  int *packets;
  int pack_num = 0;
  int max_pack = 0;
  struct sockaddr_in addr_s;
  int control_socket, i, slen=sizeof(addr_s);
  char control_buf[BUFLEN];
  if ((control_socket=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
    perror("Control socket error");
    goto DEFAULT_CLEAN_UP;
  }


  memset((char *) &addr_s, 0, sizeof(addr_s));
  addr_s.sin_family = AF_INET;
  addr_s.sin_port = htons(CONTROL_PORT);
  if (inet_aton(SRV_IP, &addr_s.sin_addr)==0) {
    perror("inet_aton() failed");
    goto DEFAULT_CLEAN_UP;
  }


  if(recvfrom(sock, metabuf, BUFLEN, 0, (struct sockaddr *)&group, &size) < 0) {
    perror("Reading datagram message error");
    goto DEFAULT_CLEAN_UP1;
  }

  sscanf(metabuf, "%d %d %s\n",  &data.st_size, &data.st_mode, data.filename);
  printf("%s %d %d\n", data.filename, data.st_size, data.st_mode);
  strcpy(filepath, PATH);
  strcat(filepath, data.filename);
  printf("%s\n", filepath);
  if  ((file = open(data.filename, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, data.st_mode)) < 0 ) {
       printf("Can't create output file\n");
       goto DEFAULT_CLEAN_UP1;
  }

  if (ftruncate(file, data.st_size) < 0) {
     printf("ftruncate failed\n");
     goto DEFAULT_CLEAN_UP2;
  }

   if ((dst = mmap(NULL, data.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0)) == MAP_FAILED) {
     printf("Mapping failed\n");
     goto DEFAULT_CLEAN_UP2;
   }

  char *ptr;
  ptr = dst;
  pack_num = data.st_size / DATA_SIZE + 1;
  packets = (int *)malloc(pack_num * sizeof(int));
  memset(packets, 0, pack_num * sizeof(int));

  memset(control_buf, '\0', BUFLEN);
  sprintf(control_buf, "%d", -1);
  char sig_ack = 0;
  if (sendto(control_socket, control_buf, BUFLEN, 0, (struct sockaddr *)&addr_s, slen)==-1) {
    perror("Sending datagram message error");
  } else {
    printf("Sending control %s datagram message...OK\n", control_buf);

    if (recvfrom(control_socket, &sig_ack, 1, 0, (struct sockaddr *)&addr_s, &size)==-1) {
      perror("Can't recvfrom");
      goto DEFAULT_CLEAN_UP3;
    } else {
      printf("ACK %d\n", sig_ack);
    }
  }


  for (i = 0; i < pack_num; i++) {
    if(recvfrom(sock, &databuf, PACK_SIZE, 0, (struct sockaddr *)&group, &size) < 0) {
      perror("Reading datagram message error");
      goto DEFAULT_CLEAN_UP3;
    } else {
      printf("Reading datagram message...OK.\n");
      printf("The message from multicast server is: \"%s\"\n", databuf.data);
      ptr = dst + DATA_SIZE * (databuf.num - 1);
      memcpy(ptr, databuf.data, DATA_SIZE);
     
      if (databuf.num - 1 > max_pack) {
          max_pack = databuf.num - 1;
      }
      packets[databuf.num - 1] = 1;
      printf("packet number %d/%d\n", databuf.num, pack_num);
      if ((i + 1) % WINDOW == 0) {
          int j;
          for (j = 0; j < max_pack; j++) {
              if (packets[j] == 0) {
                  printf("packets[%d] : %d", j, packets[j]);
                  
                  Default_protocol_packet lost_pack;
                  printf("Packet %d requared\n", j + 1);
                  memset(control_buf, '\0', BUFLEN);
                  sprintf(control_buf, "%d", j + 1);
                  if (sendto(control_socket, control_buf, BUFLEN, 0, (struct sockaddr *)&addr_s, slen)==-1) {
                    perror("Sending datagram message error");
                  } else {
                    printf("Sending datagram message...OK\n");
                  }
                  int size = sizeof(addr_s);
                  if (recvfrom(control_socket, &lost_pack, PACK_SIZE, 0, (struct sockaddr *)&addr_s, &size)==-1) {
                    perror("Can't recvfrom");
                    goto DEFAULT_CLEAN_UP3;
                  } else {
                    printf("Reading datagram message...OK.\n");
                    printf("The message from control server is: \"%s\"\n", lost_pack.data);
                    ptr = dst + DATA_SIZE * (lost_pack.num - 1);
                    memcpy(ptr, lost_pack.data, DATA_SIZE);
               
                    if (lost_pack.num - 1 > max_pack) {
                        max_pack = lost_pack.num - 1;
                    }
                    packets[lost_pack.num - 1] = 1;
                    printf("packets[%d] : %d", lost_pack.num - 1, packets[lost_pack.num - 1]);
                    i++;
                    printf("packet number %d/%d\n", lost_pack.num, pack_num);
                  }
              }
          }
      }

    }

  }

  memset(control_buf, '\0', BUFLEN);
  sprintf(control_buf, "%d", 0);
  if (sendto(control_socket, control_buf, BUFLEN, 0, (struct sockaddr *)&addr_s, slen)==-1) {
    perror("Sending datagram message error");
  } else {
    printf("Sending control datagram message...OK\n");
  }

  munmap(dst, data.st_size);
  close(file);
  close(control_socket);
  return;
DEFAULT_CLEAN_UP3:
  munmap(dst, data.st_size);
DEFAULT_CLEAN_UP2:
  close(file);
DEFAULT_CLEAN_UP1:
  close(control_socket);
DEFAULT_CLEAN_UP:
  close(sock);
}

// TFTP PROTOCOL

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


typedef struct __attribute__((packed)) {
    uint16_t type;
    char *filename;
    char *mode;
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


void tftp_protocol(char *filename) {
  int datalen, size;
  _DATA databuf;
  RQ request;
  int i;
  int file;

  request.filename = (char *)malloc((strlen(filename) + 1)*sizeof(char));
  request.mode = (char *)malloc((sizeof(OCTET) + 1)*sizeof(char));
  strcpy(request.filename, filename);
  strcpy(request.mode, OCTET);
  request.type = RRQ;
  
  printf("%d %s %s %d\n", request.type, request.filename, request.mode, strlen(request.filename));
  if(sendto(sock, &request.type, sizeof(request.type), 0, (struct sockaddr*)&localSock, sizeof(localSock)) < 0) {
    perror("Sending datagram message error");
  } else {
    printf("Sending datagram message...OK\n");
  }

 
    if(sendto(sock, request.filename, strlen(request.filename), 0, (struct sockaddr*)&localSock, sizeof(localSock)) < 0) {
      perror("Sending datagram message error");
    } else {
      printf("Sending datagram message...OK\n");
    }


    if(sendto(sock, request.mode, strlen(request.mode) + 1, 0, (struct sockaddr*)&localSock, sizeof(localSock)) < 0) {
      perror("Sending datagram message error");
    } else {
      printf("Sending datagram message...OK\n");
    }


  if ((file = open(request.filename, O_RDWR | O_CREAT | O_TRUNC | O_APPEND)) < 0 ) {
       printf("Can't create output file\n");
       exit(1);
  }

  int locsize = sizeof(localSock);
  while(1) {
    memset(&databuf, '\0', sizeof(_DATA));
    if(recvfrom(sock, &databuf, TFTP_DATA_SIZE, 0, (struct sockaddr *)&localSock, &size) < 0) {
      perror("Reading datagram message error");
      goto TFTP_CLEAN_UP;
    }

    if (databuf.block == 0) {
      break;
    }
    if (databuf.type == DATA) {
      printf("block num %d\n", databuf.block);
      int size = 0;

      if((size = recvfrom(sock, databuf.data, BLOCKSIZE, MSG_WAITALL, (struct sockaddr *)&localSock, &locsize)) < 0) {
        perror("Reading datagram message error");
        goto TFTP_CLEAN_UP;
      }

      printf("size %d\n", size);
      lseek(file, (databuf.block - 1)*BLOCKSIZE, SEEK_SET);
      write(file, databuf.data, size);
      if (size < BLOCKSIZE) {
        break;
      }
    } else if (databuf.type == ERR) {
      ERROR *err;
      err = (ERROR *)&databuf;

      if(recvfrom(sock, err->msg, BUFLEN, MSG_WAITALL, (struct sockaddr *)&localSock, &size) < 0) {
        perror("Reading datagram message error");
        goto TFTP_CLEAN_UP;
      }
      fprintf(stderr, "%d: %s", err->code, err->msg);
      goto TFTP_CLEAN_UP;
    }
  }

  close(file);
  return;
TFTP_CLEAN_UP:
  close(file);
  close(sock);
  exit(1);
}

int main(int argc, char *argv[])
{
  int port = PORT;
  int broadcast = FALSE;
  int protocol = DEFAULT;
  in_addr_t ip_addr = inet_addr(IP);
  int opt;
  int opt_num = 1;

  while ((opt = getopt(argc, argv, "a:p:B:T:h")) != -1) {
      switch (opt) {
          case 'a':
            if (optarg) {
              ip_addr = inet_addr(optarg);
            } else {
              print_usage(argv[0]);
              exit(1);
            }
             break;
          case 'p':
             if (optarg) {
               port = atoi(optarg);
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

  if ((protocol == TFTP)&&(opt_num >= argc)) {
    print_usage(argv[0]);
    exit(1);
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("Opening datagram socket error");
    exit(1);
  } else {
    printf("Opening datagram socket....OK.\n");
  }



  if (protocol == DEFAULT) {
    {
      int reuse = 1;
      if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        perror("Setting SO_REUSEADDR error");
        goto CLEAN_UP;
      } else {
        printf("Setting SO_REUSEADDR...OK.\n");
      }
    }

    memset((char *) &localSock, 0, sizeof(localSock));
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(port);
    group.imr_interface.s_addr = ip_addr;
    printf("%d\n",ip_addr);
    if (broadcast == FALSE) {
      localSock.sin_addr.s_addr = inet_addr(GROUP_ADDR);
      group.imr_multiaddr.s_addr = inet_addr(GROUP_ADDR);
    } else {
      localSock.sin_addr.s_addr = htonl(INADDR_ANY);
      group.imr_multiaddr.s_addr = htonl(INADDR_ANY);
    }
    if(bind(sock, (struct sockaddr*)&localSock, sizeof(localSock))) {
      perror("Binding datagram socket error");
      goto CLEAN_UP;
    } else {
      printf("Binding datagram socket...OK.\n");
    }

    if (broadcast == FALSE) {
      if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
        perror("Adding multicast group error");
        goto CLEAN_UP;
      } else {
        printf("Adding multicast group...OK.\n");
      }
    }
    default_protocol();
  }
  if (protocol == TFTP) {
    if ((sock=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
      perror("Control socket error");
      exit(1);
    }


    memset((char *) &localSock, 0, sizeof(localSock));
    localSock.sin_family = AF_INET;
    localSock.sin_port = htons(PORT);
    if (inet_aton(SRV_IP, &localSock.sin_addr)==0) {
      perror("inet_aton() failed");
      exit(1);
    }
    tftp_protocol(argv[opt_num]);
  }

  close(sock);
  return 0;
CLEAN_UP:
  close(sock);
  return 1;
}
