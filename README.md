# Multicast file transfer

This is the part of Multicast file transfer project which is supposed to be used to transfer file through multicast or broadcast. There is an additional option to connect by using simple realisation of [TFTP](https://tools.ietf.org/html/rfc1350) protocol. Here you can find the source code for server and client.

## Getting Started

Choose the version of project. You can use wait model where server waits some time after file transmition to send lost packets. In the other model, the server monitors that all the clients recieved file.

### Installing

Download client and server sources. 
You should use server IP address as SRV_IP which defined in client source code. Also you should define IP address in server source code. You can change IP address in code or by using command line options.
Set comfortable waiting time.

```
$ gcc clt.c -o clt
$ gcc srv.c -o srv
$ ./srv
Usage: ./srv [options] [filename]
Options:
-p		port
-BT		broadcast transfer
-TFTP		use TFTP protocol
-h		print this message
```


## Project

[This](http://w27802.vdi.mipt.ru/about.php) is online interface to manage multicast file transfer.

