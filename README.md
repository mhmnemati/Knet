# Knet
Simple and powerful Socket programming library in C



## What is Knet ?

Knet is very powerful util for creating concurrent Servers and clients in C .
Knet supports Acceptor pools for high performance and supports different types of servers : 

1. Async - NonBlocking Servers 
2. Async - Blocking Servers
3. Sync - Blocking Servers

## Min Deps : 
```
1. CMake >= 2.8
2. POSIX rt library ( Async I/O )
3. Kprocessor library ( for creating pools and processors )
4. pthread ( used in Kprocessor )
```

# How to use :
```
Add ktcp.h , kudp.h , ktcp.c , kudp.c sources to your project and use it :)
```
