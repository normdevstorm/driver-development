#!/bin/bash

# Network Fix Script for VM-to-VM Chat Connection
# Linux Driver Development Project

echo "=== VM-to-VM Network Connection Fix Script ==="
echo "Date: $(date)"
echo "Host: $(hostname)"
echo "Current IP: $(ip route get 1.1.1.1 2>/dev/null | awk '{print $7}' | head -1)"
echo

# Function to check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo "⚠️  This script needs root privileges for firewall configuration"
        echo "    Run with: sudo $0"
        return 1
    fi
    return 0
}

# Function to configure firewall for chat application
configure_firewall() {
    echo "=== Configuring Firewall for Chat Application ==="
    
    # Check if firewalld is running
    if systemctl is-active --quiet firewalld; then
        echo "✓ Firewalld is active - configuring rules"
        
        # Add rules for common chat ports
        for port in 8080 8888 8889; do
            echo "  Adding rule for port $port/tcp"
            firewall-cmd --permanent --add-port=$port/tcp 2>/dev/null
            firewall-cmd --add-port=$port/tcp 2>/dev/null
        done
        
        # Add service for our chat application
        echo "  Adding chat application service"
        firewall-cmd --permanent --add-service=ssh 2>/dev/null  # Keep SSH
        firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="192.168.43.0/24" accept' 2>/dev/null
        
        # Reload firewall
        echo "  Reloading firewall configuration"
        firewall-cmd --reload
        
        echo "✓ Firewall configured successfully"
        
    elif systemctl is-active --quiet ufw; then
        echo "✓ UFW is active - configuring rules"
        ufw allow from 192.168.43.0/24 to any port 8080
        ufw allow from 192.168.43.0/24 to any port 8888
        ufw allow from 192.168.43.0/24 to any port 8889
        echo "✓ UFW configured successfully"
        
    else
        echo "ℹ️  No active firewall detected"
    fi
    echo
}

# Function to test network connectivity
test_network() {
    local target_ip="$1"
    local target_port="${2:-8888}"
    
    echo "=== Testing Network Connectivity ==="
    echo "Target: $target_ip:$target_port"
    
    # Test ping first
    echo -n "  Ping test: "
    if ping -c 1 -W 3 "$target_ip" >/dev/null 2>&1; then
        echo "✓ SUCCESS"
    else
        echo "✗ FAILED - Host unreachable"
        return 1
    fi
    
    # Test port connectivity
    echo -n "  Port test: "
    if timeout 5 nc -z "$target_ip" "$target_port" 2>/dev/null; then
        echo "✓ SUCCESS - Port is open"
    else
        echo "✗ FAILED - Port is closed or filtered"
    fi
    echo
}

# Function to start a simple test server
start_test_server() {
    local port="${1:-8888}"
    echo "=== Starting Test Server ==="
    echo "Port: $port"
    
    # Kill any existing test servers
    pkill -f "nc.*-l.*$port" 2>/dev/null
    
    # Start simple netcat server
    echo "Starting netcat server on port $port"
    echo "Server will echo received messages"
    nc -l -p "$port" -k &
    local server_pid=$!
    
    echo "✓ Test server started (PID: $server_pid)"
    echo "  You can test from another VM with: nc $(ip route get 1.1.1.1 2>/dev/null | awk '{print $7}' | head -1) $port"
    echo "  To stop server: kill $server_pid"
    echo
    
    return $server_pid
}

# Function to test chat application
test_chat_app() {
    echo "=== Testing Chat Application ==="
    
    local app_dir="/home/normservercentos/Documents/project/driver-development"
    
    if [ -f "$app_dir/userspace/chat_server" ]; then
        echo "✓ Chat server binary found"
        
        # Check if server is already running
        if pgrep -f "chat_server" >/dev/null; then
            echo "  Chat server is already running"
        else
            echo "  Starting chat server..."
            cd "$app_dir/userspace"
            ./chat_server &
            local server_pid=$!
            echo "  Chat server started (PID: $server_pid)"
        fi
    else
        echo "✗ Chat server binary not found"
        echo "  Building chat applications..."
        cd "$app_dir/userspace"
        make clean && make
        
        if [ -f "chat_server" ]; then
            echo "✓ Chat server built successfully"
        else
            echo "✗ Failed to build chat server"
        fi
    fi
    echo
}

# Function to show current network status
show_network_status() {
    echo "=== Current Network Status ==="
    
    echo "Network interfaces:"
    ip addr show | grep -E "inet " | grep -v "127.0.0.1"
    
    echo
    echo "Active connections on chat ports:"
    if command -v netstat >/dev/null; then
        netstat -tlnp 2>/dev/null | grep -E ":(8080|8888|8889)" || echo "  No active chat servers"
    elif command -v ss >/dev/null; then
        ss -tlnp 2>/dev/null | grep -E ":(8080|8888|8889)" || echo "  No active chat servers"
    fi
    
    echo
    echo "Firewall status:"
    if systemctl is-active --quiet firewalld; then
        echo "  Firewalld: Active"
        firewall-cmd --list-ports 2>/dev/null | head -1
    elif systemctl is-active --quiet ufw; then
        echo "  UFW: Active"
    else
        echo "  No active firewall"
    fi
    echo
}

# Main execution
main() {
    case "${1:-help}" in
        "firewall")
            if check_root; then
                configure_firewall
            fi
            ;;
        "test")
            if [ -n "$2" ]; then
                test_network "$2" "$3"
            else
                echo "Usage: $0 test <target_ip> [port]"
                echo "Example: $0 test 192.168.43.131 8888"
            fi
            ;;
        "server")
            start_test_server "$2"
            ;;
        "chat")
            test_chat_app
            ;;
        "status")
            show_network_status
            ;;
        "fix")
            echo "🔧 Running complete network fix..."
            if check_root; then
                configure_firewall
            fi
            test_chat_app
            show_network_status
            echo "✅ Network fix complete!"
            ;;
        *)
            echo "VM-to-VM Network Fix Script"
            echo
            echo "Usage: $0 <command> [options]"
            echo
            echo "Commands:"
            echo "  firewall          Configure firewall (requires sudo)"
            echo "  test <ip> [port]  Test connection to target"
            echo "  server [port]     Start test server"
            echo "  chat              Test chat application"
            echo "  status            Show network status"
            echo "  fix               Run complete fix (requires sudo)"
            echo
            echo "Examples:"
            echo "  sudo $0 firewall"
            echo "  $0 test 192.168.43.131 8888"
            echo "  $0 server 8888"
            echo "  sudo $0 fix"
            ;;
    esac
}

# Run main function with all arguments
main "$@"
