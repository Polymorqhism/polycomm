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

## How?
- polycomm, written in C, is a very simple system that uses TCP sockets to transmit messages with the help of cJSON.
- There are two types of nodes: the server, and the client. Clients can connect to the server to communicate. The server handles the clients.
- The design is kept intentionally simple to minimize the number of attack surfaces and points of collapse.
- With libsodium encryption implemented, all messages are inaccessible to packet sniffing (tested).

## Why?
- polycomm aims to allow users to communicate with minimal overarching organizational surveillance.

## polycomm is not:
- An anonymous platform by default. All data is linked to your IP,
- a large-scale platform - server performance degrades after 5K users,
- aiming for mass-use,
- plug and play,
- 100% safe or secure.

## More Info:
- Any changes in users' local versions will get flagged and prompt user acknowledgement before usage.
  - In earlier stages, this may occur more frequently to prevent vulnerabilities from affecting users' systems. It is in best interest to update as soon as possible BEFORE using polycomm.
- A GUI version of polycomm is *NOT* officially endorsed. Use them with caution.
- Users must be aware that using polycomm comes with implied risk. polycomm is NOT responsible for any misuse.
- Connecting to a server for the first time will allow users to store the public server key automatically in a file `known_servers.json`. The client will automatically compare it with the public key of the server in future connections. If there is tampering, the connection ends with a fool-proof warning.
- MiTM protection works as long as users' clients is up-to-date with polycomm and the binary is not tampered with.
- View the current version [here](version.txt).