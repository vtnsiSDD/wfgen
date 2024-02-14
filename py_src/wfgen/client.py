
import cmd
import zmq
import zmq.ssh
import argparse
import yaml
import subprocess
import shlex
import sys
from typing import List
import tempfile
import os
import numpy as np
import pprint

WG_CLIENT_API_VERSION = 0x01000000

try:
    from .utils import get_interface,paramify,MultiSocket,Ettus_USRP_container
    from .c_logger import logger_client,fake_log,logger as c_logger
    from . import profiles
except:
    ## fall back for direct exection
    from wfgen.utils import get_interface,paramify,MultiSocket,Ettus_USRP_container
    from wfgen import logger_client,fake_log,c_logger
    from wfgen import profiles


def line2start_radio(line):
    try:
        message_parts = shlex.split(line)
    except ValueError:
        raise ValueError("Couldn't parse the start_radio message (forget a qoute?)")
    try:
        dev_idx = int(message_parts[0])
    except ValueError:
        raise ValueError("Could not determine radio index from ({})".format(message_parts[0]))
    mode = message_parts[1]
    if mode not in ['static','bursty','hopper','replay']:
        print("hmmm, why is this happening again?",message_parts)
        profile = message_parts[3]
        params = paramify(message_parts[4:])
        params[message_parts[1]] = message_parts[2]
    else:
        profile = message_parts[2]
        params = paramify(message_parts[3:])
    return dev_idx, mode, profile, params

def line2run_random(line):
    try:
        message_parts = shlex.split(line)
    except ValueError:
        raise ValueError("Couldn't parse the run_random message (forget a quoute?)")
    if len(message_parts)%2 == 1:
        raise SyntaxError("run_random must be run in <field> <value> pairs, make sure no spaces after commas in the <value>")

    valid_fields = Client.get_run_random_fields()
    config = dict([(x,None) for x in valid_fields])
    if not all([x in valid_fields for x in message_parts[0::2]]):
        raise ValueError("unknown fields: run_random must be run in <field> <value> pairs, make sure no spaces after commas in the <value>")

    for idx in range(0,len(message_parts),2):
        f = message_parts[idx]
        v = message_parts[idx+1]
        if f == 'radios':
            radios = [int(x) for x in v.split(',')]
            # if any([x > len(self.radios) or x < 0 for x in radios]):
            #     return "invalid radio index has been provided"
            if config[f] is None:
                config[f] = radios
            else:
                config[f].extend(radios)
        elif f == 'profiles':
            sig_profiles = v.split(',')
            if any([not hasattr(profiles,x) for x in sig_profiles]):
                return "invalid profile has been provided valid({0!s})".format(profiles.get_all_profile_names())
            if config[f] is None:
                config[f] = sig_profiles
            else:
                config[f].extend(sig_profiles)
        else:
            if(len(v.split(',')) != 2):
                raise ValueError("{} value({}) doesn't match (low,high) format".format(f,v))
            req_items = [float(v.split(",")[0]),float(v.split(",")[1])]
            if f in ['bands','freq_limits','span_limits','bw_limits']:
                req_items = [int(x) for x in req_items]
            if config[f] is None:
                config[f] = [req_items]
            else:
                config[f].append(req_items)
    return config

def line2run_script(line):
    items = line.split(' ')
    item_count = len(items)
    if (item_count > 1) and (items[-2] == ''):
        items = items[:-1]
        item_count -= 1
    purge_items = []
    for idx,x in enumerate(items[:-1]):
        if x == '':
            purge_items.append(idx)
    for idx in reversed(purge_items):
        del items[idx]
        item_count -= 1
    seed = None
    for idx in range(1,len(items)-1):
        seed_value = None
        try:
            seed_value = int(items[idx])
        except:
            seed_value = None
        if seed_value is not None:
            if seed is None:
                seed = [seed_value]
            else:
                seed.append(seed_value)
        
    return seed, items[-1]

## Not sure needed yet
# def run_random2_line(params):
#     return ''

def stringify_list(l):
    s = yaml.dump(l).replace(" ","\ ").replace(chr(10),'\\\n')
    return s

def start_radio2line(dev_idx,mode,profile,params):
    split_line = [str(dev_idx),mode,profile]
    for k,v in params.items():
        if isinstance(v,(list)):
            split_line += [k,stringify_list(v)]
        else:
            split_line += [k,str(v)]
    return ' '.join(split_line)

def listdir(root):
    res = []
    for name in os.listdir(root):
        path = os.path.join(root,name)
        if os.path.isdir(path):
            name += os.sep
        res.append(name)
    return res

def complete_path(path=None):
    if not path:
        return listdir('.')
    dirname, remainder = os.path.split(path)
    # print(repr(dirname),repr(remainder))
    tmp = dirname if dirname else '.'
    res = [os.path.join(dirname,p) for p in listdir(tmp) if p.startswith(remainder)]
    # print(res)
    if len(res) > 1 or not os.path.exists(path):
        return res
    if os.path.isdir(path):
        return [os.path.join(path,p) for p in listdir(path)]
    return [path]


class radio_plan_tracker(object):
    def __init__(self,radio_idx,dev,runtime=float('inf')):
        self.radio_idx = radio_idx
        self.dev = dev
        self.runtime = runtime
        self.occupied = []
    def reserve(self,start,stop):
        self.occupied.append((start,stop))
    def available(self,start,stop):
        available = True
        for usage in self.occupied:
            if not ((start < usage[0] and stop < usage[0]) or (start > usage[1] and stop > usage[1])):
                available = False
                break
        return available


