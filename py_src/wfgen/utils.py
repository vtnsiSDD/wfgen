
import socket
import subprocess
from typing import List
import zmq
import time
import yaml
import numpy as np
import json
import os

def have_pygr():
    try:
        import pmt
        from gnuradio import gr
        return True
    except ImportError:
        return False


def get_dns(force_ipv4=True):
    """
    Get the IP address of the current DNS server

    :return: IP Address of Current DNS Server
    :rtype: str
    """
    info = subprocess.Popen(['systemd-resolve --status'],stdout=subprocess.PIPE,shell=True)
    info_lines = info.stdout.read().decode("utf-8").splitlines()
    search_for = "Current DNS Server"
    search_extend = ""
    for ln in info_lines:
        if search_for in ln:
            if not force_ipv4:
                return ln.strip().split()[-1]
            else:
                if ":" in ln:
                    if search_for != "DNS Servers":
                        search_for = "DNS Servers"
                        search_extend = "DNS Domain"
                    else:
                        checker = ln.strip().split()[-1]
                        if ":" not in checker:
                            return checker
                else:
                    return ln.strip().split()[-1]
        if search_for == "DNS Servers":
            if ":" not in ln:
                return ln.strip().split()[-1]
        if len(search_extend) and search_extend in ln:
            break
    return "8.8.8.8"

def get_interface():
    """
    Get the IP Address of the internet facing connection

    :return: IP Address of internet facing interface
    :rtype: str
    """
    dns = get_dns()
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((dns,80))
    except Exception as e:
        import traceback as tb
        exc = tb.format_exc()
        print(dns)
        print(exc)
        raise e
    return s.getsockname()[0]


def encoder(x: List[str]):
    """
    Take a list of strings and encode them into utf-8 format

    :param x: list/tuple of strings
    :raise TypeError: if x is not iterable
    :raise AttributeError: if the contents of x are not strings
    :return: UTF-8 encoded strings
    :rtype: list[bytes]
    """
    return ["{0!s}".format(_).encode('utf-8') for _ in x]

def decoder(x: List[bytes]):
    """
    Take a list of utf-8 bytes and decode them into strings

    :param x: list/tuple of utf-8 bytes
    :raise TypeError: if x is not iterable
    :raise AttributeError: if the contents of x are not bytes
    :return: list of strings
    :rtype: list[str]
    """
    return [_.decode('utf-8') for _ in x]

def paramify(x: List[str]):
    print(x)
    params=dict()
    for idx in range(0,len(x),2):
        key = x[idx]
        value = x[idx+1]
        print(key,value)
        if isinstance(value,str) and value[0] == '-':
            try:
                new_value = yaml.safe_load(value)
                value = new_value
            except:
                pass
        if key in params:
            if not isinstance(params[key],list):
                params[key] = [params[key],value]
            else:
                params[key].append(value)
        else:
            params[key] = value
    print(params)
    return params

class MultiSocket:
    def __init__(self):
        self.ctx = None
        self.addrs = None
        self.sockets = None
        self.poller = None

    def __len__(self):
        return 0 if self.sockets is None else len(self.sockets)

    def _not_connected(self):
        raise RuntimeError("Not connected")

    def connect(self,addrs:List[str],ports:List[int]):
        if(not (isinstance(addrs,List) and isinstance(ports,List))):
            raise ValueError("address and port are not both lists")
        if(len(addrs) != len(ports)):
            raise ValueError("address and port list lengths don't match")
        self.ctx = zmq.Context()
        self.addrs = ['tcp://{}:{}'.format(x,y) for x,y in zip(addrs,ports)]
        self.sockets = [self.ctx.socket(zmq.REQ) for server in self.addrs]
        self.poller = zmq.Poller()
        for sock,addr in zip(self.sockets,self.addrs):
            sock.connect(addr)
            self.poller.register(sock,zmq.POLLIN)

    def disconnect(self):
        if self.sockets is None:
            return
        for sock in self.sockets:
            self.poller.unregister(sock)
            sock.close()
        self.ctx.term()
        self.sockets = None
        self.ctx = None
        self.addrs = None
        self.poller = None

    def close(self):
        self.disconnect()

    def setsockopt(self,flag,value):
        if self.sockets is None:
            self._not_connected()
        for sock in self.sockets:
            sock.setsockopt(flag,value)

    def send_multipart(self,multipart_message):
        if(self.sockets is None):
            self._not_connected()
        expected = 0
        for sock in self.sockets:
            sock.send_multipart(encoder(multipart_message))
            expected += 1
        return expected

    def recv_multipart(self,expected:int=None):
        if(self.sockets is None):
            self._not_connected()
        expected = len(self.sockets) if expected is None else expected
        status = self.poll(expected=expected,mode=1)
        reply = [None,]*len(self.sockets)
        for idx,(sock,event) in enumerate(status):
            if(event & zmq.POLLIN):
                reply[idx] = decoder(sock.recv_multipart())
        return reply

    def poll(self,timeout=0,event=zmq.POLLIN,mode:int = 0,expected:int = None):#ms
        ''' Poll all of the connections with servers for events

        args:
            timeout: if > 0 is the time in ms to wait before returning
                else wait until event&mode combination is met
            event: zmq.POLLIN and/or zmq.POLLOUT
            mode: 0 - any server responded
                  1 - all servers responded
            expected: if mode is 1, and not waiting on all sockets, then wait for
                expected amount of sockets instead

        returns: List[(sock,status),...] of length len(self.sockets)

        '''
        if self.poller is None:
            self._not_connected()
        for sock in self.sockets:
            self.poller.modify(sock,event)
        expected = (len(self.sockets) if expected is None else expected) if mode else 1
        starter = ender = time.time()*1000 #ms
        elapsed = ender - starter
        status = [(sock,0) for sock in self.sockets]
        while(expected > 0 and (timeout <= 0. or (timeout > 0. and elapsed < timeout))):
            poll_status = self.poller.poll(timeout=timeout-(ender-starter) if timeout>0. else 0.)
            for sk,st in poll_status:
                idx = self.sockets.index(sk)
                if(status[idx][1] == 0):
                    expected = expected - 1
                status[idx] = (sk,st)
            ender = time.time()*1000 #ms
            elapsed = ender - starter
        return status


