#!/bin/bash

# Script để unload drivers
# Bài tập lớn - Lập trình Driver Linux

echo "=== Unloading Linux Drivers ==="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Usage: sudo $0"
    exit 1
fi

# Function to unload crypto driver
unload_crypto_driver() {
    echo "Unloading crypto driver..."
    
    if lsmod | grep -q "crypto_driver"; then
        rmmod crypto_driver
        echo "✓ Crypto driver unloaded"
    else
        echo "Crypto driver not loaded"
    fi
}

# Function to unload USB keyboard driver  
unload_usb_driver() {
    echo "Unloading USB keyboard driver..."
    
    if lsmod | grep -q "usb_keyboard"; then
        rmmod usb_keyboard
        echo "✓ USB keyboard driver unloaded"
    else
        echo "USB keyboard driver not loaded"
    fi
}

# Function to check modules after unloading
check_cleanup() {
    echo
    echo "Checking cleanup..."
    
    if lsmod | grep -q -E "(crypto_driver|usb_keyboard)"; then
        echo "Warning: Some modules still loaded:"
        lsmod | grep -E "(crypto_driver|usb_keyboard)"
    else
        echo "✓ All custom drivers unloaded successfully"
    fi
    
    # Check if device files still exist
    if [ -c /dev/crypto_dev ]; then
        echo "Note: /dev/crypto_dev still exists (will be removed on reboot)"
    fi
}

# Main function
main() {
    echo "Unloading all custom drivers..."
    
    unload_usb_driver
    unload_crypto_driver
    check_cleanup
    
    echo
    echo "=== Drivers Unloaded Successfully ==="
}

# Run main function
main "$@"
