
# Grintorrent

Grintorrent is a torrent protocol and client designed for use on Grinnell College local computers.

## Program Usage

To run Grintorrent, execute the `make` command in the project directory. This will create an executable called `grintorrent`. You can run the `grintorrent` command with the `-h` flag to view the usage instructions:

```bash
./grintorrent -h
```

### Required Flags

When running Grintorrent, you must specify the following flags:

- `-u`  
  A username to join the network with. (Must be a string)

- `-p`  
  The peer hostname to connect to. (If specified, it must be used in conjunction with the `-n` flag)

- `-n`  
  The port of the peer to connect to. (If specified, it must be used in conjunction with the `-p` flag)

- `-f`  
  The file to join the network with.

## How to Use the Program

Once the program runs, a UI window will open. You must press **Enter** to activate the window, as it uses an input callback triggered by the Enter key.

When you press Enter, a list of all the current files on the network will appear. You can type the name of any file from the list to download it. 

- If the file is already downloaded, the program will notify you.
- Once the file is successfully downloaded, you will receive a confirmation message.

## Limitations

Currently, there are some known limitations in the program:

- Sending file chunks over the network is buggy and does not work as expected.
- However, when tested over localhost, it gives the correct results.

We believe the issue lies in the way files are transmitted over the network, and this is an area for future improvement.
s