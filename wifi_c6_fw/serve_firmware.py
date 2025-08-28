#!/usr/bin/env python3
"""
Simple HTTP server to serve ESP32-C6 firmware for OTA update
"""

import http.server
import socketserver
import os
import socket

PORT = 8080

class CustomHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        # Add CORS headers for cross-origin requests
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, OPTIONS')
        super().end_headers()
    
    def log_message(self, format, *args):
        # Custom logging with timestamp
        print(f"[{self.log_date_time_string()}] {format % args}")

def get_local_ip():
    """Get the local IP address of this machine"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip

if __name__ == "__main__":
    # Change to the directory containing the firmware
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    local_ip = get_local_ip()
    
    with socketserver.TCPServer(("", PORT), CustomHTTPRequestHandler) as httpd:
        print(f"ESP32-C6 Firmware OTA Server")
        print(f"=" * 50)
        print(f"Serving at: http://{local_ip}:{PORT}")
        print(f"Firmware URL: http://{local_ip}:{PORT}/ESP32C6-WiFi-SDIO-Interface-V1.4.1-96bea3a_0x0.bin")
        print(f"=" * 50)
        print(f"Use this URL in your ESP32-P4 OTA update code")
        print(f"Press Ctrl-C to stop the server")
        print()
        
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped")