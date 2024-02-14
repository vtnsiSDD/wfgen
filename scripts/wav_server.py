#!/usr/bin/env python3


import numpy as np
import zmq
import argparse
import signal
import yaml
import multiprocessing as mp
from multiprocessing import shared_memory,managers
mp.shared_memory = shared_memory
mp.managers = managers
from wfgen.profiles.analogs import AudioCombiner


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--bind-addr', default='127.0.10.10',type=str,help=('The IP address to bind the server to. (def: %(default)s)'))
    p.add_argument('--bind-port', default=0,type=int,help=('The TCP port to bind the server to. (def: %(default)s)'))
    p.add_argument('--workers', default=10, type=int, help=('The number of workers to have running for support (def: %(default)s)'))
    return p.parse_args()

def worker(ident,backend_endpoint,shared_mem,fs,nfiles):
    signal.signal(signal.SIGINT, signal.default_int_handler)
    context = zmq.Context()
    socket = context.socket(zmq.DEALER)
    socket.connect(backend_endpoint)
    socket.setsockopt(zmq.IDENTITY, "Worker-{0:03d}".format(ident).encode('utf-8'))
    identity = socket.getsockopt(zmq.IDENTITY).decode('utf-8')

    audio_array = np.frombuffer(shared_mem.buf,np.single).reshape(-1,2)

    running = False
    def sig_handler(*args,**kwargs):
        pass
        # print("{} ignoring ctrl+C".format(identity))
    print(identity,"starting...")
    socket.send_multipart([identity.encode("utf-8"),b"READY","{0}".format(ident).encode("utf-8")])

    signal.signal(signal.SIGINT, sig_handler)
    running = True
    poller = zmq.Poller()
    poller.register(socket,zmq.POLLIN)
    while running:
        socks = dict(poller.poll())
        if socket in socks:
            incoming = socket.recv_multipart()
            try:
                status, request = incoming[-2:]
                if status == b"EXIT":
                    print(identity, "exiting...")
                    running = False
                    continue
                req_dict = yaml.safe_load(request.decode('utf-8'))
                if any(['start' not in req_dict, 'len' not in req_dict]) and 'get' not in req_dict:
                    data = b'invalid request dict, needs "start" and "len" or "get"'
                else:
                    if 'get' in req_dict:
                        if req_dict['get'] in ['len', 'shape', 'file_count', 'sample_rate']:
                            if req_dict['get'] == 'len':
                                data = int(audio_array.shape[0]).to_bytes(8,'little')
                                # print(audio_array.shape[0])
                                # print(data)
                            elif req_dict['get'] == 'shape':
                                data = yaml.dump(list(audio_array.shape)).encode('utf-8')
                                # print(audio_array.shape)
                                # print(data)
                            elif req_dict['get'] == 'file_count':
                                data = int(nfiles).to_bytes(8,'little')
                                # print(nfiles)
                                # print(data)
                            elif req_dict['get'] == 'sample_rate':
                                data = int(fs).to_bytes(8,'little')
                                # print(fs)
                                # print(data)
                        else:
                            data = b'Unknown get request, expecting value to be in ["len","shape","file_count","sample_rate"]'
                    else:
                        if req_dict['start']+req_dict['len'] > audio_array.shape[0]:
                            chunk0 = audio_array[req_dict['start']:,:]
                            chunk1 = audio_array[:req_dict['len']-chunk0.shape[0],:]
                            data = np.concatenate([chunk0,chunk1],axis=0)
                        else:
                            data = audio_array[req_dict['start']:req_dict['start']+req_dict['len'],:]
                        data = data.tobytes()
            except Exception as e:
                import traceback
                tb = traceback.format_exc()
                print("Failed request:",request)
                data = 'Exception Raised during request processing\n{0}'.format(tb).encode('utf-8')
            reply = [b'',data]
            if len(incoming) > 2:
                reply = [incoming[0]] + reply
            socket.send_multipart(reply)

    socket.setsockopt(zmq.LINGER,0)
    socket.close()
    context.term()
    print(identity,"stopped.")



def main(addr=None,port=None,n_workers=None):
    if any([x is None for x in [addr,port,n_workers]]):
        args = parse_args()
        addr = args.bind_addr if addr is None else addr
        port = args.bind_port if port is None else port
        n_workers = args.workers if addr is None else n_workers

    if n_workers is None:
        n_workers = 10

    context = zmq.Context()
    frontend = context.socket(zmq.ROUTER)
    frontend.bind("tcp://{0}:{1}".format(addr,port))
    frontend_endpoint = frontend.getsockopt(zmq.LAST_ENDPOINT)
    backend = context.socket(zmq.ROUTER)
    backend.bind("tcp://127.5.4.2:0")
    backend_endpoint = backend.getsockopt(zmq.LAST_ENDPOINT)

    print("Frontend:",frontend_endpoint)
    print("Backend:", backend_endpoint)

    audio_source = AudioCombiner()
    with mp.managers.SharedMemoryManager() as smm:
        shared_mem = smm.SharedMemory(2*len(audio_source)*4) # assumes stereo float32 samples
        arr = np.frombuffer(shared_mem.buf, dtype=np.single).reshape(-1,2)
        arr[:] = audio_source.get_numpy()

        worker_procs = [None]*n_workers
        for idx in range(n_workers):
            print("Spinning up worker:",idx)
            proc = mp.Process(target=worker,args=(idx,backend_endpoint,shared_mem,audio_source.sample_rate,audio_source.file_count))
            proc.start()
            worker_procs[idx] = [None,proc]

        running = False
        def sig_handler(*args,**kwargs):
            nonlocal running
            print("\nWav Server caught ctrl+C, shutting down...")
            running = False
            signal.signal(signal.SIGINT,signal.default_int_handler)

        signal.signal(signal.SIGINT,sig_handler)
        running = True

        poller = zmq.Poller()
        poller.register(backend,zmq.POLLIN)
        frontend_ready = False

        workers_ready = []
        while running:
            sockets = dict(poller.poll(1.0))
            if backend in sockets:
                # print("backend in sockets")
                reply = backend.recv_multipart()
                wrkr = reply[0]
                reply = reply[1:]
                name, status, response = reply[-3:]
                if status == b"READY":
                    idx = int(response.decode("utf-8"))
                    print(name.decode("utf-8"),"at",idx,"is ready")
                    worker_procs[idx][0] = [name,wrkr]
                    workers_ready.append(wrkr)
                    continue
                else:
                    # print("Hmm, backend message:",reply)
                    workers_ready.append(wrkr)
                    frontend.send_multipart(reply)
            if not any([x is None for x,_ in worker_procs]) and not frontend_ready:
                print("Frontend Ready")
                poller.register(frontend,zmq.POLLIN)
                frontend_ready = True
            if workers_ready and frontend in sockets:
                request = frontend.recv_multipart()
                # print('frontend request:',request)
                backend.send_multipart([workers_ready[0],]+request)
                del workers_ready[0]

        for (name,client),proc in worker_procs:
            backend.send_multipart([client,b"EXIT",name])
            proc.join()

        del arr ## needed in order to dealloc the shared_mem


    frontend.setsockopt(zmq.LINGER,0)
    backend.setsockopt(zmq.LINGER,0)
    frontend.close()
    backend.close()
    context.term()


if __name__ == '__main__':
    main()
