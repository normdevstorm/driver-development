#!/bin/bash

# Network troubleshooting script for chat application

echo "=== Chat Application Network Troubleshooting ==="
echo

# Check if we're running as root for some commands
if [[ $EUID -eq 0 ]]; then
    IS_ROOT=true
else
    IS_ROOT=false
fi

# Server port
SERVER_PORT=8888

# 1. Check port availability
echo "1. Checking if port $SERVER_PORT is available..."
if netstat -tuln 2>/dev/null | grep -q ":$SERVER_PORT "; then
    echo "   ❌ Port $SERVER_PORT is already in use:"
    netstat -tuln | grep ":$SERVER_PORT "
    echo "   You may need to stop the existing service or use a different port."
else
    echo "   ✅ Port $SERVER_PORT is available"
fi
echo

# 2. Check firewall status
echo "2. Checking firewall status..."
if $IS_ROOT; then
    if command -v firewall-cmd >/dev/null 2>&1; then
        echo "   Firewall status:"
        firewall-cmd --state 2>/dev/null || echo "   Firewall service not running"
        
        echo "   Checking if port $SERVER_PORT is open:"
        if firewall-cmd --query-port=$SERVER_PORT/tcp 2>/dev/null; then
            echo "   ✅ Port $SERVER_PORT/tcp is open in firewall"
        else
            echo "   ❌ Port $SERVER_PORT/tcp is NOT open in firewall"
            echo "   To open it, run:"
            echo "   sudo firewall-cmd --permanent --add-port=$SERVER_PORT/tcp"
            echo "   sudo firewall-cmd --reload"
        fi
    else
        echo "   firewall-cmd not found, checking iptables..."
        if command -v iptables >/dev/null 2>&1; then
            iptables -L -n | grep -q "$SERVER_PORT" && echo "   Found iptables rules for port $SERVER_PORT" || echo "   No specific iptables rules found for port $SERVER_PORT"
        fi
    fi
else
    echo "   ⚠️  Run as root to check firewall status"
fi
echo

# 3. Check network interfaces
echo "3. Checking network interfaces..."
echo "   Available network interfaces:"
ip addr show | grep -E "^[0-9]+:" | while read line; do
    iface=$(echo "$line" | cut -d: -f2 | tr -d ' ')
    echo "   - $iface"
done

echo "   Server will bind to all interfaces (0.0.0.0:$SERVER_PORT)"
echo

# 4. Test local connectivity
echo "4. Testing local connectivity..."
echo "   Testing if we can bind to port $SERVER_PORT locally..."

# Create a simple test to see if we can bind to the port
python3 -c "
import socket
import sys

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('0.0.0.0', $SERVER_PORT))
    s.close()
    print('   ✅ Successfully bound to port $SERVER_PORT')
    sys.exit(0)
except Exception as e:
    print(f'   ❌ Failed to bind to port $SERVER_PORT: {e}')
    sys.exit(1)
" 2>/dev/null || echo "   ⚠️  Python3 not available for testing"

echo

# 5. Network connectivity suggestions
echo "5. Common connection issues and solutions:"
echo
echo "   For LOCAL connections (same machine):"
echo "   - Use IP: 127.0.0.1 or localhost"
echo "   - Make sure server is running before connecting client"
echo
echo "   For REMOTE connections (different machines):"
echo "   - Find server IP: ip addr show | grep inet"
echo "   - Use server's actual IP address (not 127.0.0.1)"
echo "   - Ensure firewall allows port $SERVER_PORT"
echo "   - Check if SELinux is blocking connections"
echo
echo "   If connection still fails:"
echo "   - Check server logs for errors"
echo "   - Try telnet <server_ip> $SERVER_PORT to test basic connectivity"
echo "   - Verify no VPN or proxy interference"
echo

# 6. Quick firewall fix
echo "6. Quick firewall configuration (run as root):"
echo "   sudo firewall-cmd --permanent --add-port=$SERVER_PORT/tcp"
echo "   sudo firewall-cmd --reload"
echo
echo "   Or temporarily disable firewall for testing:"
echo "   sudo systemctl stop firewalld"
echo "   (Remember to restart it: sudo systemctl start firewalld)"
echo

# 7. SELinux check
echo "7. SELinux status:"
if command -v sestatus >/dev/null 2>&1; then
    sestatus | grep "SELinux status"
    echo "   If SELinux is enforcing and blocking connections, you may need to:"
    echo "   sudo setsebool -P nis_enabled 1"
    echo "   Or temporarily: sudo setenforce 0 (for testing only)"
else
    echo "   SELinux tools not found"
fi
echo

echo "=== Troubleshooting Complete ==="
echo
echo "Quick test commands:"
echo "1. Start server: ./gtk_ui/chat_server_gtk"
echo "2. Test connection: telnet <server_ip> $SERVER_PORT"
echo "3. Start client: ./gtk_ui/chat_client_gtk"
echo
echo "Common server IPs to try:"
echo "- Local machine: 127.0.0.1"
echo "- Network interfaces:"
ip addr show | grep 'inet ' | grep -v '127.0.0.1' | awk '{print "  - " $2}' | cut -d'/' -f1
