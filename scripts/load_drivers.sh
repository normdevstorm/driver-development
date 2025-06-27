#!/bin/bash

# Script để load drivers
# Bài tập lớn - Lập trình Driver Linux

set -e

echo "=== Loading Linux Drivers ==="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    echo "Usage: sudo $0"
    exit 1
fi

# Function to load required crypto modules
load_crypto_modules() {
    echo "Loading required crypto modules..."
    
    # Load DES crypto module
    if ! lsmod | grep -q "des_generic"; then
        echo "Loading des_generic module..."
        modprobe des_generic || {
            echo "✗ Failed to load des_generic module"
            echo "Trying alternative method..."
            # Try to find and load the module directly
            if [ -f "/lib/modules/$(uname -r)/kernel/crypto/des_generic.ko" ]; then
                insmod "/lib/modules/$(uname -r)/kernel/crypto/des_generic.ko"
            else
                echo "✗ des_generic module not found"
                exit 1
            fi
        }
        echo "✓ des_generic module loaded"
    else
        echo "✓ des_generic module already loaded"
    fi
    
    # Load SHA1 crypto module
    if ! lsmod | grep -q "sha1_generic"; then
        echo "Loading sha1_generic module..."
        modprobe sha1_generic || {
            echo "Warning: Failed to load sha1_generic module"
            echo "SHA1 might be built into kernel"
        }
    fi
    
    # Load crypto manager if not loaded
    if ! lsmod | grep -q "cryptomgr"; then
        echo "Loading cryptomgr module..."
        modprobe cryptomgr || echo "Warning: cryptomgr load failed (might be built-in)"
    fi
}

# Function to load crypto driver
load_crypto_driver() {
    echo "Loading crypto driver..."
    
    # Remove if already loaded
    if lsmod | grep -q "crypto_driver"; then
        echo "Removing existing crypto_driver module..."
        rmmod crypto_driver || true
    fi
    
    # Load the module
    cd ../drivers/crypto_driver
    if [ -f crypto_driver.ko ]; then
        insmod crypto_driver.ko
        echo "✓ Crypto driver loaded"
        
        # Wait a bit for device creation
        sleep 1
        
        # Set device permissions
        if [ -c /dev/crypto_dev ]; then
            chmod 666 /dev/crypto_dev
            echo "✓ Device permissions set for /dev/crypto_dev"
        else
            echo "Warning: /dev/crypto_dev not found"
        fi
    else
        echo "✗ crypto_driver.ko not found. Run build.sh first."
        exit 1
    fi
    cd ../../scripts
}

# Function to load USB keyboard driver
load_usb_driver() {
    echo
    echo "Loading USB keyboard driver..."
    
    # Remove if already loaded
    if lsmod | grep -q "usb_keyboard"; then
        echo "Removing existing usb_keyboard module..."
        rmmod usb_keyboard || true
    fi
    
    # Load the module
    cd ../drivers/usb_keyboard
    if [ -f usb_keyboard.ko ]; then
        insmod usb_keyboard.ko
        echo "✓ USB keyboard driver loaded"
    else
        echo "✗ usb_keyboard.ko not found. Run build.sh first."
        exit 1
    fi
    cd ../../scripts
}

# Function to check loaded modules
check_modules() {
    echo
    echo "Checking loaded modules..."
    
    if lsmod | grep -q "crypto_driver"; then
        echo "✓ crypto_driver is loaded"
    else
        echo "✗ crypto_driver is not loaded"
    fi
    
    if lsmod | grep -q "usb_keyboard"; then
        echo "✓ usb_keyboard is loaded"
    else
        echo "✗ usb_keyboard is not loaded"
    fi
    
    echo
    echo "All loaded modules:"
    lsmod | grep -E "(crypto_driver|usb_keyboard)" || echo "No custom modules loaded"
}

# Function to show device information
show_devices() {
    echo
    echo "Device information:"
    
    # Check crypto device
    if [ -c /dev/crypto_dev ]; then
        ls -l /dev/crypto_dev
    else
        echo "/dev/crypto_dev not found"
    fi
    
    # Check dmesg for driver messages
    echo
    echo "Recent kernel messages (last 20 lines):"
    dmesg | tail -20 | grep -E "(crypto_driver|usb_keyboard)" || echo "No recent driver messages"
}

# Main function
main() {
    echo "Loading drivers for Linux Driver Project..."
    
    load_crypto_modules
    load_crypto_driver
    load_usb_driver
    check_modules
    show_devices
    
    echo
    echo "=== Drivers Loaded Successfully ==="
    echo
    echo "You can now:"
    echo "1. Test crypto functionality: cd ../userspace && ./test_crypto"
    echo "2. Run chat applications:"
    echo "   Terminal 1: cd ../userspace && ./chat_server"
    echo "   Terminal 2: cd ../userspace && ./chat_client"
    echo
    echo "To unload drivers: sudo ./unload_drivers.sh"
}

# Run main function
main "$@"
