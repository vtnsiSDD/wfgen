
import os
import zmq
import random
import argparse
from typing import List,Dict,Union,Tuple
import subprocess
import shlex
import signal
import datetime
import yaml
import multiprocessing as mp
import glob
import json
import time

from queue import deque
from pynet import ServerNetworking,BetterConnection,ConnectionRole as CR,BetterThread

WG_SERVER_API_VERSION = 0x02000000
sp_failback = -1


try:
    from .launcher import launch
    from .utils import get_interface,encoder,decoder,Ettus_USRP_container
    from .c_logger import logger_client,fake_log,logger as c_logger
except ImportError:
    ### fall back for direct execution
    from wfgen.launcher import launch
    from wfgen.utils import get_interface,encoder,decoder,Ettus_USRP_container
    from wfgen import logger_client,fake_log,c_logger



def shutdown():
    print("Shutting down...")

class ServerRequests(object):
    def __init__(self, who_from, what_do):
        self.requester = who_from
        self.action = what_do if isinstance(what_do,list) else [what_do]
    def get_message(self):
        return self.action
    def get_dest(self):
        return self.requester
    def get_payload(self):
        return [self.requester] + self.action
    def __str__(self):
        return f'<{repr(self.requester)} : {self.action}>'
    def __repr__(self):
        return self.__str__()

class ServerNet(ServerNetworking):
    def __init__(self,filepath):
        if not os.path.exists(filepath):
            raise RuntimeError(f"network config file does not exist: {filepath}")
        with open(filepath,'r') as fp:
            config = json.load(fp)
        addr = config['server_addr']
        port = config['server_port']
        endpoint = f"{addr}:{port}"
        self.request_queue = deque()
        super().__init__(endpoint=endpoint)
        self.set_callback(self.handle_tasks)

    def handle_tasks(self,payload):
        if not isinstance(payload,tuple):
            print(f"Unexpected type: {type(payload)}, ...dropping",flush=True)
        sid,msg = payload[0],payload[1]
        print("Wrapping Request:",sid,msg,flush=True)
        request = ServerRequests(sid,decoder(msg))
        self.request_queue.append(request)

    def send_reply(self, reply_req:ServerRequests):
        if self.listener is not None:
            if self.listener.is_stopped():
                print("BG Stopped:",self.listener.reason,flush=True)
                raise RuntimeError("Network issues...")
            msg = reply_req.get_payload()
            try:
                # print("SENDING:",msg[0],msg[1:])
                self.reply(msg[0],msg[1:])
            except Exception as e:
                import traceback
                self.stop("\n".join(["-MAIN THREAD ERROR-",traceback.format_exc()]))
                raise RuntimeError("Replying issues...")
            return True
        return None