class Client(object):
    _not_connected = "Not connected"
    """
    Attempting to make the client a class in case persistance is needed

    :param addr: string of the IP address to connect to (defaults to local internet connection)
    :param port: the integer value of the port to connect to (defaults to 50000)
    :return: A client through which programs can be interfaced
    """
    def __init__(self,addrs:List[str]=[],ports:List[int]=[],ssh_tunnel:List[bool]=[],use_log=False):
        if(len(addrs) == 0):
            # Assuming a local connection only
            self.addrs = [get_interface()]
        else:
            self.addrs = [x for x in addrs]
        self.ports = ports
        self.conn = MultiSocket()
        self.sshs = ssh_tunnel
        self.tunnel = None
        self.radios = None
        self.server_there = False
        self.conn_addrs = [None]*len(self.addrs)
        for idx,(a,s) in enumerate(zip(self.addrs,self.sshs)):
            if(s == True):
                self.conn_addrs[idx] = '127.0.0.1'
            else:
                self.conn_addrs[idx] = a
        self.log_c = logger_client("Client") if use_log else fake_log("Client",cout=False)

    def is_connected(self):
        if len(self.conn) == len(self.addrs):
            if self.server_there:
                return True
            else:
                self.conn.setsockopt(zmq.REQ_RELAXED,1)
                self.conn.setsockopt(zmq.REQ_CORRELATE,1)
                self.conn.send_multipart(['ping'])## encoder moved internal
                if self.conn.poll(timeout=5000., event=zmq.POLLIN) != 0:#ms
                    if all([x[0] == "pong" for x in self.conn.recv_multipart()]):
                        self.conn.setsockopt(zmq.REQ_RELAXED,0)
                        self.conn.setsockopt(zmq.REQ_CORRELATE,0)
                        self.server_there = True
                        return True
                self.server_there = False
        return False

    def disconnect(self):
        if not self.is_connected():
            return
        self.conn.close()
        self.conn = MultiSocket()

    def connect(self):
        if self.is_connected():
            self.conn.close()
        
        self.tunnel = [None]*len(self.addrs)
        for idx,(a,p,s) in enumerate(zip(self.addrs,self.ports,self.sshs)):
            if(s == True):
                self.tunnel[idx] = zmq.ssh.openssh_tunnel(p,p,a,a,timeout=10)
        
        self.conn.connect(self.conn_addrs,self.ports)
        # if self.ssh:
        #     self.tunnel = zmq.ssh.openssh_tunnel(self.port,self.port,
        #         self.addr,self.addr,timeout=10) # tunnel closes after 10s inactivity
        #     self.conn.connect("tcp://127.0.0.1:{}".format(self.port))
        # else:
        #     self.conn.connect("tcp://{}:{}".format(self.addr,self.port))

    def get_help(self):
        if not self.is_connected():
            return self._not_connected
        # self.conn.send_multipart(encoder(["help","me"]))
        # reply = decoder(self.conn.recv_multipart())
        expected = self.conn.send_multipart(["help","me"])
        reply = self.conn.recv_multipart(expected)
        return "\n\t".join(reply[0])

    def get_radios(self):
        if not self.is_connected():
            return self._not_connected
        # self.conn.send_multipart(encoder(["get_radios",""]))
        # reply = decoder(self.conn.recv_multipart())
        expected = self.conn.send_multipart(["get_radios",""])
        reply = self.conn.recv_multipart(expected)
        # print("raw get_radios:")
        # print(repr(reply))
        return ["".join(x) for x in reply]

    def get_active(self):
        if not self.is_connected():
            return self._not_connected
        # self.conn.send_multipart(encoder(["get_active",""]))
        # reply = decoder(self.conn.recv_multipart())
        command = ["get_active",""]
        expect_nreplies = self.conn.send_multipart(command)
        reply = self.conn.recv_multipart(expect_nreplies)
        return "\n".join([" ".join(x) for x in reply])

    def get_finished(self):
        if not self.is_connected():
            return self._not_connected
        # self.conn.send_multipart(encoder(["get_finished",""]))
        # reply = decoder(self.conn.recv_multipart())
        command = ["get_finished",""]
        expect_nreplies = self.conn.send_multipart(command)
        reply = self.conn.recv_multipart(expect_nreplies)
        return "\n".join([" ".join(x) for x in reply])

    def start_radio_command(self, dev_idx, mode, profile, params):
        if not self.is_connected():
            return self._not_connected
        if self.radios is None:
            return "Get radios first!"
        if dev_idx >= len(self.radios) or dev_idx < 0:
            return "Device index provided is invalid ({}) valid are in the range (0,{})".format(dev_idx,len(self.radios))


        command_line = start_radio2line(dev_idx,mode,profile,params)
        print("src_cl:",repr(command_line))
        split_line = shlex.split(command_line)
        print("src_sl:",split_line)
        command = ["start_radio",mode,
                   '-a {0:s}'.format(self.radios.radios[dev_idx]['args'])] + split_line[2:]
        print("sending command:",command)
        # self.conn.send_multipart(encoder(command))
        # reply = decoder(self.conn.recv_multipart())
        expect_nreplies = self.conn.send_multipart(command)
        reply = self.conn.recv_multipart(expect_nreplies)
        return "\n".join([" ".join(x) for x in reply])

    def kill_id(self,pid):
        if not self.is_connected():
            return self._not_connected
        if self.radios is None:
            return "Get radios first!"
        command = ["kill","{0!s}".format(pid)]
        print("sending command:",command)
        # self.conn.send_multipart(encoder(command))
        # reply = decoder(self.conn.recv_multipart())
        nreplies = self.conn.send_multipart(command)
        reply = self.conn.recv_multipart(nreplies)
        return "\n".join([" ".join(x) for x in reply])

    def shutdown(self):
        if not self.is_connected():
            return self._not_connected
        # self.conn.send_multipart(encoder(["shutdown","now"]))
        # reply = decoder(self.conn.recv_multipart())
        nreply = self.conn.send_multipart(["shutdown","now"])
        reply = self.conn.recv_multipart(nreply)
        return "\n".join([" ".join(x) for x in reply])
    
    def collect_truth(self,outfile=None):
        if not self.is_connected():
            return self._not_connected
        nreply = self.conn.send_multipart(["get_truth",outfile])
        reply = self.conn.recv_multipart(nreply)
        file_count = 0
        with tempfile.TemporaryDirectory() as tmpdir:
            proto = 'truth_report_{0:03d}.json'
            for idx,sr in enumerate(reply):
                self.log_c.log(c_logger.level_t.INFO,f"{idx}, {sr[:2]}")
                if sr[1] != 'empty':
                    file_count += 1
                    with open(os.path.join(tmpdir,proto.format(idx)),'w') as fp:
                        fp.write(sr[2])
            if file_count:
                report_file = outfile if outfile is not None else 'report_of_truth.json'
                if os.path.isfile(report_file):
                    out_proto = ''.join([report_file[:-5] + '{0:03d}' + report_file[-5:]])
                    counter = 0
                    report_file = out_proto.format(counter)
                    while os.path.isfile(report_file):
                        counter += 1
                        report_file = out_proto.format(counter)
                cmd = 'consolidate_reports.py -output {0} "{1}"'.format(report_file,os.path.join(tmpdir,'truth*.json'))
                self.log_c.log(c_logger.level_t.INFO,"Running command:\n\t{0!s}".format(cmd))
                cmd = shlex.split(cmd)
                proc = subprocess.Popen(cmd,shell=False)
                proc.wait()
                outfile = report_file
            else:
                report_file = 'No valid non-empty reports came in'
        return report_file


    def _radio_cleanup(self):
        possible_radios = ['{0:s}'.format(x['args']) for x in self.radios.radios]
        possible_servers = [x for x in self.radios.radio_server_map]

        server_set = dict([(x,list()) for x in sorted(np.unique(possible_servers).tolist())])
        for radio_idx in range(len(self.radios)):
            server_set[possible_servers[radio_idx]].append(possible_radios[radio_idx])
        return server_set

    def run_script(self,file_location=None,seed=None):
        if not self.is_connected():
            return self._not_connected
        if self.radios is None:
            return "Get radios first!"
        from wfgen.run_modes import scripted
        if file_location is None:
            raise ValueError("No script path provided")
        try:
            runtime,energies,signals,sources,flags,toggles = scripted.load_script(file_location)
            if flags in [1,2,3]:
                num_radios_needed = scripted.needed_radios_for_script(file_location)
            else:
                num_radios_needed = -1
            if flags in [1,2]:
                og_profs = profiles.get_traceback_profiles(file_location)
            else:
                og_profs = {}
        except:
            import traceback
            traceback.print_exc()
            return "invalid script provided"
        
        ## Clean up sources to servers
        server_set = self._radio_cleanup()
        all_radios = []
        radio_servers = []
        for srvr in range(len(server_set)):
            all_radios.extend(server_set[srvr])
            radio_servers.extend([srvr for _ in server_set[srvr]])

        if len(all_radios) < num_radios_needed and num_radios_needed >= 0:
            self.log_c.log(c_logger.level_t.WARNING,"Fewer radios are available than needed, will drop signals")

        plan_tracking = [radio_plan_tracker(idx,dev) for idx,dev in enumerate(all_radios)]
        radio_mapping = dict([(dev,[]) for dev in all_radios])
        radio_failback = dict()

        ### seed maker
        if seed is not None:
            print("Seeding run with:",seed)
        rng = np.random.default_rng(seed)
        signal_lineup = [None]*len(signals)

        if flags in [1,2,3]:
            #### playing back a truth file
            signals = sorted(signals,key=lambda x: x['time_start'])
            src_id = [y['device_origin'] if len(y['device_origin']) else y['instance_name'] \
                      for x in signals for y in sources if x['instance_name'] in y['signal_set']]
            time_windows = [[x['time_start'],x['time_stop']] for x in signals]
            for idx,sig in enumerate(signals):
                src = src_id[idx]
                time_win = time_windows[idx]
                if sig['instance_name'] not in og_profs:
                    if flags not in [3]:
                        self.log_c.log(c_logger.level_t.WARNING,"Dropping signal at index {} because can't find a replay profile for it".format(idx))
                        signal_lineup[idx] = [None,None,None]
                        continue
                if flags in [1,2]:
                    prof_name = og_profs[sig['instance_name']]
                else:
                    prof_name = sig['mod_src_name']
                radio_idx = None
                if src in radio_mapping:
                    ### great I know what radio this should go to
                    dev = src
                    radio_idx = all_radios.index(dev)
                    if not plan_tracking[radio_idx].available(*time_win):
                        ### but the device is busy during this time
                        radio_idx = None
                        dev = None
                else:
                    ### this isn't my radio format
                    if src in radio_failback:
                        ### but we have seen it before
                        radio_idx,dev = radio_failback[src]
                        if not plan_tracking[radio_idx].available(*time_win):
                            ### but the device is busy during this time
                            radio_idx = None
                            dev = None
                    else:
                        ### haven't seen this radio yet
                        possible_radio_count = len(all_radios)
                        checked = 0
                        r_idx = rng.integers(possible_radio_count)
                        while not plan_tracking[r_idx].available(*time_win):
                            r_idx = (r_idx+1)%possible_radio_count
                            checked += 1
                            if checked == possible_radio_count:
                                #### No radios are free at this time
                                r_idx = None
                                break
                        if r_idx is not None:
                            ### a radio is available, so let's store it
                            radio_idx = r_idx
                            dev = all_radios[r_idx]
                            radio_failback[src] = (radio_idx,dev)
                if radio_idx is None and src not in all_radios:
                    ### This isn't a radio I actually know, so see if there's another radio that
                    #### can cover this
                    possible_radio_count = len(all_radios)
                    if src in radio_failback:
                        r_idx,_ = radio_failback[src]
                    else:
                        r_idx = rng.integers(possible_radio_count)
                    checked = 0
                    while not plan_tracking[r_idx].available(*time_win):
                        r_idx = (r_idx+1)%possible_radio_count
                        checked += 1
                        if checked == possible_radio_count:
                            #### No radios are free at this time
                            r_idx = None
                            break
                    if r_idx is not None:
                        ### a radio is available, so let's store it
                        radio_idx = r_idx
                        dev = all_radios[r_idx]
                        radio_failback[src] = (radio_idx,dev)
                if radio_idx is not None:
                    ### Alright there is a radio that can take this task
                    radio_mapping[dev].append({'time':time_win,'profile':prof_name,'signal_index':idx})
                    plan_tracking[radio_idx].reserve(*time_win)
                    sig['profile'] = prof_name
                    signal_lineup[idx] = [sig,{radio_servers[radio_idx]:[dev]},time_win]
                else:
                    self.log_c.log(c_logger.level_t.WARNING,"Dropping signal at index {}".format(idx))
                    signal_lineup[idx] = [None,None,None]
        else:
            ### creating a script from a config space
            for sig_idx,sig in enumerate(signals):
                radio_idx = []
                if 'device' in sig:
                    devices = sig['device']
                    if not isinstance(devices,list):
                        devices = [devices]
                    for dev in devices:
                        if isinstance(dev,str):
                            if dev in all_radios: # direct radio args str '<args>'
                                radio_idx.append(all_radios.index(dev))
                            elif any([dev in x for x in all_radios]): ### likely just a subset of '<args>'??
                                radio_idx.append([dev in x for x in all_radios].index(True))
                        if isinstance(dev,int) and dev in list(range(len(self.radios))):### specifying radio index directly
                            radio_idx.append(dev)
                send_to_server = dict()
                if radio_idx:
                    ### some set of radios were provided
                    for ri in radio_idx:
                        ### for the specific index if the unique server this radio is on
                        if radio_servers[ri] not in send_to_server:
                            ### is not in the server_mapping, add it and append the radio_dev
                            send_to_server[radio_servers[ri]] = [all_radios[ri]]
                        else:
                            ### else append radio_device to that server
                            send_to_server[radio_servers[ri]].append(all_radios[ri])
                else:
                    ### if not specified, then just send it to all radios
                    send_to_server = server_set.copy()
                tst = None if 'time_start' not in sig else sig['time_start']
                tsp = None if 'time_stop' not in sig else sig['time_stop']
                signal_lineup[sig_idx] = [sig,send_to_server,[tst,tsp]]# no time constraints

        ### signal_lineup[x] = [signal_dict, {server_idx:[radio_list]}, [time_start, time_stop]]


        ##### Now we have a setup of signals paired with all possible servers/radio combos
        #####   want servers/radios with all possible signals
        radio_structure = {"runtime":runtime,"flags":flags,"toggles":toggles} #runtime/flags/signal_limit extracted server side
        for sig,ss,timing in signal_lineup: ### iterate over all signals
            if sig is None: ## skip those that aren't defined
                continue
            for server_idx in ss.keys(): ### iterate over all servers this signal can go to
                radio_set = ss[server_idx] ### the set of radios on this server that can send the signal
                for radio_arg in radio_set:
                    if radio_arg not in radio_structure: ## add device args to radio_structure, add seed, add {sig:sig,timing:timeing}
                        radio_structure[radio_arg] = [rng.integers(np.iinfo(np.uint64).min,np.iinfo(np.int64).max,size=10).tolist(),{'signal':sig,'timing':timing}]
                    else: ## just append this {sig:sig, timing:timing}
                        radio_structure[radio_arg].append({'signal':sig,'timing':timing})

        ### radio_structure[radio_args] = [[seed list], {sig:sig, timinig:timinig}, ... ]

        command = ['run_script',yaml.dump(radio_structure)]
        self.conn.send_multipart(command)
        reply = self.conn.recv_multipart()
        return '\n'.join([' '.join(x) for x in reply])

    @staticmethod
    def get_run_random_fields():
        return ['bands','freq_limits','burst_idle_limits','burst_dwell_limits',
            'duration_limits','radios','span_limits','bw_limits','profiles',
            'gain_limits', 'digital_gain_limits', 'digital_cycle_limits']

    def run_random(self,rr_config):
        if not self.is_connected():
            return self._not_connected
        if self.radios is None:
            return "Get radios first!"
        radios = rr_config['radios']
        if any([x > len(self.radios) or x < 0 for x in radios]):
            return "invalid radio index has been provided"

        command = ['run_random',yaml.dump(rr_config)]
        self.conn.send_multipart(command)
        reply = self.conn.recv_multipart()
        return '\n'.join([' '.join(x) for x in reply])







