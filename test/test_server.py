
import time
import zmq
import multiprocessing as mp
import wfgen as wg

encoder = wg.encoder
decoder = wg.decoder
get_interface = wg.utils.get_interface

def get_client_conn(addr=None,port=50000):
    context = zmq.Context()
    conn = context.socket(zmq.REQ)
    conn.connect("tcp://{}:{}".format(get_interface() if addr is None else addr,port))
    return conn

def start_server():
    p = mp.Process(target=wg.server.run,args=(None,50000,None,56000,True),kwargs={"use_log":False})
    p.start()
    return p

def shutdown_server(p,conn):
    start = time.time()
    conn.send_multipart(encoder(["shutdown","now"]))
    while time.time()-start < 5.0:
        if not p.is_alive():
            break
    if p.is_alive():
        p.terminate()
        p.join()
        return 1
    p.join()
    return 0

def shutdown():
    p = start_server()
    conn = get_client_conn()
    return shutdown_server(p,conn)

def invalid():
    p = start_server()
    conn = get_client_conn()
    conn.send_multipart(encoder(["Fail","me"]))
    reply = decoder(conn.recv_multipart())
    print(reply)
    shutdown_server(p,conn)
    return reply

def invalid_kill():
    p = start_server()
    conn = get_client_conn()
    conn.send_multipart(encoder(["kill","250"]))
    reply = decoder(conn.recv_multipart())
    print(reply)
    shutdown_server(p,conn)
    return reply

def start():
    p = start_server()
    conn = get_client_conn()
    conn.send_multipart(encoder(["start_radio","__debug_test__"]))
    reply = decoder(conn.recv_multipart())
    print(reply)
    shutdown_server(p,conn)
    return reply

def kill():
    p = start_server()
    conn = get_client_conn()
    conn.send_multipart(encoder(["start_radio","__debug_test__"]))
    reply = decoder(conn.recv_multipart())
    conn.send_multipart(encoder(["kill","{}".format(reply[1])]))
    reply = decoder(conn.recv_multipart())
    shutdown_server(p,conn)
    return reply

def test_shutdown():
    assert shutdown() == 0

def test_invalid():
    reply = invalid()
    assert reply[0] == 'Invalid command:'
    assert reply[1] == 'Fail'

def test_invalid_kill():
    reply = invalid_kill()
    assert reply[0] == 'No'
    assert reply[1] == 'process'
    assert reply[2] == '250'
    assert len(reply) == 3

# def test_kill():
#     reply = kill()
#     assert reply[0] == 'No'
#     assert reply[1] == 'process'
#     assert reply[2] == '250'
#     assert len(reply) == 3

def test_start():
    reply = start()
    assert reply[0] == "Starting process"
    assert len(reply) == 2

def test_kill():
    reply = kill()
    assert reply[0] == "Killing"
    assert reply[1] == "process"
    assert len(reply) == 3
