

import numpy as np
import zmq
import inspect
import datetime
import ctypes
from typing import Union


class fake_log(object):
    def __init__(self, *args, **kwargs):
        self.cout = False
        if 'cout' in kwargs:
            self.cout = kwargs['cout']
    def log(self,*args):
        if self.cout:
            print(args)

class logger(object):
    class level_t(object):
        OFF = 0
        CRITICAL = 1
        ERROR = 2
        WARNING = 3
        INFO = 4
        DEBUG = 5
        @classmethod
        def level_map(cls,level):
            if(level == cls.OFF):
                return "OFF"
            elif(level == cls.CRITICAL):
                return "CRITICAL"
            elif(level == cls.ERROR):
                return "ERROR"
            elif(level == cls.WARNING):
                return "WARNING"
            elif(level == cls.INFO):
                return "INFO"
            elif(level == cls.DEBUG):
                return "DEBUG"
            return "INVALID"

class payload_t(object):
    CHAR=0
    UCHAR=1
    CHAR16=2
    CHAR32=3
    WCHAR=4
    @classmethod
    def get_payload_itemsize(cls,p_type:"payload_t"):
        if(p_type == cls.CHAR):
            return ctypes.sizeof(ctypes.c_char)
        elif(p_type == cls.UCHAR):
            return ctypes.sizeof(ctypes.c_char)
        elif(p_type == cls.CHAR16):
            return ctypes.sizeof(ctypes.c_int16)
        elif(p_type == cls.CHAR32):
            return ctypes.sizeof(ctypes.c_int32)
        elif(p_type == cls.WCHAR):
            return ctypes.sizeof(ctypes.c_wchar)
        return 1
    @classmethod
    def get_payload_encoding(cls,p_type:"payload_t"):
        if(p_type == cls.CHAR):
            return 'utf-8'
        elif(p_type == cls.UCHAR):
            return 'utf-8'
        elif(p_type == cls.CHAR16):
            return 'utf-16-le'
        elif(p_type == cls.CHAR32):
            return 'utf-32-le'
        elif(p_type == cls.WCHAR):
            return 'utf-16-le' if ctypes.sizeof(ctypes.c_wchar)==2 else 'utf-32-le'
        return 1

