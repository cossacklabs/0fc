
![imgs/0fclogo.png](imgs/0fclogo.png)

## anonymous web chat with strong cryptography

0fc enables you to run a secure in-browser group chat with isolated chatrooms, having some special features:
- end-to-end for specific chat room: server cannot do better than DoS attack
- server is considered minimal trusted zone, all important operations happen on client side:
  - ephemeral keys used to protect chat room traffic are generated within room owner's browser and propagated to the rest
  - secret tokens, used to give access to chatroom, are generated on client side (although part of the verification happen on server side)
- during key sharing every service message is protected by keys, derived from random data of more than one party
- outgoing messages are encrypted and sent only once (all room members share the same symmetric key)
- secret access token is used once (deleted after key confirmation)

0fc was started as testing playground for some sophisticated use-cases of [Themis](https://github.com/cossacklabs/themis)/[WebThemis](https://github.com/cossacklabs/webthemis), but became interesting enough to release it as a separate blob of code.

IMPORTANT: To be considered really secure, 0fc should be validated by third parties and deployed properly. No cryptographic tool should be trusted without third-party audit. Before that happens (if it ever does), there's a protocol description at the end of this document, which allows you to take a look at the inner workings of 0fc and make your own judgement. 

0fc backend is written in Python, front-end is [WebThemis](https://github.com/cossacklabs/webthemis)-based, so it works in Google Chrome-based browsers only (yet).

0fc is licensed via Apache 2 license. We would be happy if you build something based on this code and 0fc's protocol; if you'd like any help with this, [get in touch](https://cossacklabs.com/contacts.html). 

# Installing and using

0fc consists of two components: a server and client. 

### 0fc server

0fc server requires:
- python 3.4 
- pip
- themis ([building and installing](https://github.com/cossacklabs/themis/wiki/3.1-Building-and-installing))

First, you will need to install python dependencies: 

```
pip install -r requirements.txt
```

Having done so, you can run the server: 

```
python3 server.py
```

by default server will listen to port 5103. To change the port add `-p <port>`:

```
python3 server.py -port 333
```

### 0fc client

0fc client already comes compiled in /static/ folder. Bear in mind that it has server keys hardcoded; if you regenerate the keys, you will need to rebuilt the client (see below).  

### Using 0fc

... is quite self-explanatory. You may create new room, generate tokens and invite people to join, or enter existing token to enter the room.

# Rebuilding 0fc client

If you'd like to recompile 0fc client (PNaCl object) yourself, here's what you have to do: 

1. To build PNaCl object you need to install NaCl SDK and create enviromant variable `PNACL_ROOT` with path to installed SDK files.
2. Clone 0fc repository with submodules from github:

  ```
  git clone https://github.com/cossacklabs/0fc
  cd 0fc
  git submodules update --init --recursive
  ```
  
3. Build webthemis:

  ```
  cd webthemis
  make
  ```

4. Build 0fc PNaCl module:

  ```
  cd ..
  make
  ```
  
You're done!

# Protocol & scheme

![imgs/0fc_prot.png](imgs/0fc_prot.png)

#### Room creation

- Room owner generates a key pair `[client]`
- Room owner generates room key (which will be used to encrypt messages in the room) `[client]`
- Room owner requests the server to create the room, receiving room id in response `[client]`+`[server]`

#### Inviting others (key sharing)

- Room owner generates a random (one-time) invite token `[client]`
- Room owner sends an invite by some out-of-band channel (like email), which includes invite token, his public key and room id `[client]`
- User receives the invite token `[client]`
- User generates a key pair `[client]`
- User generates random joining key `[client]`
- User sends a secure message to room owner through server with encrypted joining key `[client]`
- Server may check through ACL whether this invite is valid and pass the message to room owner `[server]`
- Room owner unwraps joining key `[client]`
- Room owner sends sealed room key to user through server using joining key as master key and invite token as context `[client]`
- Server may check through ACL whether this response is valid and pass the message to the user `[server]`
- User unseals the room key `[client]`
- User sends confirmation sealed message to the room owner. `[client]`
- Owner, upon checking users confirmation message signs his public key and sends to server `[client]`+`[server]`
- Server checks the signature and considers user as added to the chatroom `[server]`
- Once invite token been used, it is discarded by the room owner `[server]`

#### Message exchange

- Room members exchange messages sealing them with room key. Server just forwards encrypted messages without having access to their contents. `[server]`

#### Key management

- Keypair is generated for every room `[client]`
- Keypair is stored in browser persistent storage `[client]`
- Browser persistent storage is encrypted with Secure Cell (seal mode), key derived from user's password, inputs when joining the chat `[client]`

#### Server communication

- clients communicate with server using Themis secure session `[server]`
- server's trusted public key is hardcoded in the clients `[client]`
- server does not perform client authentication, automatically trusts every SS client key (this is first obvious step to harden if security is more important than ubiquity and anonymity) `[server]`

#### Key rotation

- every 100 (configurable) messages sent and received, room owner generates new key, encrypts it with old key and sends special message `[client]`
- server enforces such messages may come only from room owner `[server]`

#### Room orchestration

- a list of members is maintained for every room as a list of public keys (+indication who is room owner) `[server]`
- every room has a room owner (originally, room creator) `[server]`
- room owner is responsible for key rotation  `[client]`

#### Chat history

- server enables clients to fetch chat history since their last departure for members who have been online and know keys before rotation `[server]`
- server enables clients to fetch chat history since last key rotation for new members `[server]`
