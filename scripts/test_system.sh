#!/bin/bash

# Test script cho toàn bộ hệ thống
# Bài tập lớn - Lập trình Driver Linux

echo "=== System Test Script ==="
echo

# Function to test crypto driver
test_crypto_driver() {
    echo "Testing crypto driver..."
    
    if [ -c /dev/crypto_dev ]; then
        echo "✓ Crypto device exists: /dev/crypto_dev"
        
        # Test with userspace program
        cd ../userspace
        if [ -x ./test_crypto ]; then
            echo "Running crypto tests..."
            ./test_crypto
        else
            echo "✗ test_crypto executable not found. Run make in userspace/"
        fi
        cd ../scripts
    else
        echo "✗ Crypto device not found. Is crypto_driver loaded?"
    fi
}

# Function to test USB driver
test_usb_driver() {
    echo
    echo "Testing USB keyboard driver..."
    
    if lsmod | grep -q "usb_keyboard"; then
        echo "✓ USB keyboard driver is loaded"
        
        # Check for USB keyboards
        echo "Connected USB devices:"
        lsusb | grep -i keyboard || echo "No USB keyboards detected"
        
        echo "Check dmesg for USB keyboard events:"
        dmesg | grep -i "usb_keyboard" | tail -5 || echo "No USB keyboard messages"
    else
        echo "✗ USB keyboard driver not loaded"
    fi
}

# Function to test chat system
test_chat_system() {
    echo
    echo "Testing chat system build..."
    
    cd ../userspace
    if [ -x ./chat_server ] && [ -x ./chat_client ]; then
        echo "✓ Chat applications built successfully"
        echo "To test chat system:"
        echo "  Terminal 1: ./chat_server"
        echo "  Terminal 2: ./chat_client"
        echo
        echo "Test accounts:"
        echo "  Username: admin, Password: admin123"
        echo "  Username: user1, Password: password1"
        echo "  Username: user2, Password: password2"
    else
        echo "✗ Chat applications not found. Run make in userspace/"
    fi
    cd ../scripts
}

# Function to show system information
show_system_info() {
    echo
    echo "=== System Information ==="
    echo "Kernel version: $(uname -r)"
    echo "Architecture: $(uname -m)"
    echo "OS: $(cat /etc/redhat-release 2>/dev/null || echo 'Unknown')"
    echo
    echo "Loaded custom modules:"
    lsmod | grep -E "(crypto_driver|usb_keyboard)" || echo "No custom modules loaded"
    echo
    echo "Device files:"
    ls -l /dev/crypto_dev 2>/dev/null || echo "/dev/crypto_dev not found"
}

# Function to check build status
check_build_status() {
    echo
    echo "=== Build Status ==="
    
    # Check driver builds
    echo "Checking driver builds..."
    if [ -f ../drivers/crypto_driver/crypto_driver.ko ]; then
        echo "✓ crypto_driver.ko exists"
    else
        echo "✗ crypto_driver.ko not found"
    fi
    
    if [ -f ../drivers/usb_keyboard/usb_keyboard.ko ]; then
        echo "✓ usb_keyboard.ko exists"
    else
        echo "✗ usb_keyboard.ko not found"
    fi
    
    # Check userspace builds
    echo "Checking userspace builds..."
    cd ../userspace
    for app in chat_server chat_client test_crypto; do
        if [ -x ./$app ]; then
            echo "✓ $app exists and is executable"
        else
            echo "✗ $app not found or not executable"
        fi
    done
    cd ../scripts
}

# Main function
main() {
    echo "Running comprehensive system tests..."
    
    show_system_info
    check_build_status
    test_crypto_driver
    test_usb_driver
    test_chat_system
    
    echo
    echo "=== Test Complete ==="
    echo
    echo "If any tests failed, try:"
    echo "1. Build system: ./build.sh"
    echo "2. Load drivers: sudo ./load_drivers.sh"
    echo "3. Run tests again: ./test_system.sh"
}

# Run main function
main "$@"
