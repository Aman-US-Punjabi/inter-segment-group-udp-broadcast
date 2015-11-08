#!/bin/sh
g++ Socket.cpp UdpMulticast.cpp UdpRelay.cpp driver.cpp -o UdpRelay -lpthread