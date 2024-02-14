
import os
import multiprocessing as mp
import shlex
import subprocess
import random
import yaml
import time
import pprint
from typing import List
from wfgen import profiles
from wfgen.utils import paramify
from wfgen.run_modes import parse_random_run_reqest,parse_script_request


log_c = None
r_logger = None

def launch(cmd: List[str],server_state:"ServerState",use_log=False):
    """
    Handler to figure out what to launch if anything at all

    :param cmd: List of strings
    """
    global log_c, r_logger
    if log_c is None:
        from wfgen import logger_client,fake_log,c_logger
        log_c = logger_client() if use_log else fake_log(cout=False)
        r_logger = c_logger
    if cmd == 'get_radios':
        log_c.log(r_logger.level_t.INFO,"Getting Radio Info")
        return get_radios(server_state.uhd_args)
    if cmd[0] not in ['start_radio','run_random','run_script']:
        return None
    log_c.log(r_logger.level_t.INFO,"\nLaunching with command:\n{0!s}".format(pprint.pformat(cmd,indent=2)))
    if cmd[0] == 'start_radio':
        if cmd[1].startswith("wfgen_"):
            log_c.log(r_logger.level_t.INFO,"\nstarting a radio with command\n\t{}".format(cmd[1:]))
            if any(['quiet' in x for x in cmd]):
                proc = subprocess.Popen(cmd[1:],stdout=subprocess.PIPE,stderr=subprocess.PIPE,shell=False)
            else:
                proc = subprocess.Popen(cmd[1:],shell=False)
            if proc.poll() is None:
                return proc
            else:
                proc.wait()
                return None
        elif cmd[1].startswith("__debug_test__"):
            return random.randint(0,100)
        # elif cmd[1].startswith("profile") or cmd[1].startswith("constant"):
        elif cmd[1] in ['static','replay']:
            if hasattr(profiles,cmd[3]):
                prof = getattr(profiles,cmd[3])
                log_c.log(r_logger.level_t.DEBUG,"\nbefore paramify: {0!s}".format(cmd[4:]))
                params = paramify(cmd[4:])
                log_c.log(r_logger.level_t.INFO,"\nUsing Parameters: {0!s}".format(params))
                p = prof(use_log=use_log)
                launch_env = os.environ.copy()
                launch_command = p.start(cmd[2],params)
                command = shlex.split(launch_command)
                log_c.log(r_logger.level_t.DEBUG,"---launch command: {0!s}".format(command))
                if any(['quiet' in x for x in cmd]):
                    proc = subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,shell=False,env=launch_env)
                else:
                    proc = subprocess.Popen(command,shell=False,env=launch_env)
                if proc.poll() is None:
                    return proc
                else:
                    proc.wait()
                    return None
            else:
                log_c.log(r_logger.level_t.ERROR,"Unknown profile: {0!s}".format(cmd[3]))
                return None
        elif cmd[1].startswith("bursty") or cmd[1].startswith("hopper"):
            if hasattr(profiles,cmd[3]):
                ### Cool, we can do this
                prof = getattr(profiles,cmd[3])
                params = paramify(cmd[4:])
                params['hopper'] = True
                if cmd[1] == "bursty":
                    params["num_channels"] = 1
                print(params)
                p = prof(use_log=use_log)
                launch_env = os.environ.copy()
                command = p.start(cmd[2],params).split()
                log_c.log(r_logger.level_t.DEBUG,"---launch command: {0!s}".format(command))
                if any(['quiet' in x for x in cmd]):
                    proc = subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,shell=False,env=launch_env)
                else:
                    proc = subprocess.Popen(command,shell=False,env=launch_env)
                if proc.poll() is None:
                    return proc
                else:
                    proc.wait()
                    return None

            else:
                log_c.log(r_logger.level_t.ERROR,"Cannot handle profle: {0!s}".format(cmd[3]))
                return None
        else:
            log_c.log(r_logger.level_t.ERROR,"Unknown Command: {0!s}".format(cmd))
            return None
    elif cmd[0] == 'run_random':
        log_c.log(r_logger.level_t.INFO,"Attempting to start a run_random request")
        request = yaml.safe_load(" ".join(cmd[1:]))
        proc = parse_random_run_reqest(request,server_state)
        if isinstance(proc,str):
            return proc.split()
        return proc
    elif cmd[0] == 'run_script':
        log_c.log(r_logger.level_t.INFO,"Attempting to start a run_script request")
        request = yaml.safe_load(" ".join(cmd[1:]))
        proc = parse_script_request(request,server_state,use_log)
        if isinstance(proc,str):
            return proc.split()
        return proc
    else:
        log_c.log(r_logger.level_t.CRITICAL,"Shouldn't get here -- check logic in 'launch'")
        return None

def get_radios(uhd_args=[]):
    command = "uhd_find_devices"
    if len(uhd_args):
        ettus_radios = []
        command += ' --args="{0}"'
        for restrict in uhd_args:
            exec_command = shlex.split(command.format(restrict))
            try:
                this_one = subprocess.run(exec_command,check=True,capture_output=True,text=True).stdout
                ettus_radios.append(this_one)
            except subprocess.CalledProcessError as e:
                print("No devices found with restriction:",restrict)
        if not ettus_radios:
            ettus_radios = "No devices found"
        else:
            ettus_radios = '\n'.join(ettus_radios)
    else:
        try:
            ettus_radios = subprocess.run(command,check=True,capture_output=True,text=True).stdout
            ettus_radios = ettus_radios
        except subprocess.CalledProcessError as e:
            print("No devices found, try using UHD arguments.")
            ettus_radios = "No devices found"
    return ettus_radios
