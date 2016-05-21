# Multicast file transfer

This is the part of Multicast file transfer project which is supposed to be used to transfer file through multicast or broadcast. There is an additional option to connect by using simple realisation of [TFTP](https://tools.ietf.org/html/rfc1350) protocol. Here you can find the source code for server and client.

## Getting Started

Choose the version of project. You can use [waiting model](https://github.com/Pereberina/file/tree/master/wait) where the server waits some time after file transmision to resend lost packets if it is neccessary. In the [other model](https://github.com/Pereberina/file/tree/master/alarm), the server monitors whether all the clients have recieved the file.

### Installing

Download the source files. 
Set `SRV_IP` which is defined in `clt.c` into your server's IP address.

```c
#define SRV_IP "192.168.1.1" // address of the server
```

Also you should define IP address in the server source code.

```c
#define IP "192.168.1.62" // address of the current machine
```

You can change client IP address in the source code or by using command line options.

Set comfortable waiting time and the other defined parameters. 

Don't forget to make changes both in `clt.c` and `srv.c`.

Compile server and client by using gcc.

```bash
$ gcc clt.c -o clt
$ gcc srv.c -o srv
```

### Usage

```bash
$ ./srv -h
Usage: ./srv [options] [filename]
Options:
-p		port
-BT		broadcast transfer
-TFTP	use TFTP protocol
-h		print this message
```
```bash
$ ./clt -h
Usage: ./clt [options]
Options:
-p		port
-BT		broadcast transfer
-TFTP	use TFTP protocol
-h		print this message
```
In default protocol, you should start all the clients before the server. You can skip the options. Simple example:
```bash
$ ./clt 
```

```bash
$ ./srv hello.txt
```

In TFTP protocol, you should start the server at first. After that, you should start the client with the name of the file you want to get from the server. Use flag '-TFTP':
```bash
$ ./srv -TFTP
```

```bash
$ ./clt -TFTP hello.txt
```

## Project

This is the educational project for the Network technologies course. [Here](http://w27802.vdi.mipt.ru/about.php) is an online interface to manage the multicast file transfer. 

