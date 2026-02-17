<p align="center">
  <img
    src="polycomm.png"
    alt="polycomm"
    width="120"
  />

</p>

<h1 align="center">polycomm</h1>

<p align="center">
  Direct; autonomous.
</p>


> [!TIP]
> You must host your own servers.

> [!CAUTION]
> This is unfinished and NOT production ready. Clients and servers may see your public IP.
---

## Why?
- Ever since most platforms started pushing ID verification for communication, privacy has become a luxury.
- polycomm works by leaving moderation entirely in the hands of the users in a decentralized manner.
- This solves the problem of having a larger organization viewing everything you do.

## How?
- polycomm, written in C, is a very simple system that uses TCP sockets to transmit messages with the help of cJSON.
- There are two types of nodes: the server, and the client. Clients can connect to the server to communicate. The server handles the clients. The server also authenticates users based on a predetermined password automatically created by the server.
- The design is kept intentionally simple to minimize the number of attack surfaces and points of collapse.
- Encryption with libsodium is planned in the near future.

---
Note: polycomm works entirely in the terminal. GUI support will never be added.