class logger_client(object):
    def __init__(self,tag:str=None, frontend_addr:str="tcp://127.0.0.1:40000"):
        import os
        if tag is None:
            calling_frame = inspect.stack()[1]
            if(calling_frame[0].f_locals==globals()):
                calling_method = calling_frame[0].f_locals["__name__"]
            else:
                calling_method = calling_frame[3]
            # calling_names = calling_frame[0].f_back.f_code.co_names
            try:
                caller_file = os.path.basename(inspect.getmodule(calling_frame[0]).__file__)
            except AttributeError:
                caller_file = "<cmd>.c_logger"
            self.header_tag = caller_file+":"+calling_method
        else:
            self.header_tag = tag
        self.header_tag += f"({os.getpid()})"
        self._z_frontend_addr = frontend_addr
        self._running = False
        self._z_context = None
        self._z_frontend_socket = None
        self.last_timestamp = None
        self.date_fmt = "%Y%m%d"
        self.timestamp_fmt = "%H:%M:%S.%f"
        self.timeout_count = 0
        self.connected = False
        self.enabled = True
        self._init_defaults()
        self.setup_network()

    def _init_defaults(self):
        self.specials = [
            "{color_reset}",
            "{header_tag_color}",
            "{header_tag}",
            "{date_color}",
            "{date}",
            "{timestamp_color}",
            "{timestamp}",
            "{level_color}",
            "{level}"
        ]
        self.format = "[{header_tag} :: {timestamp} ] {level:>8} - "

    def disable(self):
        self.enable = False
        self.shutdown_network()
    
    def setup_network(self):
        if self.connected or self.timeout_count >= 100:
            return
        self.attempting_setup = True
        if self._z_context is None:
            self._z_context = zmq.Context()
        if self._z_frontend_socket is None:
            self._z_frontend_socket = self._z_context.socket(zmq.REQ)
        self._z_frontend_socket.connect(self._z_frontend_addr)
        poller = zmq.Poller()
        poller.register(self._z_frontend_socket, zmq.POLLOUT)
        if poller.poll(timeout=0): ### should be true already
            self.connected = self.log(logger.level_t.INFO,"Connecting") # is someone actually there?
            self.attempting_setup = False
            return
        self.shutdown_network()
        self.attempting_setup = False

    def shutdown_network(self):
        if self._z_frontend_socket is not None:
            self._z_frontend_socket.close()
        if self._z_context is not None:
            self._z_context.term()
            self._z_context = None
        if self._z_frontend_socket is not None:
            self._z_frontend_socket = None

    
    def log(self,level_enum:"logger.level_t",message:Union[str,bytes]="",
            payload_type:"payload_t"=payload_t.CHAR,add_endl=True,print_local=False):
        if not self.enabled: return True
        self.last_timestamp = datetime.datetime.now().astimezone()
        scale = payload_t.get_payload_itemsize(payload_type)
        encoding = payload_t.get_payload_encoding(payload_type)

        all_specials = dict()
        all_specials["{color_reset}"] = "\033[m"
        all_specials["{header_tag_color}"] = "\033[m"
        all_specials["{header_tag}"] = self.header_tag
        all_specials["{date_color}"] = "\033[m"
        all_specials["{date}"] = self.last_timestamp.strftime(self.date_fmt)
        all_specials["{timestamp_color}"] = "\033[m"
        all_specials["{timestamp}"] = self.last_timestamp.strftime(self.timestamp_fmt)
        all_specials["{level_color}"] = "\033[m"
        all_specials["{level}"] = logger.level_t.level_map(level_enum)

        ## add_header
        formatter = dict()
        for spc in self.specials:
            if spc[:-1] in self.format:
                formatter[spc[1:-1]] = all_specials[spc]

        header = self.format.format(**formatter)

        msg = header + message
        if add_endl :
            msg += "\n"
        if(print_local):
            print(self.last_timestamp.timestamp(),msg,end='' if add_endl else '\n')
        if self.timeout_count>=3:
            self.shutdown_network()
            if not print_local:
                print(self.last_timestamp.timestamp(),msg,end='' if add_endl else '\n')
            self.timeout_count = 100
            return False

        if not self.connected and not self.attempting_setup: # not connected, but trying to connect now
            print("logger({0!s}-->{1!s}) is not connected\tRetrying...".format(self._z_frontend_addr,header))
            self.timeout_count += 1
            self.shutdown_network()
            self.setup_network()
            if not self.connected:
                print("logger reconnect failed...")
                return False
            print("logger reconnect success")

        payload_dtype = np.dtype([
            ('key','S',8),
            ('double',np.float64),
            ('payload_type',np.uint8),
            ('length',np.uint64),
            ('buffer','V',len(msg)*scale)])
        payload = np.array([
            ('saikou:\00',
             self.last_timestamp.timestamp(),
             np.uint8(payload_type),
             len(msg),
             msg.encode(encoding))
            ],dtype=payload_dtype)[0].tobytes()
        
        z_msg = zmq.Message(payload,False,False)
        # print("-- send:",z_msg)
        poller = zmq.Poller()
        poller.register(self._z_frontend_socket,zmq.POLLOUT | zmq.POLLIN)#should always be in POLLOUT state here
        sockets = dict(poller.poll(timeout=100))
        if sockets and sockets[self._z_frontend_socket] == zmq.POLLIN:
            #### odd
            print("odd state?",self._z_frontend_socket.recv())
            sockets = dict(poller.poll(timeout=100))
        if sockets and sockets[self._z_frontend_socket] == zmq.POLLOUT:
            self._z_frontend_socket.send(z_msg)
        else:
            print("logger is not in send state?\n  Reconnecting...")
            self.shutdown_network()
            self.timeout_count += 1
            self.connected = False
            self.setup_network()
            if not self.connected:
                print("failed to reset socket and reconnect")
                return False
            else:
                print("success")
                self._z_frontend_socket.send(z_msg)
        sockets = dict(poller.poll(timeout=100))
        if poller.poll(timeout=200):
            ## should reply instantly...
            z_msg = self._z_frontend_socket.recv()
        else:
            print("logger recv_reply timeout",self.header_tag)
            return False
        self.timeout_count = 0
        return True


if __name__ == "__main__":
    import time,sys
    if len(sys.argv) > 1:
        log_c = logger_client(None,sys.argv[1])
    else:
        log_c = logger_client()

    for log_level in range(1,6):
        level_name = logger.level_t.level_map(log_level)
        msg = "Logging some data at the level of {}".format(level_name)
        print("--Logging:",msg)
        log_c.log(log_level,msg)
        time.sleep(1.0)

    





