#!/bin/bash

# Network Debug Script for VM-to-VM Chat Connection
# Linux Driver Development Project

echo "=== Network Connectivity Debug Script ==="
echo "Date: $(date)"
echo "Host: $(hostname)"
echo

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 1. Check network interfaces
echo "=== Network Interfaces ==="
ip addr show 2>/dev/null || ifconfig -a
echo

# 2. Check routing table
echo "=== Routing Table ==="
ip route show 2>/dev/null || route -n
echo

# 3. Check if we can determine VM network setup
echo "=== VM Network Detection ==="
if [ -f /sys/class/dmi/id/product_name ]; then
    PRODUCT=$(cat /sys/class/dmi/id/product_name 2>/dev/null)
    echo "Product: $PRODUCT"
fi

# Detect virtualization
if command_exists systemd-detect-virt; then
    VIRT=$(systemd-detect-virt)
    echo "Virtualization: $VIRT"
elif [ -f /proc/cpuinfo ]; then
    if grep -q "hypervisor" /proc/cpuinfo; then
        echo "Virtualization: Detected (hypervisor flag present)"
    fi
fi
echo

# 4. Check current server processes
echo "=== Active Chat Server Processes ==="
if command_exists netstat; then
    netstat -tlnp 2>/dev/null | grep -E ":(8080|8888|8889)" || echo "No chat servers found on common ports"
elif command_exists ss; then
    ss -tlnp 2>/dev/null | grep -E ":(8080|8888|8889)" || echo "No chat servers found on common ports"
else
    echo "Neither netstat nor ss available"
fi
echo

# 5. Check iptables/firewall
echo "=== Firewall Status ==="
if command_exists iptables; then
    echo "IPTables rules:"
    iptables -L -n 2>/dev/null | head -20
    echo
fi

if command_exists systemctl; then
    if systemctl is-active --quiet firewalld; then
        echo "Firewalld is active"
        if command_exists firewall-cmd; then
            echo "Default zone: $(firewall-cmd --get-default-zone 2>/dev/null)"
            echo "Active zones: $(firewall-cmd --get-active-zones 2>/dev/null)"
        fi
    elif systemctl is-active --quiet ufw; then
        echo "UFW is active"
        ufw status 2>/dev/null
    else
        echo "No active firewall service detected"
    fi
fi
echo

# 6. Test network connectivity tools
echo "=== Network Tools Available ==="
for tool in ping nc telnet nmap; do
    if command_exists $tool; then
        echo "✓ $tool available"
    else
        echo "✗ $tool not available"
    fi
done
echo

# 7. Get NAT/Bridge network info
echo "=== NAT Network Information ==="
echo "Default gateway:"
ip route show default 2>/dev/null || route -n | grep "^0.0.0.0"

echo
echo "DNS servers:"
if [ -f /etc/resolv.conf ]; then
    grep nameserver /etc/resolv.conf
fi
echo

# 8. VM-specific network configuration
echo "=== VM Network Configuration Suggestions ==="
echo "For VirtualBox NAT networking:"
echo "  - Host-only adapter: Usually 192.168.56.x/24"
echo "  - NAT adapter: Usually 10.0.2.x/24"
echo "  - Port forwarding needed for NAT-to-NAT communication"
echo
echo "For VMware NAT networking:"
echo "  - NAT adapter: Usually 192.168.x.x/24"
echo "  - vmnet8 (NAT) or vmnet1 (Host-only)"
echo
echo "Current IP addresses detected:"
ip addr show | grep "inet " | grep -v "127.0.0.1" | awk '{print $2}' | cut -d'/' -f1

echo
echo "=== Recommended Solutions ==="
echo "1. Change VM networking to 'Bridged' mode for both VMs"
echo "2. Use 'Host-only' networking with both VMs on same network"
echo "3. Set up port forwarding for NAT (if using VirtualBox)"
echo "4. Configure firewall to allow chat application ports"
echo

# 9. Test connectivity function
test_connectivity() {
    local target_ip="$1"
    local target_port="$2"
    
    echo "Testing connectivity to $target_ip:$target_port"
    
    if command_exists nc; then
        echo -n "  nc test: "
        if timeout 3 nc -z "$target_ip" "$target_port" 2>/dev/null; then
            echo "SUCCESS"
        else
            echo "FAILED"
        fi
    fi
    
    if command_exists telnet; then
        echo -n "  telnet test: "
        if timeout 3 bash -c "echo '' | telnet $target_ip $target_port" 2>/dev/null | grep -q "Connected"; then
            echo "SUCCESS"
        else
            echo "FAILED"
        fi
    fi
}

# 10. Interactive testing
echo "=== Interactive Testing ==="
echo "Enter server IP address to test (or press Enter to skip):"
read -r SERVER_IP
if [ -n "$SERVER_IP" ]; then
    echo "Enter server port (default 8888):"
    read -r SERVER_PORT
    SERVER_PORT=${SERVER_PORT:-8888}
    
    echo "Testing connection to $SERVER_IP:$SERVER_PORT"
    test_connectivity "$SERVER_IP" "$SERVER_PORT"
fi

echo
echo "=== Debug Script Complete ==="
echo "Save this output and check the suggested solutions above."
