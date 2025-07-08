#!/bin/bash

# Test script for GTK chat applications with RSA key exchange

echo "=== Testing GTK Chat Applications with RSA Key Exchange ==="

# Check if crypto driver is loaded
if ! lsmod | grep -q crypto_driver; then
    echo "Loading crypto driver..."
    sudo insmod ../drivers/crypto_driver/crypto_driver.ko
fi

# Check if device exists
if [ ! -c /dev/crypto_dev ]; then
    echo "Creating crypto device node..."
    MAJOR=$(cat /proc/devices | grep crypto_dev | awk '{print $1}')
    sudo mknod /dev/crypto_dev c $MAJOR 0
    sudo chmod 666 /dev/crypto_dev
fi

echo "Crypto driver setup complete"
echo "Device status:"
ls -la /dev/crypto_dev

echo ""
echo "To test the RSA key exchange feature:"
echo "1. Start the GTK server: ./gtk_ui/chat_server_gtk"
echo "2. Start the GTK client: ./gtk_ui/chat_client_gtk"
echo "3. The client will perform RSA key exchange with the server automatically"
echo "4. Once connected, all messages will be encrypted with the exchanged key"
echo ""
echo "Key exchange features:"
echo "- RSA public/private key pair generation"
echo "- Secure DES key exchange using RSA encryption"
echo "- Automatic encryption of all chat messages"
echo "- Support for both login and signup operations"
echo ""
echo "Server log will show key exchange progress"
echo "Client status will show connection and key exchange status"
