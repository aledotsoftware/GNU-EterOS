import socket
import sys
import time

def send_command(cmd, host='127.0.0.1', port=4444, timeout=5):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect((host, port))
        
        # Clear buffer
        s.settimeout(0.5)
        try:
            while True:
                s.recv(4096)
        except socket.timeout:
            pass
            
        s.settimeout(timeout)
        s.sendall((cmd + '\n').encode('utf-8'))
        
        output = ""
        try:
            while True:
                data = s.recv(4096).decode('utf-8', errors='replace')
                if not data:
                    break
                output += data
                # Stop if we see the shell prompt
                if "/> " in output or "/$ " in output:
                    break
        except socket.timeout:
            pass
            
        s.close()
        return output
    except Exception as e:
        return f"Error: {e}"

if __name__ == "__main__":
    if len(sys.argv) > 1:
        cmd = " ".join(sys.argv[1:])
        print(send_command(cmd))
    else:
        print("Usage: python interact_serial.py <command>")
