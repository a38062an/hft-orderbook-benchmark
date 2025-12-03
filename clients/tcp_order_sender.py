import socket
import time
import random

def create_fix_message(order_id, price, qty, side):
    # Minimal FIX message: 8=FIX.4.2|35=D|11=ID|54=Side|38=Qty|44=Price|40=2|
    body = f"35=D\x0111={order_id}\x0154={side}\x0138={qty}\x0144={price}\x0140=2\x01"
    
    header = f"8=FIX.4.2\x019={len(body)}\x01"
    msg = header + body
    
    # Checksum
    checksum = sum(ord(c) for c in msg) % 256
    msg += f"10={checksum:03}\x01"
    
    return msg.encode('ascii')

def main():
    host = '127.0.0.1'
    port = 12345
    
    print(f"Connecting to {host}:{port}...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
    except ConnectionRefusedError:
        print("Connection refused. Is the server running?")
        return

    print("Connected. Sending orders...")
    
    start_time = time.time()
    count = 10000
    
    for i in range(count):
        side = 1 if random.random() > 0.5 else 2
        price = random.randint(90, 110)
        qty = random.randint(1, 100)
        
        msg = create_fix_message(i, price, qty, side)
        s.sendall(msg)
        
        if i % 1000 == 0:
            print(f"Sent {i} orders")
            
    end_time = time.time()
    duration = end_time - start_time
    print(f"Sent {count} orders in {duration:.4f}s ({count/duration:.2f} orders/s)")
    
    s.close()

if __name__ == "__main__":
    main()