class cli(cmd.Cmd,object):
    _REPLAY_PROFILES = profiles.get_replay_profile_names()
    _LINMOD_PROFILES = profiles.linmod_available_mods
    _LINMOD_OPTIONS = profiles.get_base_options('static')
    _LINMOD_HOPPER_OPTIONS = profiles.get_base_options('hopper')
    _FSKMOD_PROFILES = profiles.fskmod_available_mods
    _FSKMOD_OPTIONS = profiles.get_base_options('static','fsk')
    _FSKMOD_HOPPER_OPTIONS = profiles.get_base_options('hopper','fsk')
    _AFMOD_PROFILES = profiles.afmod_available_mods
    _AFMOD_OPTIONS = profiles.get_base_options('static','analog')
    # _AFMOD_HOPPER_OPTIONS = profiles.get_base_options('hopper','analog')
    _TONE_PROFILES = profiles.tones_available_mods
    _TONE_OPTIONS = profiles.get_base_options('static','tone')
    _TONE_HOPPER_OPTIONS = profiles.get_base_options('hopper','tone')
    _OFDM_PROFILES = profiles.ofdm_available_mods
    _OFDM_OPTIONS = profiles.get_base_options('ofdm')
    _OFDM_SUBMODS = profiles.ofdm_submods

    # _STATIC_OPTIONS = profiles.get_base_options('static')
    # _BURSTY_OPTIONS = profiles.get_base_options('bursty')
    # _HOPPER_OPTIONS = profiles.get_base_options('hopper')

    _AVAILABLE_PROFILES = ([] + _LINMOD_PROFILES
        + _FSKMOD_PROFILES
        + _AFMOD_PROFILES
        + _TONE_PROFILES
        + _OFDM_PROFILES
        + _REPLAY_PROFILES)
    def __init__(self,addrs, ports, use_sshs, verbose=True, dev=True, use_log=False):
        super(cli,self).__init__()
        self.client = Client(addrs, ports, use_sshs,use_log=use_log)
        self.radios = None
        self.verbose = verbose
        self.use_log = use_log #### not using this here at the moment
        class tweaked(str):
            def __len__(self):
                return 6
        self.prompt = tweaked('(\033[38;5;82mCmd\033[0m) ')
        # self.dev_mode = 100 if dev else dev
        self.dev_mode = dev
        self.intro = "".join([
            "\n** -------------------------------------------------------------------------- **\n",
            "** Command List\n",
            "** - help\n",
            "**     -- Show this help menu\n",
            "** - exit\n",
            "**     -- Closes the cli\n",
            "** - shutdown\n",
            "**     -- Closes both the cli and server\n",
            "** - get_radios\n",
            "**     -- The process of how the cli figures out radio numbers\n",
            "** - get_active\n",
            "**     -- List of tuples -- each tuple is an active process on the server\n",
            "** - get_finished\n",
            "**     -- List of tuples -- each tuple is an finished process on the server\n",
            "** - get_truth local_filename.json\n",
            "**     -- Saves all sent signals since start/last call into local_filename.json\n",
            "** - start_radio <device number> <profile type> <profile name> [options]\n",
            "**     start_radio 0 profile qpsk frequency 2.45e6\n",
            "**     -- This starts device 0 (with need args) at the specified frequency\n",
            "**     -- This start_radio command will be revised\n",
            "** - run_random [options]\n",
            "**     run_random radios 0,1 bands 2.4e9,2.5e9 duration_limits 2.0,5.0 \\\n",
            "**         bw_limits 50e3,1e6 profiles qpsk,fsk16\n",
            "**     -- This starts device 0,1 (with need args) with carrier in [2.4e9,2.5e9] Hz\n",
            "**        with signals of on time in range [2,5] seconds,\n",
            "**        with energy bandwidth in range [50e3,1e6] Hz\n",
            "**        with profiles qpsk and fsk16\n",
            "**     -- This run_random command will be revised\n",
            "** - run_script [seed int, ... ] <script.json filepath/ dir with main.json>\n",
            "**     run_script 1 2 3 4 5 configs\n",
            "**     run_script 1 2 3 4 5 configs/main.json\n",
            "**     run_script 1 2 3 4 5 truth_scripts/output-1a-truth.json\n",
            "**     -- This starts all devices queue by the script\n",
            "**     -- This run_script command will be revised\n",
            "** - kill <proc_id>\n",
            "**     -- Send kill signal to subprocess running a profile\n",
            "** -------------------------------------------------------------------------- **\n"
            ])

    def do_debug(self,line):
        print(self.client)
        if(self.radios is None):
            print("No radios yet")
        else:
            print(self.radios.debug())
        print("verbose:",self.verbose)
        print("raw_input:",self.use_rawinput)
        if self.use_rawinput:
            print("  old_completer:",self.old_completer)
        print("completekey:",self.completekey)

    def do_shutdown(self, line):
        if self.verbose:
            print(self.client.shutdown())
        else:
            self.client.shutdown()
        return True

    def do_get_radios(self, line):
        radio_info = self.client.get_radios() ## list of strings from each server
        if radio_info == self.client._not_connected:
            print(radio_info)
        self.radios = Ettus_USRP_container(radio_info,verbose=self.verbose)
        self.client.radios = self.radios

    def do_get_active(self,line):
        print(self.client.get_active())

    def do_get_finished(self,line):
        print(self.client.get_finished())

    def do_start_radio(self, line):
        if len(line.split()) <   3: # cuts 'start_radio' from the line
            print("invalid use of 'start_radio' command -> start_radio <radio idx> <profile type> <profile name> [<option> <value> ...]")
            print(line.split())
            return
        if line.split()[-1] == "help":
            print("Special help here hasn't been implemented yet")
            return
        items,item_count = self._get_items(line)
        if not hasattr(profiles,items[2]):
            print(items,items[2])
            print("Invalid profile -- check spelling?")
            return
        if 'debug' in line:
            items,item_count = self._get_items(line)
            print("Hmm I need to debug this waveform:",items[2])
            try:
                prof_peek = getattr(profiles,items[2])(silent=1)
                print(prof_peek.options)
            except AttributeError:
                print("--Don't think that's a valid profile")
            return
        try:
            dev_idx, mode, profile, params = line2start_radio(line)
        except Exception as e:
            return str(e)
        start_info = self.client.start_radio_command(dev_idx, mode, profile, params)
        if self.verbose:
            print("start_radio",line)
        print("reply:",start_info)

    def do_kill(self, line):
        to_kill = line.split()
        for k in to_kill:
            kill_info = self.client.kill_id(k)
            if self.verbose:
                print("kill",k)
            print("reply:",kill_info)

    def do_run_random(self,line):
        if line.split()[-1] == "help":
            print("Special help here hasn't been implemented yet")
            return
        ##### note: 'run_random' header is removed from the line
        rr_config = line2run_random(line)
        client_respone = self.client.run_random(rr_config)
        print(client_respone)

    def do_run_script(self,line):
        if len(line.split()):
            if line.split()[-1] == "help":
                print("Special help here hasn't been implemented yet")
                return

        ##### note: 'run_random' header is removed from the line
        seed, rs_config = line2run_script(line)
        client_respone = self.client.run_script(rs_config,seed=seed)
        print(client_respone)

    def do_get_truth(self,line):
        items,item_count = self._get_items(line)
        if item_count:
            if items[-1] in ['help','h']:
                print("Special help her hasn't been implemented yet")
        filename = [x for x in items if x.endswith('.json')]
        filename = filename[0] if len(filename) else None
        reply = self.client.collect_truth(filename)
        print('truth file:',reply)

    def do_exit(self, line):
        return True

    def do_q(self, line):
        return True

    def do_EOF(self, line):
        print()
        return True

    def do_help(self, line):
        print(self.intro)

    def _get_items(self,line):
        items = line.split(' ')
        item_count = len(items)
        if self.dev_mode>10:
            print("\nStart items:{},{}".format(item_count,items),end='')
        if (item_count > 1) and (items[-2] == ''):
            items = items[:-1]
            item_count -= 1
        purge_items = []
        for idx,x in enumerate(items[:-1]):
            if x == '':
                purge_items.append(idx)
        for idx in reversed(purge_items):
            del items[idx]
            item_count -= 1
        if self.dev_mode>10:
            print("\nFinish items:{},{}".format(item_count,items),end='')
        return items, item_count

    def complete_start_radio(self, text, line, begidx, endidx):
        #                              0           1             2              3          4         5    ...
        #'start_radio' command -> start_radio <radio idx> <profile type> <profile name> [<option> <value> ...]")
        items, item_count = self._get_items(line)
        parsing_mode = None
        available_modes = ['static','bursty','hopper','replay']
        initial_mode = None
        if(self.radios is None):
            print("\n  get_radios first")
            return (-1, "get_radios")
        if(len(self.radios) == 0):
            print("\n  no radios found!")
            return (-1, "no radios_found!")
        if item_count == 2:
            parsing_mode = 0 # trying to figure out a valid radio index
        elif item_count == 3:
            parsing_mode = 1 # trying to figure out a valid profile type
        else: # trying to figure out a valid profile name or it's options
            initial_mode = items[2]
            if initial_mode in available_modes:
                parsing_mode = available_modes.index(initial_mode) + 2
            else:
                print("Invalid 'mode'({0!s}) found in start_radio command; valid = {1!s}",initial_mode,available_modes)
                return []

        while True: # parse until done
            if parsing_mode == 0: #### Need to load a radio #
                matches = ['{}'.format(x).startswith(text) for x in range(len(self.radios))]
                # print(text,matches)
                if not any(matches):
                    if(len(text) > 0):
                        print("\n  invalid radio number")
                    ret = list(range(self.radios))
                    # print('radio_ret',ret)
                    return ret
                else:
                    ret = [str(idx) for idx,x in enumerate(matches) if x == True]
                    # print('radio_ret',ret)
                    return ret
            elif parsing_mode == 1: ### Need to load the modes
                matches = [x.startswith(text) and len(text)>0 for x in available_modes]
                if not any(matches):
                    if(len(text) > 0):
                        print("\n  invalid mode name")
                    return available_modes
                else:
                    return [available_modes[idx] for idx,x in enumerate(matches) if x == True]
            elif parsing_mode == 2: #### Spinning up a 'static' profile
                if item_count > 4:
                    #### hopefully a valid profile was selected, so time to figure out the
                    ####  args that need to be taken in
                    parsing_mode = 8
                    continue
                if initial_mode in ['static','replay']:
                    matches = [i for i in self._AVAILABLE_PROFILES if i.startswith(text)]
                elif initial_mode in ['bursty','hopper']:
                    ### constrained for now
                    matches = [i for i in self._LINMOD_PROFILES + self._FSKMOD_PROFILES + self._AFMOD_PROFILES + self._OFDM_PROFILES + [self._TONE_PROFILES[0]] if i.startswith(text)]
                else:
                    print("\n  Somehow have an initial mode that isn't tracked -- ?")
                    return []
                if(len(matches)==0):
                    print("\n  no valid profiles found(2)")
                    return []
                return matches
            elif parsing_mode == 3: #### Spinning up a 'bursty'
                if item_count > 4:
                    #### hopefully a valid profile was selected, so time to figure out the
                    ####  args that need to be taken in
                    parsing_mode = 8
                    continue
                if initial_mode in ['replay','static']:
                    matches = [i for i in self._AVAILABLE_PROFILES if i.startswith(text)]
                elif initial_mode in ['bursty','hopper']:
                    ### constrained for now
                    matches = [i for i in self._LINMOD_PROFILES + self._FSKMOD_PROFILES + self._AFMOD_PROFILES + self._OFDM_PROFILES + [self._TONE_PROFILES[0]] if i.startswith(text)]
                else:
                    print("\n  Somehow have an initial mode that isn't tracked -- ?")
                    return []
                if(len(matches)==0):
                    print("\n  no valid profiles found(3)")
                    return []
                return matches
            elif parsing_mode == 4: #### Spinning up a 'hopper'
                if item_count > 4:
                    #### hopefully a valid profile was selected, so time to figure out the
                    ####  args that need to be taken in
                    parsing_mode = 8
                    continue
                if initial_mode in ['replay','static']:
                    matches = [i for i in self._AVAILABLE_PROFILES if i.startswith(text)]
                elif initial_mode in ['bursty','hopper']:
                    ### constrained for now
                    matches = [i for i in self._LINMOD_PROFILES + self._FSKMOD_PROFILES + self._AFMOD_PROFILES + self._OFDM_PROFILES + [self._TONE_PROFILES[0]] if i.startswith(text)]
                else:
                    print("\n  Somehow have an initial mode that isn't tracked -- ?")
                    return []
                if(len(matches)==0):
                    print("\n  no valid profiles found(4)")
                    return []
                return matches
            elif parsing_mode == 5: #### Spinning up a 'replay'
                if item_count > 4:
                    #### hopefully a valid profile was selected, so time to figure out the
                    ####  args that need to be taken in
                    parsing_mode = 8
                    continue
                if initial_mode in ['replay','static']:
                    matches = [i for i in self._AVAILABLE_PROFILES if i.startswith(text)]
                elif initial_mode in ['bursty','hopper']:
                    ### constrained for now
                    matches = [i for i in self._LINMOD_PROFILES + self._FSKMOD_PROFILES + self._AFMOD_PROFILES + self._OFDM_PROFILES + [self._TONE_PROFILES[0]] if i.startswith(text)]
                else:
                    print("\n  Somehow have an initial mode that isn't tracked -- ?")
                    return []
                if(len(matches)==0):
                    print("\n  no valid profiles found(5)")
                    return []
                return matches
            elif parsing_mode == 8:
                return self.complete_profile_options(text,line,begidx,endidx)
            else:
                print("\nNot sure how we've gotten here")
                print(text, line, begidx, endidx, item_count, parsing_mode)
                return []

    def complete_run_random(self, text, line, begidx, endidx):
        items = line.split()
        item_count = len(items)
        print(item_count)
        if item_count == 1:
            return self.client.get_run_random_fields()
        if item_count % 2 == 0:
            if text == '':
                if items[-1] == 'profiles':
                    return profiles.get_all_profile_names()
                else:
                    return ["comma", "seperated", "values"]
            return [x for x in self.client.get_run_random_fields() if x.startswith(text)]
        else:
            if items[-2] == 'profiles':
                last_prof = text.split(',')[-1]
                return [x for x in profiles.get_all_profile_names() if x.startswith(last_prof)]
            elif text == '':
                return self.client.get_run_random_fields()
            else:
                return ["comma", "seprated", "values"]

    def complete_run_script(self, text, line, begidx, endidx):
        items,item_count = self._get_items(line)
        # print("\ncomplete_run_script:",repr(line),items)
        searching = items[-1]
        # print('+s+"{}"|'.format(searching))
        possibles = complete_path(searching)
        while len(possibles) == 1 and os.path.isdir(possibles[0]):
            searching = possibles[0]
            # print('+s+"{}"|'.format(searching))
            possibles = complete_path(searching)
        return possibles

    def complete_profile_options(self, text, line, begidx, endidx):
        #                              0           1             2              3           4        5    ...
        #'start_radio' command -> start_radio <radio idx> <profile type> <profile name> [<option> <value> ...]")
        items, item_count = self._get_items(line)
        profile_type = items[2]
        prof_name = items[3]

        if self.dev_mode:
            option_count = items[4:]
            option_labels = items[4::2]
            option_count = len(option_labels)
            print(option_count, option_labels)

        # first what mode are we in?
        active_options = items[4:]
        option_labels = active_options[::2]
        option_count = len(option_labels)


        if prof_name in self._AVAILABLE_PROFILES:
            prof_peek = getattr(profiles,prof_name)(silent=True)
            ### need options for the profile here
            if prof_name in self._LINMOD_PROFILES:
                option_helpers = prof_peek.get_options()#self._LINMOD_OPTIONS
                if profile_type in ["hopper","bursty"]:
                    option_helpers = prof_peek.get_options("hopper")
                if text == '':
                    if item_count % 2 == 0:
                        if items[-2] in ['debug','Debug','DEBUG']:
                            return []
                        print("Provide value")
                        return []
                    else:
                        return [i for i in option_helpers if i not in option_labels]
                elif len(text) > 0 and item_count % 2 == 0:
                    return [text]
                matches = [i for i in option_helpers if i.startswith(text)]
                if text in option_labels and option_labels.count(text) == 1 and option_labels[-1] == text:
                    ### I've matched, and might match more than one, need to keep me in there too
                    return matches
                if len(matches) == 1 and sum([o.startswith(text) for o in option_labels]) == 1:
                    return matches
                matches = [i for i in option_helpers if i.startswith(text) and i not in option_labels]
                return matches
            elif prof_name in self._FSKMOD_PROFILES:
                option_helpers = prof_peek.get_options()
                if profile_type in ["hopper","bursty"]:
                    option_helpers = prof_peek.get_options('hopper')
                if text == '':
                    if item_count % 2 == 0:
                        print   ("Provide value")
                        return []
                    else:
                        return [i for i in option_helpers if i not in option_labels]
                matches = [i for i in option_helpers if i.startswith(text)]
                if text in option_labels and option_labels.count(text) == 1 and option_labels[-1] == text:
                    ### I've matched, and might match more than one, need to keep me in there too
                    return matches
                if len(matches) == 1 and sum([o.startswith(text) for o in option_labels]) == 1:
                    return matches
                matches = [i for i in option_helpers if i.startswith(text) and i not in option_labels]
                return matches
            elif prof_name in self._AFMOD_PROFILES:
                option_helpers = prof_peek.get_options()
                if profile_type in ["hopper","bursty"]:
                    option_helpers = prof_peek.get_options('hopper')
                if text == '':
                    if item_count % 2 == 0:
                        print   ("Provide value")
                        return []
                    else:
                        return [i for i in option_helpers if i not in option_labels]
                matches = [i for i in option_helpers if i.startswith(text)]
                if text in option_labels and option_labels.count(text) == 1 and option_labels[-1] == text:
                    ### I've matched, and might match more than one, need to keep me in there too
                    return matches
                if len(matches) == 1 and sum([o.startswith(text) for o in option_labels]) == 1:
                    return matches
                matches = [i for i in option_helpers if i.startswith(text) and i not in option_labels]
                return matches
            elif prof_name in self._OFDM_PROFILES:
                option_helpers = self._OFDM_OPTIONS
                if profile_type not in ['static','replay','hopper','bursty']:
                    print("Invalid profile type to use this profile")
                    return []
                if text == '':
                    if item_count & 0x1 == 0:
                        print("Provide value")
                        return []
                    else:
                        # return [i for idx,i in enumerate(option_helpers) if i not in option_labels and prof_peek.option_flags[idx] is not None]
                        return [i for i in option_helpers if i not in option_labels]
                elif len(text) > 0 and item_count % 2 == 0:
                    return [text]
                matches = [i for idx,i in enumerate(option_helpers) if i.startswith(text) and prof_peek.option_flags[idx] is not None]
                if text in option_labels and option_labels.count(text) == 1 and option_labels[-1] == text:
                    ### I've matched, and might match more than one, need to keep me in there too
                    return matches
                if len(matches) == 1 and sum([o.startswith(text) for o in option_labels]) == 1:
                    return matches
                matches = [i for idx,i in enumerate(option_helpers) if i.startswith(text) and i not in option_labels and prof_peek.option_flags[idx] is not None]
                return matches
            elif prof_name in self._TONE_PROFILES:
                option_helpers = self._TONE_OPTIONS
                if profile_type not in ['static','replay','hopper','bursty']:
                    print("Invalid profile type to use this profile")
                    return []
                if text == '':
                    if item_count & 0x1 == 0:
                        print("Provide value")
                        return []
                    else:
                        return [i for idx,i in enumerate(option_helpers) if i not in option_labels and prof_peek.option_flags[idx] is not None]
                matches = [i for idx,i in enumerate(option_helpers) if i.startswith(text) and prof_peek.option_flags[idx] is not None]
                if text in option_labels and option_labels.count(text) == 1 and option_labels[-1] == text:
                    ### I've matched, and might match more than one, need to keep me in there too
                    return matches
                if len(matches) == 1 and sum([o.startswith(text) for o in option_labels]) == 1:
                    return matches
                matches = [i for idx,i in enumerate(option_helpers) if i.startswith(text) and i not in option_labels and prof_peek.option_flags[idx] is not None]
                return matches
            else:
                print("Don't know what options are available... but expected format:")
                return ['<option>',' ','<value>']
        else:
            print("Unknown profile, check spelling?")
            return []

    def preloop(self):
        self.client.connect()

    def postloop(self):
        self.client.disconnect()

    def emptyline(self):
        pass

    def cmdloop(self, intro=None):
        """Repeatedly issue a prompt, accept input, parse an initial prefix
        off the received input, and dispatch to action methods, passing them
        the remainder of the line as argument.

        """

        self.preloop()
        if self.use_rawinput and self.completekey:
            try:
                import readline
                self.old_completer = readline.get_completer()
                readline.set_completer(self.complete)
                readline.parse_and_bind(self.completekey+": complete")
            except ImportError:
                pass
        try:
            if intro is not None:
                self.intro = intro
            if self.intro:
                self.stdout.write(str(self.intro)+"\n")
            stop = None
            while not stop:
                if self.cmdqueue:
                    line = self.cmdqueue.pop(0)
                else:
                    if self.use_rawinput:
                        try:
                            line = input(self.prompt)
                        except EOFError:
                            line = 'EOF'
                        except KeyboardInterrupt:
                            print()
                            line = 'EOF'
                    else:
                        self.stdout.write(self.prompt)
                        self.stdout.flush()
                        line = self.stdin.readline()
                        if not len(line):
                            line = 'EOF'
                        else:
                            line = line.rstrip('\r\n')
                line = self.precmd(line)
                stop = self.onecmd(line)
                stop = self.postcmd(stop, line)
            self.postloop()
        finally:
            if self.use_rawinput and self.completekey:
                try:
                    import readline
                    readline.set_completer(self.old_completer)
                except ImportError:
                    pass

    def complete(self, text, state):
        """Return the next possible completion for 'text'.

        If a command has not been entered, then complete against command list.
        Otherwise try to call complete_<command> to get list of completions.
        """
        if self.dev_mode:
            print("--{0:d}--".format(state),end='',flush=True)
        import readline
        origline = readline.get_line_buffer()
        if state == 0:
            # print((origline,))
            line = origline.lstrip()
            stripped = len(origline) - len(line)
            begidx = readline.get_begidx() - stripped
            endidx = readline.get_endidx() - stripped
            if begidx>0:
                cmd, args, foo = self.parseline(line)
                if cmd == '':
                    if self.dev_mode:
                        print("+D+".format(cmd))
                    compfunc = self.completedefault
                else:
                    try:
                        compfunc = getattr(self, 'complete_' + cmd)
                    except AttributeError:
                        if self.dev_mode:
                            print("+{}+".format(cmd),end='')
                        compfunc = self.completedefault
                    if self.dev_mode:
                        print("+{}+".format(compfunc),end='',flush=True)
            else:
                if self.dev_mode:
                    print("=C=",end='',flush=True)
                compfunc = self.completenames
            # print("\nA",text,line,begix,endidx)
            # print("{}{}".format(self.prompt,origline),end='')
            if self.dev_mode:
                print("(({},{},{},{}))".format(state,line,begidx,endidx))
            try:
                self.completion_matches = compfunc(text, line, begidx, endidx)
            except Exception as e:
                if self.dev_mode:
                    print("Exception({0!s}) raised in ({1!s})".format(e,compfunc))
            if self.dev_mode>1:
                print("[len(match):{}]".format(len(self.completion_matches)))
            # print((state,text,line,begidx,endidx,self.completion_matches))
            # print("\nB",len(self.completion_matches),begidx,endidx)
            items,item_count = self._get_items(line)
            try:
                if isinstance(self.completion_matches,tuple):
                    if self.completion_matches[0] == -1:
                        # readline.insert_text("\r"+(" "*40)+"\r{}".format(self.completion_matches[1]))
                        print("{}{}".format(self.prompt,origline),end='',flush=True)
                        return None
                if len(self.completion_matches) > 1:
                    print('\n {}'.format(self.completion_matches))
                    print("{}{}".format(self.prompt,origline),end='',flush=True)
                    check_list = [str(x) for x in self.completion_matches]
                    starter = check_list[0]
                    match_len = len(starter)
                    check_idx = 1
                    # print('\n',starter,match_len)
                    while match_len > len(text) and check_idx < len(check_list):
                        checking = check_list[check_idx]
                        checked  = [x for x in range(min(len(starter),len(checking))) if checking[x] != starter[x]]
                        match_len = min(match_len,min(checked))
                        check_idx += 1
                        # print('\n',starter,checking,match_len,checked)
                    if self.dev_mode>1:
                        print("\n\\\\//",repr(text),repr(starter),repr(starter[:match_len]),repr(starter[len(text):match_len]))
                        print("\n{}{}".format(self.prompt,origline),end='',flush=True)
                    # if(starter[:match_len] == text):
                    #     return None
                    # elif (starter[:match_len].startswith(text)):
                    #     return starter[len(text):match_len]
                    if line.endswith(os.sep) and len(text)==0:
                        return None
                    if ('/' in starter or '-' in starter) and ('/' in items[-1] or '-' in items[-1]):
                        ridx = max(items[-1].rfind('/'),items[-1].rfind('-'))+1
                        return starter[ridx:match_len]
                    return starter[:match_len]
                # if self.dev_mode:
                #     # print("{}{}".format(self.prompt,origline),end='',flush=True)
                elif len(self.completion_matches)==0:
                    print("\n{}{}".format(self.prompt,origline),end='',flush=True)
                # print(self.completion_matches[0],type(self.completion_matches[0]),text,type(text))
                if self.completion_matches[0] == text:
                    return text+' '
                
                if self.dev_mode>1:
                    print("\n\\\\__//",repr(text),items,repr(self.completion_matches[0]))
                    print("\n{}{}".format(self.prompt,origline),end='',flush=True)
                if ('/' in self.completion_matches[0] or '-' in self.completion_matches[0]) \
                    and ('/' in items[-1] or '-' in items[-1]):
                    ridx = max(items[-1].rfind('/'),items[-1].rfind('-'))+1
                    return self.completion_matches[0][ridx:]
                return self.completion_matches[0]
            except IndexError:
                return None
        else:
            print("\n{}{}".format(self.prompt,origline),end='',flush=True)

        return None

