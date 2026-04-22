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
> Servers will see your public IP[^1]. Do not delete the `known_servers.json` file if you wish to keep MiTM protection against servers you've already connected to.
---

## How?
- polycomm, written in C, is a very simple system that uses TCP sockets to transmit messages with the help of cJSON[^2].
- The design is kept intentionally simple to minimize the number of attack surfaces and points of collapse.
- Full encryption implemented for the transport layer, more information below.

## Why?
- polycomm aims to allow users to communicate with minimal overarching organizational surveillance.

## polycomm is not:
- An anonymous platform by default - servers can see your IP.
- a large-scale platform - server performance degrades after 5K users[^3],
- aiming for mass-use - the terminal user interface is not for everyone,
- plug and play - setting up a server is tedious,
- protecting you from first-connection MiTM attacks - uses TOFU (Trust On First Use).
- 100% safe or secure from metadata linking - timestamps are not hidden.

## More Info:
- Any changes in users' local versions will get flagged and prompt user acknowledgement before usage.
  - In earlier stages, this may occur more frequently to prevent vulnerabilities from affecting users' systems. It is in best interest to update as soon as possible BEFORE using polycomm.
- GUI forks of polycomm is *NOT* officially endorsed. Use them with caution.
- Users must be aware that using polycomm comes with implied risk. polycomm is NOT responsible for any misuse[^5].
- Connecting to a server for the first time will allow users to store the public server key automatically in a file `known_servers.json`. The client will automatically compare it with the public key of the server in future connections. If there is tampering, the connection ends with a fool-proof warning.
- MiTM protection works as long as users' clients is up-to-date with polycomm and the binary is not tampered with.
- Ensure there is trust in the server being connected to - servers can view messages.
- View the current version [here](version.txt).

## Features:
- Basic server console logging.
- TOFU-based (Trust on First Use) MiTM protection[^4].
- Basic commands for clients (users), including but not limited to:
  - !leave
  - !changeusername [username]
- Anti-spam features:
  - 5 users per IP limit.
  - Message spam ratelimiting.
  - 16 character username limit.
- Encryption with libsodium using crypto_secretstream_xchacha20poly1305, full implementation visible [here](network.c).
- Anti-tamper/corruption packet handling.
- Quick setup, minimal prerequisites:
  - Linux, libsodium, cURL.
- Messages stored locally, in `.[server-ip].json` files.
  - You cannot see messages that were sent while you were not connected.

## Credits:
- @45pv on Discord -> bug hunting & penetration testing.

### Note:
- As of the latest version, this software is partially ready for daily use. No generative LLM/AI used for the creation of this README.

[^1]: polycomm supports Transport Layer Security over clients. This ensures that attackers won't be able to see packet data unless they have direct access to the server through system compromise. Thus, it is advised that the server is hardened and secure.
[^2]: cJSON is used for transporting information between clients and servers. A JSON format allows for much easier data transfer and logging given its nature.
[^3]: polycomm uses threads per client, on the server. This is the simplest setup that can be implemented while having the least complexity and best performance.
[^4]: TOFU (Trust on First Use) is a method of protection against MiTM (Man-in-The-Middle) attacks. It protects the users if the known server public key changes at any point. While it does work, it has some limitations: if you are connecting to a server while there is active spoofing, future connections will not warn you. Therefore, it is important that first ever connection to a server is safe. polycomm will actively point it out while connecting for the first time. You may double check the public key with the server.
[^5]: polycomm only provides the application for users and is not responsible for actions taken by the users, clients, servers or third parties involved.
