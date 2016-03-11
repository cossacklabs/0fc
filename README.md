
![imgs/0fclogo.png](imgs/0fclogo.png)

#  0fc aka zeroface 
## anonymous web chat with strong cryptography

0fc was started as testing playground for some sophisticated use-cases of [Themis](https://github.com/cossacklabs/themis)/[WebThemis](https://github.com/cossacklabs/webthemis), but became interesting enough to release it as a separate blob of code.

0fc enables you to run a secure in-browser group chat with isolated chatrooms, having some special features:
- end-to-end for specific chat room: server cannot do better than DoS attack
- server is considered minimal trusted zone, all important operations happen on client side:
  - ephemeral keys used to protect chat room traffic are generated within room owner's browser and propagated to the rest
  - secret tokens, used to give access to chatroom, are generated on client side (although part of the verification happen on server side)
- during key sharing every service message is protected by keys, derived from random data of more than one party

0fc backend is written in Python, front-end is [WebThemis](https://github.com/cossacklabs/webthemis)-based, so it works in Google Chrome-based browsers only (yet).

0fc is licensed via Apache 2 license. 

## Installing

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

0fc client already comes compiled in /static/ folder. But if you'd like to recompile it yourself, here are the steps: 

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

# 