class ServerState(object):
    def __init__(self, root_dir, interface, uhd_args, log_template, debug, use_log=True):
        self.radios         = None
        self.root           = root_dir
        self.save_dir       = None
        self.network        = interface
        self.uhd_args       = uhd_args
        self.json_proto     = log_template
        self.burst_instance = 0
        self.idle_radios    = []
        self.active_radios  = []
        self.debug          = debug
        self.log_c          = logger_client("ServerState") if use_log else fake_log("ServerState",cout=False)
        self.make_root()
    def make_root(self):
        truth_folder = os.path.join(self.root,datetime.datetime.now().strftime('%Y%m%d%H%M%S') + "_truth")
        if not os.path.isdir(truth_folder) and not self.debug:
            os.makedirs(truth_folder)
        self.save_dir = truth_folder
    def consolidate(self):
        report_file = os.path.join(self.save_dir,'report_of_truth.json')
        if os.path.isfile(report_file):
            os.remove(report_file)
        cmd = 'consolidate_reports.py -output {0} "{1}"'.format(report_file,os.path.join(self.save_dir,'truth*.json'))
        self.log_c.log(c_logger.level_t.INFO,"Running command:\n\t{0!s}".format(cmd))
        cmd = shlex.split(cmd)
        proc = subprocess.Popen(cmd,shell=False)
        proc.wait()
    def activate_radios(self,radios:List=[]):
        if any([x in self.active_radios for x in radios]):
            raise RuntimeError("Requesting use of a radio already activated")
        if any([x not in self.idle_radios for x in radios]):
            raise RuntimeError("Requesting use of a radio({}) not under my purview({!s})".format(radios,self.idle_radios))
        for x in radios:
            del self.idle_radios[self.idle_radios.index(x)]
            self.active_radios.append(x)
    def deactivate_radios(self,radios:List=[]):
        if any([x not in self.active_radios for x in radios]):
            raise RuntimeError("Requesting to return a radio not under my purview")
        for x in radios:
            del self.active_radios[self.active_radios.index(x)]
            self.idle_radios.append(x)
    def set_radios(self,radio_info:str):
        self.radios = Ettus_USRP_container([radio_info],verbose=False)
        ##### being lazy here -- I'm assuming that if it's empty then I can fill them out
        #####  otherwise just ignore repeated requests
        if (len(self.idle_radios) == 0 and len(self.active_radios) == 0) or (len(self.idle_radios) != len(self.active_radios)):
            self.idle_radios = list(range(len(self.radios)))
    def __str__(self):
        str_out = "Remote-Listener Server State:\n"
        str_out += "  Saving to: {}\n".format(self.root)
        str_out += str(self.radios)
        str_out += '\nIdle: {}'.format(self.idle_radios)
        str_out += '\nActive: {}'.format(self.active_radios)
        return str_out
    def get_json_filename(self,dev_serial:str):
        filename = os.path.join(self.save_dir,self.json_proto.format(serial=dev_serial,instance=self.burst_instance))
        self.burst_instance += 1
        return filename
    def increment_instance(self,count:int):
        self.burst_instance += count
    def is_active(self,radio_idx:int):
        if radio_idx in self.active_radios:
            return True
        return False
    def is_idle(self,radio_idx:int):
        if radio_idx in self.idle_radios:
            return True
        return False
    def get_truth_file(self):
        report = os.path.join(self.save_dir,'report_of_truth.json')
        if os.path.isfile(report):
            return report
        return None


command_list=["help","ping","get_radios","get_active","get_finished",
              "start_radio","run_random","kill","shutdown","get_truth",
              "run_script"]