class Ettus_USRP_container(object):
    b200_sample_bounds = [220e3,61.44e6]
    e3xx_sample_bounds = [220e3,61.44e6]
    def __init__(self,radio_info, verbose=False):
        if "No devices found" in radio_info:
            self.radio_info = []
        else:
            self.radio_info = radio_info ## list of strings
        self.radios = []
        self.verbose = verbose
        self._radio_pointer = [list() for _ in self.radio_info]
        self._filter_info()
        if self.verbose:
            print(self)

    def __len__(self):
        return self.radio_count

    def _filter_info(self):
        self.radio_count = 0
        radio_start = 0
        server_idx = -1
        self.server_map = [None,]*len(self.radio_info)
        for servers_info in self.radio_info:
            radio_start = self.radio_count
            server_idx += 1
            radio_lines = servers_info.splitlines()
            parse_lines = []
            pstart = None
            pend = None
            local_radio_count = 0
            for idx,ln in enumerate(radio_lines):
                if "UHD Device" in ln:
                    local_radio_count += 1
                if "Device Address" in ln:
                    pstart = idx
                if len(ln) == 0 and pstart is not None and pend is None:
                    pend = idx
                if pstart is not None and pend is not None:
                    parse_lines.append((pstart,pend))
                    pstart = None
                    pend = None
            self.radio_count += local_radio_count
            self.server_map[server_idx] = (radio_start,radio_start+local_radio_count,local_radio_count)
            self.radios += [None]*local_radio_count
            for idx,parse_pair in enumerate(parse_lines):
                snippet = "\n".join(radio_lines[parse_pair[0]:parse_pair[1]])
                radio_dict = yaml.safe_load(snippet)
                args = ''
                if 'type' in radio_dict["Device Address"]:
                    args += 'type={0:s}'.format(radio_dict["Device Address"]['type'])
                if 'addr' in radio_dict["Device Address"]:
                    if len(args):
                        args += ','
                    args+="addr={0:s}".format(radio_dict["Device Address"]["addr"])
                if 'serial' in radio_dict["Device Address"]:
                    if len(args):
                        args += ','
                    args+="serial={0:s}".format(radio_dict["Device Address"]["serial"])
                if 'mgmt_addr' in radio_dict["Device Address"]:
                    if len(args):
                        args += ','
                    args+="mgmt_addr={0:s}".format(radio_dict["Device Address"]["mgmt_addr"])
                radio_dict['args'] = args
                self.radios[radio_start+idx] = radio_dict
        self.radio_server_map = [None]*len(self.radios)
        for server_idx in range(len(self.server_map)):
            rstart,rend,rlen = self.server_map[server_idx]
            for idx in range(rlen):
                self.radio_server_map[rstart+idx] = server_idx


    def __str__(self):
        s = "Radio List ({}):\n".format(self.radio_count)
        for idx,r in enumerate(self.radios):
            s += "  {0:>4d} server={2:<3d} args={1!s}\n".format(idx,repr(r['args']),self.radio_server_map[idx])
        return(s)

    def debug(self):
        s = str(self)
        s += "Radio Bounds per Server:\n"
        for idx,r in enumerate(self.server_map):
            s+= "  {0:>4d} : {1!s}\n".format(idx,r)
        s += 'Radio to Server Map:\n'
        for idx,r in enumerate(self.radio_server_map):
            s+= "  {0:>4d} : {1!s}\n".format(idx,r)
        return s

    def get_index_from_serial(self,serial):
        for idx,r in enumerate(self.radios):
            if serial in r['args']:
                return idx
        return -1


def dat_to_wav(dat_file):
    data = np.fromfile(dat_file,np.float32)
    scaler = 32767/np.max(np.abs(data))
    data = (data*scaler).astype(np.int16)
    data = data.tobytes()
    wav = (b'RIFF'
           + (len(data)+36).to_bytes(4,'little')
           + b'WAVEfmt '
           + (16).to_bytes(4,'little')
           + (1).to_bytes(2,'little')
           + (2).to_bytes(2,'little')
           + (48000).to_bytes(4,'little')
           + (48000*2*16//8).to_bytes(4,'little')
           + (4).to_bytes(2,'little')
           + (16) .to_bytes(2,'little')
           + b'data'
           + (len(data)*2).to_bytes(4,'little')
           + data)
    return wav


def wav_disect(wav_file):
    data = np.fromfile(wav_file,np.uint8)
    segments = []
    splits = []
    repeat = False
    segments.append(data[ 0: 4].view("|S4")[0])
    splits.append(( 0, 4))
    segments.append(data[ 4: 8].view(np.uint32)[0])
    splits.append(( 4, 8))
    segments.append(data[ 8:12].view("|S4")[0])
    splits.append(( 8,12))
    segments.append(data[12:16].view("|S4")[0])
    splits.append((12,16))
    segments.append(data[16:20].view(np.uint32)[0])
    splits.append((16,20))
    segments.append(data[20:22].view(np.uint16)[0])
    splits.append((20,22))
    segments.append(data[22:24].view(np.uint16)[0])
    splits.append((22,24))
    segments.append(data[24:28].view(np.uint32)[0])
    splits.append((24,28))
    segments.append(data[28:32].view(np.uint32)[0])
    splits.append((28,32))
    segments.append(data[32:34].view(np.uint16)[0])
    splits.append((32,34))
    segments.append(data[34:36].view(np.uint16)[0])
    splits.append((34,36))
    segments.append(data[36:40].view("|S4")[0])
    splits.append((36,40))
    offset = 40
    repeat = segments[-1] != b'data'
    while repeat:
        ##### what's the field?
        print(segments[-1])
        ##### what's the byte length of the field
        data_len = data[offset:offset+4].view(np.uint32)[0]
        segments.append(data_len)
        splits.append((offset,offset+4))
        offset += 4
        ##### cache the field
        segments.append(data[offset:offset+data_len])
        splits.append((offset,offset+data_len))
        offset += data_len
        # next field?
        segments.append(data[offset:offset+4].view('|S4')[0])
        splits.append((offset,offset+4))
        offset += 4
        repeat = b'data' != segments[-1]
    segments.append(data[offset:offset+4].view(np.uint32)[0])
    splits.append((offset,offset+4))
    print(segments)
    return segments,splits,data


if have_pygr:
    import pmt
    from gnuradio import gr
    def parse_pmt_header(serial):
        byte_offset = 0
        header_magic = np.frombuffer(serial[byte_offset:byte_offset+2],np.uint16)[0]
        byte_offset += 2
        header_version = np.frombuffer(serial[byte_offset:byte_offset+1],np.uint8)[0]
        if header_magic != 24560:
            raise RuntimeError("Hmm... header magic failed")
        if header_version != 1:
            raise RuntimeError("Hmm... header version isn't 1")
        byte_offset += 1
        offset_out = np.frombuffer(serial[byte_offset:byte_offset+8],np.uint64)[0]
        byte_offset += 8
        rcvd_tags = np.frombuffer(serial[byte_offset:byte_offset+8],np.uint64)[0]
        byte_offset += 8
        if rcvd_tags > 0:
            tags = [None]*rcvd_tags
            for idx in range(rcvd_tags):
                tag_offset = np.frombuffer(serial[byte_offset:byte_offset+8],np.uint64)[0]
                byte_offset += 8
                key = pmt.deserialize_str(serial[byte_offset:])
                byte_len = len(pmt.serialize_str(key))
                byte_offset += byte_len
                value = pmt.deserialize_str(serial[byte_offset:])
                byte_len = len(pmt.serialize_str(value))
                byte_offset += byte_len
                srcid = pmt.deserialize_str(serial[byte_offset:])
                byte_len = len(pmt.serialize_str(srcid))
                byte_offset += byte_len
                tags[idx] = gr.tag_t()
                tags[idx].offset = tag_offset
                tags[idx].key = key
                tags[idx].value = value
                tags[idx].srcid = srcid
        else:
            tags = []
        return byte_offset, offset_out, tags


    def gen_pmt_header(offset,tags):
        header_magic = np.int16(24560).tobytes()
        header_version = np.int8(1).tobytes()
        offset_ = np.uint64(offset).tobytes()
        ntags = np.uint64(len(tags)).tobytes()

        header_bytes = header_magic+header_version+offset_ + ntags

        for tag in tags:
            # print(tag.offset,tag.key,tag.value,tag.srcid)
            toff = np.uint64(tag.offset).tobytes()
            tkey = pmt.serialize_str(tag.key)
            tvalue = pmt.serialize_str(tag.value)
            tsrc = pmt.serialize_str(tag.srcid)
            # print(type(header_bytes),type(toff),type(tkey),type(tvalue),type(tsrc))
            header_bytes += (toff + tkey + tvalue + tsrc)

        return header_bytes
