# Portless
Portless is an application that allows you to port forward ports without actually port forwarding them.

## Usage
We are going to be using a relay server which needs to have a public IP and allow clients from outside the network to connect.

### On the relay server:
1. Head to [releases](https://github.com/portless-zip/portless/releases) and download the relay server app.
2. Run the app. If on Linux, run chmod +x ./relay && ./relay (PORTS YOU WANT TO ALLOW)
2.1. example: ./relay 80 8080 25565 25566 6767 (haha, guys i'm funny right)
3. The relay is now running at port 36008.

Make sure to port forward 36008 and the port you want to make accessible on the relay!

### On the client:
1. Head to [releases](https://github.com/portless-zip/portless/releases) and download the client app.
2. Run the app. If on Linux, run chmod +x ./client && ./client {PORT YOU WANT TO SHARE} {RELAY IP ADDRESS}
3. The app should succesfully connect and the service hosted on your client should be accessible under the relay's ip address and port.