def run(addr=[],port=50000,use_ssh=True,verbose=True,dev=False,use_log=False):
    c = cli(addr,port,use_ssh,verbose,dev,use_log=use_log)
    try:
        c.cmdloop()
    except KeyboardInterrupt:
        pass

def parse_args():
    p = argparse.ArgumentParser()
    # p.add_argument("--addr",default=[],type=str,action='append',help="Interface address to connect to. Defaults to internet connection for local testing.")
    # p.add_argument("--port",default=50000,type=int,help="Port to connect to (def: %(default)s)")
    # p.add_argument("--disable-ssh",action="store_true",help="Don't use ssh tunnel to talk to server.")
    p.add_argument("--conn",default=[],action='append',nargs=3,metavar=('addr','port','use_ssh'),
        help=("Unique interface address to connect to. Default: host<internet connection IP> 50000 False"))
    p.add_argument("--verbose",action="store_true",help="Should the cli spit out everything?")
    p.add_argument("--dev",action="store_true",help="Should the cli spit out dev messages?")
    p.add_argument("--log-server",action='store_true',help="Use if a log-server is active (meant for debugging)")
    args = p.parse_args()
    args.addrs,args.ports,args.sshs = zip(*args.conn)
    args.addrs = list(args.addrs)
    args.ports = list(args.ports)
    args.sshs = list(args.sshs)
    return args

def main():
    args = parse_args()
    run(args.addrs,args.ports,args.sshs,args.verbose,args.dev,use_log=args.log_server)

if __name__ == "__main__":
    main()