def run(network_interface:ServerNet,
        uhd_args=[],debug=False,root_dir='/tmp/wfgen_reports',use_log=True):
    """
    Run the server application

    :param addr: string of the IP address to bind with (defaults to internet connection)
    :param port: the integer value of the port to bind with (defaults to 50000)
    """
    if root_dir is None:
        root_dir = '/tmp/wfgen_reports'
    if not os.path.exists(root_dir):
        os.makedirs(root_dir)
    log_c = logger_client("server.run") if use_log else fake_log("server.run",cout=False)
    global sp_failback
    print("Starting server...",end='')
    log_c.log(c_logger.level_t.INFO,"Starting server...")
    try:
        signal.signal(signal.SIGINT,lambda *x,**y: ())
        network_interface.connect()
        signal.signal(signal.SIGINT,signal.default_int_handler)
    except:
        print("failed")
        log_c.log(c_logger.level_t.ERROR,"failed start")
        raise RuntimeError("Could not start the server")
    print("started")
    log_c.log(c_logger.level_t.INFO,"started")
    active_procs = []
    finished_procs = []
    if network_interface.endpoint is not None:
        print("CONTROL ENDPOINT:",network_interface.endpoint)

    json_template = 'truth_dev_{serial:s}_instance_{instance:05d}.json'
    Current_STATE = ServerState(root_dir,network_interface,uhd_args,json_template,debug,use_log=use_log)

    def clean_up():
        nonlocal active_procs,finished_procs,Current_STATE
        if len(active_procs) > 0:
            active_pids = [x[0] for x in active_procs]
            for idx in range(len(active_procs)):
                log_c.log(c_logger.level_t.INFO,"Killing process id: {0!s}".format(active_pids[idx]))
                if not isinstance(active_procs[idx][1],tuple):
                    if isinstance(active_procs[idx][1],mp.Process):
                        active_procs[idx][1].terminate()
                    else:
                        active_procs[idx][1].send_signal(signal.SIGINT)
                else:
                    active_procs[idx][1][0].send_signal(signal.SIGINT)
                    active_procs[idx][1][1].stop()
            for idx in reversed(range(len(active_procs))):
                if not isinstance(active_procs[idx][1],tuple):
                    if isinstance(active_procs[idx][1],mp.Process):
                        active_procs[idx][1].join()
                        active_procs[idx][1].close()
                    else:
                        active_procs[idx][1].wait()
                else:
                    for p in active_procs[idx][1]:
                        if p is None:
                            continue
                        p.wait()
                finished_procs.append(active_procs[idx])
                completed = active_procs[idx][2]

                if completed.startswith('start_radio'):
                    radio_to_deactivate = active_procs[idx][3]
                    if radio_to_deactivate[0] >= 0:
                        Current_STATE.deactivate_radios(radio_to_deactivate)
                elif completed.startswith('run_random'):
                    radios_to_deactivate = active_procs[idx][3]
                    Current_STATE.deactivate_radios(radios_to_deactivate)
                del active_procs[idx]

    def can_clean(radio_idx):
        # check if there's any active process using this radio that should be cleaned up
        nonlocal log_c,active_procs,finished_procs,Current_STATE
        singular = False
        if not isinstance(radio_idx,list):
            radio_idx = [radio_idx]
            singular = True
        cleaned = [True]*len(radio_idx)
        cleanup = []
        ##### Check for all possible radio connections to processes
        for ridx in radio_idx:
            # For this radio find all processes that say it's in use
            proc_idx = [pidx for pidx,x in enumerate(active_procs) if ridx in x[3]]
            if len(proc_idx):
                for pidx in proc_idx:
                    proc_set = active_procs[pidx][1]
                    if isinstance(proc_set,tuple):
                        #### (usrp:mp.Process,flowgraph:profile)
                        if proc_set[0] is not None:
                            callem = lambda x: x.is_alive() if isinstance(x,mp.Process) else x.poll()
                            if not callem(proc_set[0]):
                                if pidx not in cleanup:
                                    cleanup.append(pidx)
                                if proc_set[1] is not None:
                                    proc_set[1].stop()
                            else:
                                cleaned[ridx] = False
                    elif isinstance(proc_set,mp.Process):
                        if not proc_set.is_alive():
                            if pidx not in cleanup:
                                cleanup.append(pidx)
                        else:
                            cleaned[ridx] = False
                    elif isinstance(proc_set,subprocess.Popen):
                        if proc_set.poll() is not None:
                            if pidx not in cleanup:
                                cleanup.append(pidx)
                        else:
                            cleaned[ridx] = False
                    else:
                        log_c.log(c_logger.level_t.WARNING,"Not sure what this proc is... type({0!s})".format(type(active_procs[pidx][1])))
        cleanup = sorted(cleanup)
        ### Cleanup all active_procs that are done
        for pidx in reversed(cleanup):
            ### These procs need to be cleanly closed and moved to finished
            proc_info = active_procs[pidx]
            proc_set = proc_info[1]
            if isinstance(proc_set,tuple):
                ### (usrp:mp.Process,flowgraph:profile)
                for sub_proc in proc_set:
                    log_c.log(c_logger.level_t.INFO,'cleanup sub_proc? {0!s}'.format(sub_proc))
                    if sub_proc is None:
                        continue
                    sub_proc.wait()
            elif isinstance(proc_set,mp.Process):
                proc_set.join()
                proc_set.close()
            else:
                proc_set.wait()
            finished_procs.append(proc_info)
            completed = proc_info[2]
            if completed.startswith('start_radio'):
                radio_to_deactivate = proc_info[3] ## should be a list of len 1
                if radio_to_deactivate[0] >= 0:
                    Current_STATE.deactivate_radios(radio_to_deactivate)
            elif completed.startswith('run_random') or completed.startswith('run_script'):
                radios_to_deactivate = proc_info[3]
                Current_STATE.deactivate_radios(radios_to_deactivate)
            del active_procs[pidx]

        if singular:
            cleaned = cleaned[0]
        return cleaned

    msg_waiting = False
    no_reply = False
    logged_mprocs = []
    # running = True
    def sigint_handle(sig,frame):
        nonlocal msg_waiting, network_interface
        network_interface.request_queue.appendleft(ServerRequests(None,["shutdown","quiet"]))
        msg_waiting = False
        signal.signal(signal.SIGINT,signal.default_int_handler)
    signal.signal(signal.SIGINT,sigint_handle)

    while True:
        if not msg_waiting and network_interface.request_queue:
            msg_req = network_interface.request_queue.popleft()
            print("Handling:",msg_req)
            msg_waiting = True
        if msg_waiting:
            sent_from = msg_req.get_dest()
            message = msg_req.get_message()
            log_c.log(c_logger.level_t.INFO,"Got message with length: {0!s}".format(len(" ".join(message))))
            if not message:
                if not no_reply:
                    stat = network_interface.send_reply(ServerRequests(sent_from,['No command given']))
                msg_req = None
                message = None
                msg_waiting = False
                no_reply = False
                continue
            cmd = message[0]
            if cmd not in command_list:
                if not no_reply:
                    stat = network_interface.send_reply(ServerRequests(sent_from,['Invalid command:',cmd]))
                msg_req = None
                message = None
                msg_waiting = False
                no_reply = False
                continue

            if cmd == 'shutdown':
                clean_up()
                Current_STATE.consolidate()
                if 'quiet' not in message:
                    rep = ServerRequests(sent_from,["Shutting","down"] + ["{0!s}".format(x) for x in finished_procs])
                    network_interface.send_reply(rep)
                for x in network_interface.state_map.keys(): 
                    rep = ServerRequests(x,["bye","bye"])
                    network_interface.send_reply(rep)
                time.sleep(2.0) ## hopefully all messages get out with this delay
                shutdown()
                break
            elif cmd == 'get_radios':
                info = launch(cmd,Current_STATE,use_log=use_log)
                if not no_reply:
                    rep = ServerRequests(sent_from,['Found:\n',info])
                    stat = network_interface.send_reply(rep)
                    rep = None
                Current_STATE.set_radios(info)
                log_c.log(c_logger.level_t.INFO,"{0!s}".format(Current_STATE))
            elif cmd == 'start_radio':
                args_index = [idx for idx,x in enumerate(message) if x.startswith('-a')]
                if(len(args_index) == 0):
                    if '__debug_test__' in message:
                        ### assuming a debug/test event here
                        class dummy_proc:
                            def __init__(self,pid):
                                self.pid = pid
                            def kill(self):
                                pass
                            def send_signal(self,sig):
                                pass
                            def wait(self):
                                pass
                        try:
                            proc = launch(message,Current_STATE,use_log=use_log)
                            if not no_reply:
                                rep = ServerRequests(sent_from,["Starting process","{}".format(proc)])
                                stat = network_interface.send_reply(rep)
                                rep = None
                            active_procs.append(([proc,dummy_proc(proc)," ".join(message),[-1]]))
                        except:
                            if not no_reply:
                                rep = ServerRequests(sent_from,["Unable","to","start","process"])
                                stat = network_interface.send_reply(rep)
                                rep = None
                    else:
                        if not no_reply:
                            rep = ServerRequests(sent_from,["Invalid","command","or","unknown","command"])
                            stat = network_interface.send_reply(rep)
                            rep = None
                else:
                    ####### Blind assumption of USRP radio
                    args_index = args_index[0]
                    tail = message[args_index][message[args_index].find("serial")+7:]
                    if tail.find(',') > 0:
                        dev_serial = tail[:tail.find(',')]
                    else:
                        dev_serial = tail
                    radio_index = Current_STATE.radios.get_index_from_serial(dev_serial)
                    if radio_index == -1:
                        log_c.log(c_logger.level_t.INFO,"Request for radio {} is not mine, dropping".format(dev_serial))
                        ########### need to still reply
                        if not no_reply:
                            rep = ServerRequests(sent_from,["not my radio ->","{}".format(dev_serial)])
                            stat = network_interface.send_reply(rep)
                            rep = None
                    else:
                        proc = None
                        if Current_STATE.is_active(radio_index):
                            #### let's check to see if it finished on it's own since user is asking for it again.
                            cleaned = can_clean(radio_index)
                            if not cleaned:
                                invalid_request = ["Radio","is","still","in","use"]
                                proc = -1
                        if proc is None:
                            #### In a good state, so let's start a process
                            truth_file = Current_STATE.get_json_filename(dev_serial=dev_serial)
                            message.append('json')
                            message.append(truth_file)
                            invalid_request = ["Invalid","command","or","unknown","command"]
                            try:
                                proc = launch(message,Current_STATE,use_log=use_log)
                            except:
                                import traceback
                                traceback.print_exc()
                                proc = None
                                invalid_request = ["Unable","to","launch","last","command"]
                        if proc is None:
                            #### In a bad state, so return what went wrong
                            if not no_reply:
                                rep = ServerRequests(sent_from,invalid_request)
                                stat = network_interface.send_reply(rep)
                                rep = None
                        elif isinstance(proc,subprocess.Popen):
                            if not no_reply:
                                rep = ServerRequests(sent_from,["Starting process","{}".format(proc.pid),truth_file])
                                stat = network_interface.send_reply(rep)
                                rep = None
                            active_procs.append((proc.pid,proc," ".join(message),[radio_index]))
                            Current_STATE.activate_radios([radio_index])
                        elif isinstance(proc,tuple):
                            #### Had to start multiple procs
                            if not no_reply:
                                rep = ServerRequests(sent_from,["Starting process","<{0!s} & {1!s}>".format(
                                    proc[0].pid if proc[0] is not None else sp_failback,proc[1]),truth_file])
                                stat = network_interface.send_reply(rep)
                                rep = None
                            if proc[0] is None:
                                active_procs.append((sp_failback,proc," ".join(message),[radio_index]))
                                sp_failback -= 1
                            else:
                                active_procs.append((proc[0].pid,proc," ".join(message),[radio_index]))
                            Current_STATE.activate_radios([radio_index])
                        else:
                            log_c.log(c_logger.level_t.CRITICAL,"Not sure why we got here....")
            elif cmd == 'kill':
                cleanup_messages = []
                for kill_slot in message[1:]:
                    active_pids = [x[0] for x in active_procs]
                    if int(kill_slot) in active_pids:
                        log_c.log(c_logger.level_t.INFO,"Killing process id: {0!s}".format(int(kill_slot)))
                        idx = active_pids.index(int(kill_slot))
                        if not isinstance(active_procs[idx][1],tuple):
                            if not isinstance(active_procs[idx][1],mp.Process):
                                active_procs[idx][1].send_signal(signal.SIGINT)
                            else:
                                active_procs[idx][1].terminate()
                        else:
                            if active_procs[idx][1][0] is not None:
                                active_procs[idx][1][0].send_signal(signal.SIGINT)
                            if active_procs[idx][1][1] is not None:
                                active_procs[idx][1][1].stop()
                        start_message = active_procs[idx][2]
                        # active_procs[idx][1].kill()
                        if not no_reply:
                            cleanup_messages.append("Killing process {}".format(kill_slot))
                        if not isinstance(active_procs[idx][1],tuple):
                            if isinstance(active_procs[idx][1],mp.Process):
                                active_procs[idx][1].join()
                                active_procs[idx][1].close()
                            else:
                                active_procs[idx][1].wait()
                        else:
                            for sub_proc in active_procs[idx][1]:
                                log_c.log(c_logger.level_t.INFO,"sub_proc? {0!s}".format(sub_proc))
                                if sub_proc is None:
                                    continue
                                sub_proc.wait()
                        log_c.log(c_logger.level_t.INFO,"process killed? {0!s}".format(kill_slot))
                        finished_procs.append(active_procs[idx])
                        completed = active_procs[idx][2]

                        if completed.startswith('start_radio'):
                            radio_to_deactivate = active_procs[idx][3]
                            if radio_to_deactivate[0] >= 0:
                                Current_STATE.deactivate_radios(radio_to_deactivate)
                        elif completed.startswith('run_random') or completed.startswith('run_script'):
                            radios_to_deactivate = active_procs[idx][3]
                            Current_STATE.deactivate_radios(radios_to_deactivate)
                        del active_procs[idx]
                    else:
                        log_c.log(c_logger.level_t.INFO,"Not such process id to kill: {0!s}".format(int(kill_slot)))
                        if not no_reply:
                            cleanup_messages.append("No process {}".format(message[1]))
                if cleanup_messages:
                        reply = "\n".join(cleanup_messages)
                        rep = ServerRequests(sent_from,reply.split(' '))
                        stat = network_interface.send_reply(rep)
                        rep = None
            elif cmd == 'ping':
                if not no_reply:
                    stat = network_interface.send_reply(ServerRequests(sent_from,['pong']))
            elif cmd == 'get_active':
                if len(active_procs) == 0:
                    if not no_reply:
                        rep = ServerRequests(sent_from,["No","Active","Processes"])
                        stat = network_interface.send_reply(rep)
                        rep = None
                else:
                    if not no_reply:
                        rep = ServerRequests(sent_from,
                            ['Active',]+["{0!s}".format(x) for x in active_procs])
                        stat = network_interface.send_reply(rep)
                        rep = None
            elif cmd == 'get_finished':
                if len(finished_procs) == 0:
                    if not no_reply:
                        rep = ServerRequests(sent_from,["No","Finished","Processes"])
                        stat = network_interface.send_reply(rep)
                        rep = None
                else:
                    if not no_reply:
                        rep = ServerRequests(sent_from,
                            ['Finished',]+["{0!s}".format(x) for x in finished_procs])
                        stat = network_interface.send_reply(rep)
                        rep = None
            elif cmd == 'run_random':
                try:
                    request = yaml.safe_load(" ".join(message[1:]))
                    response = None
                except:
                    response = "Couldn't handle that just yet".split()
                    log_c.log(c_logger.level_t.INFO,' '.join(response))
                    if not no_reply:
                        rep = ServerRequests(sent_from,response)
                        stat = network_interface.send_reply(rep)
                        rep = None
                if response is None:
                    radios = request['radios'] if 'radios' in request else [x for x in Current_STATE.idle_radios]
                    proc = None
                    if any([Current_STATE.is_active(x) for x in radios]):
                        cleaned = can_clean(radio_index)
                        if not all(cleaned):
                            response = " ".join(["Radio","is","still","in","use"])
                            proc = -1
                    if proc is None:
                        proc = launch(message,Current_STATE,use_log=use_log)
                    if proc is None:
                        response = "No launch process returned yet." if response is None else response
                        if not no_reply:
                            rep = ServerRequests(sent_from,response.split())
                            stat = network_interface.send_reply(rep)
                            rep = None
                    elif isinstance(proc,list) and len(proc) > 0 and isinstance(proc[0],str):
                        if not no_reply:
                                rep = ServerRequests(sent_from,proc)
                                stat = network_interface.send_reply(rep) ## something went wrong
                                rep = None
                    elif isinstance(proc,mp.Process):
                        if not no_reply:
                            rep = ServerRequests(sent_from,["Starting random run:","{}".format(proc.pid)])
                            stat = network_interface.send_reply(rep)
                            stat = False
                            rep = None
                        active_procs.append((proc.pid,proc," ".join(message),radios))
            elif cmd == 'get_truth':
                clean_up()
                Current_STATE.consolidate()
                report = Current_STATE.get_truth_file()
                if report is None:
                    stat = network_interface.send_reply(ServerRequests(sent_from,['report','empty','']))
                    continue
                report_status = 'valid'
                try:
                    with open(report,'r') as fp:
                        #### attempting to load, but might be invalid
                        #### really just want the string, so not important value
                        report_contents = json.load(fp)
                        if len(report_contents['reports']) == 0:
                            report_status = 'empty'
                except:
                    report_status = 'invalid'
                if report_status != 'empty':
                    with open(report,'r') as fp:
                        report_contents = fp.read() ###
                else:
                    report_contents = ''
                stat = network_interface.send_reply(ServerRequests(sent_from,['report',report_status,report_contents]))
                Current_STATE.make_root()
            elif cmd == 'run_script':
                try:
                    request = yaml.safe_load(" ".join(message[1:]))
                    response = None
                    log_c.log(c_logger.level_t.INFO,"run_script running from:"+" ".join(message[1:]))
                except:
                    response = "Couldn't handle that just yet".split()
                    log_c.log(c_logger.level_t.INFO,' '.join(response))
                    if not no_reply:
                        stat = network_interface.send_reply(ServerRequests(sent_from,response))
                if Current_STATE.radios is None:
                    response = "Get radios FIRST!".split()
                    log_c.log(c_logger.level_t.INFO,' '.join(response))
                    if not no_reply:
                        stat = network_interface.send_reply(ServerRequests(sent_from,response))
                if response is None:
                    valid_radio_args = [x for x in request.keys() if x in [y['args'] for y in Current_STATE.radios.radios] and x != 'runtime']
                    if len(valid_radio_args) == 0:
                        response = "Got your request, no valid radios here."
                        if not no_reply:
                            stat = network_interface.send_reply(ServerRequests(sent_from,response.split()))
                        print([y['args'] for y in Current_STATE.radios.radios])
                        print([x for x in request.keys()])
                        print([x for x in request.keys() if x in [y['args'] for y in Current_STATE.radios.radios] and x != 'runtime'])
                    else:
                        radios = [x for x in Current_STATE.idle_radios if x in 
                                    [y['args'] for y in Current_STATE.radios.radios]]
                        proc = None
                        if any([Current_STATE.is_active(x) for x in radios]):
                            cleaned = can_clean(radio_index)
                            if not all(cleaned):
                                response = " ".join(["Radio","is","still","in","use"])
                                proc = -1
                        if proc is None:
                            ### there's at least one valid radio in this request
                            proc = launch(message,Current_STATE,use_log=use_log)
                        if proc is None:
                            response = "No launch process returned yet." if response is None else response
                            if not no_reply:
                                stat = network_interface.send_reply(ServerRequests(sent_from,response.split()))
                        elif isinstance(proc,list) and len(proc) > 0 and isinstance(proc[0],str):
                            if not no_reply:
                                stat = network_interface.send_reply(ServerRequests(sent_from,proc)) ## something went wrong
                        elif isinstance(proc,mp.Process):
                            if not no_reply:
                                stat = network_interface.send_reply(ServerRequests(sent_from,["Starting scripted run:","{}".format(proc.pid)]))
                                stat = False
                            active_procs.append((proc.pid,proc," ".join(message),radios))
            else:
                if not no_reply:
                    stat = network_interface.send_reply(ServerRequests(sent_from,["Valid commands:"]+command_list))
        msg_req = None
        message = None
        msg_waiting = False
        no_reply = False
        if any([isinstance(x[1],mp.Process) for x in active_procs]):
            if any([True for x in active_procs if x not in logged_mprocs]):
                log_c.log(c_logger.level_t.INFO,"Got some mp.Procs")
            logged_mprocs = [x for x in active_procs if isinstance(x[1],mp.Process)]
            for x in active_procs:
                if not isinstance(x[1], mp.Process):
                    continue
                if not x[1].is_alive():
                    log_c.log(c_logger.level_t.INFO,"Found one that's done")
                    msg_waiting = True
                    no_reply = True
                    message = ['kill',str(x[0])]
        if logged_mprocs:
            logged_mprocs = [x for x in logged_mprocs if x in active_procs]


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--net-config',
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)),'default_server.json'),
        type=str,help=f'Json file with a server network configuration (def: default_server.json)')
    p.add_argument("--truth-dir",default=None,type=str,help="Where should truth dump to? (def: /tmp/wfgen_reports")
    p.add_argument("--uhd-args",default=[],type=str,action='append',help="Limit to devices whose flag provided will find (def: all uhd devices)")
    p.add_argument("--log-server",action='store_true',help="Use if a log-server is active (meant for debugging)")
    return p.parse_args()

def main():
    args = parse_args()
    network = ServerNet(filepath=args.net_config)
    try:
        run(network,args.uhd_args,root_dir=args.truth_dir,use_log=args.log_server)
    except:
        import traceback
        traceback.print_exc()
    finally:
        network.disconnect()



if __name__ == '__main__':
    main()
