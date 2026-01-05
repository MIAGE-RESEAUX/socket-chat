import socket
import time
import sys

def receive_until(sock, kw, timeout=2):
    sock.settimeout(timeout)
    data = b""
    start = time.time()
    while time.time() - start < timeout:
        try:
            chunk = sock.recv(4096)
            if not chunk: break
            data += chunk
            if kw.encode() in data:
                return True, data.decode()
        except socket.timeout:
            break
    return False, data.decode()

def test():
    print("--- Starting Verification ---")
    
    # Client 1
    s1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s1.connect(('127.0.0.1', 8080))
    except:
        print("Could not connect to server. Is it running?")
        sys.exit(1)

    # Auth Client 1
    s1.send(b"SIGNUP user1 pass1\n")
    time.sleep(0.1)
    s1.send(b"LOGIN user1 pass1\n")
    ok, msg = receive_until(s1, "SUCCES_SESSION")
    if not ok:
        print("Client 1 Login Failed:", msg)
        return False
    print("Client 1 Logged in")

    # Client 1 Creates Channel
    s1.send(b"/create secret private 1234\n")
    ok, msg = receive_until(s1, "Canal")
    print("Create output:", msg)

    # Client 1 Joins (Wait, /create usually implies join or stay? The server logic was: /create just creates. Need to join)
    s1.send(b"/join secret 1234\n")
    ok, msg = receive_until(s1, "rejoint")
    if not ok:
        print("Client 1 Join Failed:", msg)
        return False
    print("Client 1 Joined 'secret'")

    # Client 1 sends message
    s1.send(b"Hello secret world\n")
    time.sleep(0.5)

    # Client 2
    s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s2.connect(('127.0.0.1', 8080))
    
    # Auth Client 2
    s2.send(b"SIGNUP user2 pass2\n")
    time.sleep(0.1)
    s2.send(b"LOGIN user2 pass2\n")
    receive_until(s2, "SUCCES_SESSION")
    print("Client 2 Logged in")

    # Client 2 Joins
    s2.send(b"/join secret 1234\n")
    ok, msg = receive_until(s2, "rejoint")
    if not ok:
        print("Client 2 Join Failed:", msg)
        # return False # Continue to see what happens

    # Check for history
    # The history should be sent immediately after join
    # expected: "[user1] Hello secret world"
    ok, msg = receive_until(s2, "Hello secret world")
    if ok:
        print("SUCCESS: Client 2 received history message.")
    else:
        print("FAILURE: Client 2 did not receive history. Received:", msg)
        return False

    s1.close()
    s2.close()
    return True

if __name__ == "__main__":
    if test():
        print("TEST PASSED")
        sys.exit(0)
    else:
        print("TEST FAILED")
        sys.exit(1)
