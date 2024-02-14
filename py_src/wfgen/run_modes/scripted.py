

import os
import json,yaml
import numpy as np
from copy import copy,deepcopy
from scipy.stats import mode,chi2,t as t_student
from scipy.special import erf
from scipy.optimize import curve_fit
from typing import Dict
import warnings

import multiprocessing as mp
import signal
from datetime import datetime, timezone
import shlex
import subprocess
import time
import glob

import matplotlib.pyplot as plt

try:
    from ..profiles import get_all_profile_names,extract_profile_by_name,get_replay_profile_names
except ImportError:
    from wfgen.profiles import get_all_profile_names,extract_profile_by_name,get_replay_profile_names

RNG_TYPE = np.random.Generator

# DEBUG_TIMEOUT = 240
DEBUG_TIMEOUT = float('inf')

def get_time():
    return datetime.now(timezone.utc).timestamp()

scripted_parameter_keys = {
    ### system/process params
    'system':[
        'server_addresses',         # what servers to connect to
        'server_ports',             # a 1-1 map with server address
        'server_ssh',               # a 1-1 map with server address
        'runtime',                  # how long should this process be active
        'configs',                  # what files should be pulled in as signals
        'toggles',                  #### optional -- change modes for increased randomization
        'parameters',               #### can override at the system level any of the below
    ],
    ### source params
    'source':[
        'device',                   # can be absent/None
    ],
    ### signal params
    'signal':[
        'activity_type',            # can be absent/None
        'freq_lo', 'freq_hi',       ### either these two \
        'span',    'band_center',   ### or these two are needed
        'mode',                     # static, bursty, hopper, emanation
        'profile',                  # what is being sent
        'rate',                     # sample rate of the radio transmitting
        'bw',                       # Bandwidth of each energy
        'duration',                 # How long the signal is on for
        'time_start',               # When the signal should turn on in the run
        'time_stop',                # When the signal should no longer run
        'idle',                     # Minimum time the signal should be off for
        'gain',                     # How loud should the signal be
        'src_fq',                   # am/fm parameter for tone generation
        'signal_limit',             # Maximum number of occurences None, <= 0, no limit
    ],
    ### energy params
    'energy':[
        'dwell',                    # How long can an energy burst be
        'absence',                  # How long can the signal be off between bursts
        'frequency',                # Specific frequency to use
    ],
}

def load_config(config_path=None,config_file=None):
    """
    Loads the config files for automated run.

    Args:
        config_path (string): Path to config DIRECTORY
        config_file (string): Path to config FILE

    Returns:
        list(dict(configs)): List of configuration dictionaries
    """

    if config_path is None and config_file is None:
        raise ValueError("Either specify a config_path or a config_file")
    
    config_files = []
    if config_path is not None:
        main_config = os.path.join(config_path, "main.json")
    # End if
    else:
        main_config = config_file
        config_path = os.path.dirname(config_file)
    # End else
    with open(main_config, 'r', encoding='utf-8') as config_file:
        config_data = json.load(config_file)
    # End with
    if any([key not in config_data for key in scripted_parameter_keys['system'][:5]]):## parameters can be missing
        raise RuntimeError("Missing expected system level keys: {0!s}".format(scripted_parameter_keys['system'][:5]))
    # End if
    config_files = [os.path.join(config_path, file_name+'.json') for file_name in config_data["configs"]]

    sig_configs = []
    all_paths = []
    for path in config_files:
        curr_paths = [path]
        if '*' in path:
            if '**' in path:
                curr_paths = sorted(glob.glob(path,recursive=True),key=lambda x: x.lower())
            else:
                curr_paths = sorted(glob.glob(path),key=lambda x: x.lower())
        all_paths.extend(curr_paths)
    all_paths = sorted(all_paths,key=lambda x: x.lower())
    for path in all_paths:
        with open(path, 'r', encoding='utf-8') as config_file:
            config_wf = json.load(config_file)
        if 'profile' not in config_wf:
            continue
        if config_wf['profile'] not in get_all_profile_names():
            continue
        # End with
        sig_configs.append(config_wf)
    # End for

    config_data['configs'] = sig_configs
    return config_data
# End load_config

def load_script(filepath):

    flags = 0
    if os.path.basename(filepath).startswith('output-') and os.path.basename(filepath).endswith('-truth.json'):
        flags = 1 #Type 1 Truth file
    elif 'NGC' in os.path.basename(filepath):
        flags = 2 # Type 2 Truth file
    elif filepath.endswith('.json'):
        # could be a wfgen rerun, or a wfgen config, check if 'reports' is in the rerun
        try:
            with open(filepath,'r') as fp:
                base_info = json.load(fp)
        except:
            raise RuntimeError("Invalid json file found in load_script(1)")
        if 'reports' in base_info:
            flags = 3
    if flags in [1,2,3]:
        try:
            with open(filepath,'r') as fp:
                reports = json.load(fp)
        except:
            raise RuntimeError("Invalid json file found in load_script(2)")
        energies = [x for x in reports['reports'] if x['report_type'] == 'energy']
        signals  = [x for x in reports['reports'] if x['report_type'] == 'signal']
        sources  = [x for x in reports['reports'] if x['report_type'] == 'source']
        energies,signals,sources = normalize_times(energies,signals,sources)
        for idx,sig in enumerate(signals):
            energy_set,energy_dict = orgainize_energy_set(energies,sig)
            if len(energy_set) > 1:
                dwells = extract_dwells(energy_set,energy_dict)
                dwelling = sum(dwells)/len(dwells)
                periods = extract_interarrivals(energy_set,energy_dict)
                perioding = sum(periods)/len(periods)
                signals[idx]['avg_dwell'] = dwelling
                signals[idx]['avg_period'] = perioding
        runtime = max([x['time_stop'] for x in signals])
        toggles = dict()
    else:
        ## Assuming this is one of our launches, rather than an exact script
        if filepath.endswith('.json'):
            system_setup = load_config(config_file=filepath)
        else:
            system_setup = load_config(filepath)
        energies,signals,sources = create_script_from_config(system_setup)
        runtime = system_setup['runtime']
        toggles = sources.pop(0)

    return runtime,energies,signals,sources,flags,toggles

def create_script_from_config(system_setup):
    signal_configs = system_setup['configs']
    if "parameters" in system_setup:
        ## These should override the values of every signal_config
        params = system_setup['parameters']
    else:
        params = dict()
    if 'toggles' in system_setup:
        toggles = system_setup['toggles']
    else:
        toggles = dict()
    energies = [] ##### ignoring this for now
    sources = [toggles]  ##### not sure how to handle this just yet
    signals = []
    for idx,sig in enumerate(signal_configs):
        signal_setup = dict()
        for key in scripted_parameter_keys['source']:
            if key in params:
                signal_setup[key] = params[key]
            elif key in sig:
                signal_setup[key] = sig[key]
        profile_found = False
        signal_keys = [x for x in scripted_parameter_keys['signal']]
        if 'band_center' in params or 'band_center' in sig:
            if 'span' not in params and 'span' not in sig:
                raise RuntimeError("'band_center' key found, but not 'span'")
            d_idx = signal_keys.index('freq_lo')
            del signal_keys[d_idx]
            d_idx = signal_keys.index('freq_hi')
            del signal_keys[d_idx]
        else:
            d_idx = signal_keys.index('band_center')
            del signal_keys[d_idx]
            d_idx = signal_keys.index('span')
            del signal_keys[d_idx]
        for key in signal_keys:
            if key in params and key != 'profile':
                signal_setup[key] = params[key]
            elif key in sig:
                if key == 'profile':
                    profile_found = True
                signal_setup[key] = sig[key]
        if not profile_found:
            print(sig)
            raise RuntimeError("Signal at index {} didn't specify 'profile' key".format(idx))
        for key in scripted_parameter_keys['energy']:
            if key in params:
                signal_setup[key] = params[key]
            elif key in sig:
                signal_setup[key] = sig[key]
        signals.append(signal_setup)

    return energies,signals,sources

def normalize_times(energies,signals,sources):
    start_time = float('inf')
    for x in energies:
        if x['time_start'] < start_time:
            start_time = x['time_start']
    for x in signals:
        if x['time_start'] < start_time:
            start_time = x['time_start']

    for idx,x in enumerate(energies):
        for k in x.keys():
            if 'time' in k:
                energies[idx][k] -= start_time
    for idx,x in enumerate(signals):
        for k in x.keys():
            if 'time' in k:
                signals[idx][k] -= start_time
    for idx,x in enumerate(sources):
        for k in x.keys():
            if 'time' in k:
                sources[idx][k] -= start_time
    return energies,signals,sources

def orgainize_energy_set(energies,signal):
    energy_dict = dict()
    energy_set = signal['energy_set']
    for x in energies:
        if x['instance_name'] in energy_set:
            energy_dict[x['instance_name']] = x
    return energy_set,energy_dict

def extract_dwells(energy_set,energy_dict,rounder=6):
    dwells = [None]*len(energy_set)
    for idx,ek in enumerate(energy_set):
        energy = energy_dict[ek]
        dwells[idx] = float(np.round(energy['time_stop'] - energy['time_start'],rounder))
    return dwells

def extract_interarrivals(energy_set,energy_dict,rounder=6):
    start_times = [None]*len(energy_set)
    for idx,ek in enumerate(energy_set):
        energy = energy_dict[ek]
        start_times[idx] = energy['time_start']
    interarrivals = np.array(start_times)
    interarrivals = np.round(interarrivals[1:] - interarrivals[:-1],rounder)
    return interarrivals.tolist()

def extract_squelchs(energy_set,energy_dict,rounder=6):
    lows = [None]*(len(energy_set)-1)
    prev_ek = None
    for idx,ek in enumerate(energy_set):
        if idx == 0:
            prev_ek = ek
            continue
        energy_new = energy_dict[ek]
        energy_old = energy_dict[prev_ek]
        lows[idx-1] = float(np.round(energy_new['time_start'] - energy_old['time_stop'],rounder))
        prev_ek = ek
    return lows

def extract_frequencies(energy_set,energy_dict,rounder=0):
    freqs = [None]*len(energy_set)
    for idx,ek in enumerate(energy_set):
        energy = energy_dict[ek]
        freqs[idx] = float(np.round((energy['freq_hi'] + energy['freq_lo'])/2*1e6,rounder))
    return freqs

def extract_bandwidths(energy_set,energy_dict,rounder=0):
    bws = [None]*len(energy_set)
    for idx,ek in enumerate(energy_set):
        energy = energy_dict[ek]
        bws[idx] = float(np.round((energy['freq_hi'] - energy['freq_lo'])*1e6,rounder))
    return bws

def extract_span(signal,rounder=0):
    span = signal['freq_hi']-signal['freq_lo']
    span *= 1e6
    return float(np.round(span,rounder))

def extract_metrics(energies,signals):
    signal_metrics = dict()
    for signal in signals:
        #### energy metrics
        energy_set,energy_dict = orgainize_energy_set(energies,signal)
        dwells = extract_dwells(energy_set,energy_dict)
        arrivals = extract_interarrivals(energy_set,energy_dict)
        absence = extract_squelchs(energy_set,energy_dict)
        freqs = extract_frequencies(energy_set,energy_dict)
        bws = extract_bandwidths(energy_set,energy_dict)
        #### signal stats
        span = extract_span(signal)
        sig = copy(signal)
        del sig['energy_set']
        signal_metrics[signal['instance_name']] = {
            'energy_set': energy_set,
            'energy_dict': energy_dict,
            'signal': sig,
            'dwells': dwells,
            'arrivals': arrivals,
            'absence': absence,
            'freqs': freqs,
            'bws': bws,
            'span': span
        }
    return signal_metrics


def sigmoid(x, L, x0, k, b):
    y = L / (1+np.exp(-k*(x-x0))) + b
    return y

def gauss_sigmoid(x,x0,k):
    return sigmoid(x,1,x0,k,0)

def uni_sigmoid(x,L,k,theta=1,x0=0):
    return sigmoid(x,L,x0,k/theta,-L/2)

def exp_sigmoid(x,x0,k):
    return sigmoid(x,2,x0,k,-1)

def gauss_eval(x):
    if x is None:
        return None
    if isinstance(x,(int,float,np.number)):
        return (0,0,None,None)
    if len(x) == 1:
        return (0,0,None,None)
    x = np.array(x)
    n = len(x)
    # f(x) = exp(-.5*(x-u)^2/v)/sqrt(2*pi*v)
    # F(x) = 0.5*(1+erf((x-u)/sqrt(2*v)))
    #
    # f(z) = exp(-.5*z^2)/sqrt(2*pi)
    # F(z) = 0.5*(1+erf(z/sqrt(2)))
    u = np.mean(x)
    v = np.mean(np.power(x-u,2))
    if v < np.finfo(np.float32).eps:
        return (0,0,None,None)
    z = (x-u)/np.sqrt(v)

    cdf_of_observations = np.array([np.sum(x <= c) for c in x])/n
    exact_observations = 0.5*(1+erf(z/np.sqrt(2)))
    try:
        popt, pcov = curve_fit(gauss_sigmoid, z, cdf_of_observations, [0,1], bounds=([-3,0.1],[3,10]), method='dogbox')
        popt = [v*popt[0]+u,v*popt[1]] ### denormalize
    except RuntimeError as e:
        if "Optimal parameters not found" in str(e):
            popt = None
            pcov = None
        else:
            raise e

    mm = [(cdf_of_observations-exact_observations).max(),(exact_observations-cdf_of_observations).max()]
    num = np.power(cdf_of_observations-exact_observations,2)
    den = (exact_observations*(1-exact_observations) + np.ones(x.shape))/2
    scaler = np.sum(np.unique(den))
    gauss_conf = (u-t_student.ppf(1-0.05/2,n-1)*v/np.sqrt(n),
                  u+t_student.ppf(1-0.05/2,n-1)*v/np.sqrt(n))
    return (np.sum(mm),
            n*scaler*np.sum(num/den),
            gauss_conf,
            popt)

def folded_normal_eval(x):
    pass

def half_normal_eval(x):
    pass

def uni_eval(x):
    if x is None:
        return None
    if isinstance(x,(int,float,np.number)):
        return (0,0,None,None)
    if len(x) == 1:
        return (0,0,None,None)
    x = np.array(x)
    n = len(x)
    # f(x) = { 1/(b-a), a <= x <= b; 0, o.w.}
    # F(x) = { (x-a)/(b-a), a <= x <= b; 0, x < a; 1, x > b}
    a = np.min(x)
    b = np.max(x)
    if np.abs(b-a) < np.finfo(np.float32).eps:
        return (0,0,None,None)

    z = (x-a)/(b-a) #[0,1]
    # f(z) = { 1, 0 <= z <= 1; 0, o.w.}
    # F(z) = { z, 0 <= x <= 1; 0, x < 0; 1, x > 1}
    u = (a+b)/2

    cdf_of_observations = np.array([np.sum(x <= c) for c in x])/n
    exact_observations = (x-a)/(b-a)
    try:
        popt, pcov = curve_fit(uni_sigmoid, z, cdf_of_observations, [3,1],
                            bounds=([2,0],[1000,1000]), method='dogbox')
    except RuntimeError as e:
        if "Optimal parameters not found" in str(e):
            popt = None
            pcov = None
        else:
            raise e


    mm = [(cdf_of_observations-exact_observations).max(),(exact_observations-cdf_of_observations).max()]
    num = np.power(cdf_of_observations-exact_observations,2)
    den = (exact_observations*(1-exact_observations) + np.ones(x.shape))/2
    scaler = np.sum(np.unique(den))

    theta = b-a
    epsilon = (1-0.05)**(-1/n)-1
    uni_conf = (theta,theta*(1+epsilon))
    return (np.sum(mm),
            n*scaler*np.sum(num/den),
            uni_conf,
            popt)

def exp_eval(x):
    if x is None:
        return None
    if isinstance(x,(int,float,np.number)):
        return (0,0,None,None)
    if len(x) == 1:
        return (0,0,None,None)
    x = np.array(x)
    n = len(x)
    # f(x) = lam*exp(-lam*x)
    # F(x) = 1-exp(-lam*x)
    # z = (x-)
    # f(z) = exp(-z)
    # F(z) = 1-exp(-z)
    u = np.mean(x)
    if np.abs(u) < np.finfo(np.float32).eps:
        return (0,0,None,None)
    l = 1/np.mean(x)
    v = np.power(l,2)

    # z = (values/l) * np.exp((l-1)*x)
    cdf_of_observations = np.array([np.sum(x <= c) for c in x])/n
    exact_observations = 1-np.exp(-l*x)
    try:
        popt, pcov = curve_fit(exp_sigmoid, x, cdf_of_observations, [0,1],
                               bounds=([0,0.1],[x.max(),10]), method='dogbox')
    except RuntimeError as e:
        if "Optimal parameters not found" in str(e):
            popt = None
            pcov = None
        else:
            raise e

    mm = [(cdf_of_observations-exact_observations).max(),(exact_observations-cdf_of_observations).max()]
    num = np.power(cdf_of_observations-exact_observations,2)
    den = (exact_observations*(1-exact_observations) + np.ones(x.shape))/2
    scaler = np.sum(np.unique(den))

    a = 2*n/l/chi2.ppf(0.05,2*n)
    b = 2*n/l/chi2.ppf(1-0.05,2*n)
    exp_conf = (min(a,b),max(a,b))
    return (np.sum(mm),
            n*scaler*np.sum(num/den),
            exp_conf,
            popt)


def stats(vec,dive=1):

    count = len(vec) if not isinstance(vec,(int,float,np.number)) and vec is not None else 1
    if count == 1 and isinstance(vec,(int,float,np.number)):
        vec = [vec]
    if not isinstance(vec,np.ndarray):
        vec = np.array(vec)
    param = np.sort(np.unique(vec))
    stats_dict = {
        'count': count,
        'u_count': param.size,
        'min':None,
        'max':None,
        'mean':None,
        'var':None,
        'var_uni':None,
        'var_exp':None,
        'var_incr':None,
        'slope': None,
        'steps': None,
        'prob_gauss': None,
        'prob_uni': None,
        'prob_exp': None,
        'opt_gauss': None,
        'opt_uni': None,
        'opt_exp': None,
        'diff': None,
    }
    if count == 0:
        return stats_dict
    stats_dict['min'] = vec.min()
    stats_dict['max'] = vec.max()
    stats_dict['mean'] = vec.mean()
    stats_dict['var'] = vec.var()
    stats_dict['var_uni'] = np.power(vec.max()-vec.min(),2)/12
    stats_dict['var_exp'] = np.power(vec.mean(),2)
    if param.size == 1:
        return stats_dict
    old_setting = np.seterr(all='raise')
    np.seterr(under='ignore')
    x = gauss_eval(vec)
    if x is not None:
        stats_dict['prob_gauss'] = x[:2]
        stats_dict['conf_gauss'] = x[2]
        stats_dict['opt_gauss'] = x[3]
    x = uni_eval(vec)
    if x is not None:
        stats_dict['prob_uni'] = x[:2]
        stats_dict['conf_uni'] = x[2]
        stats_dict['opt_uni'] = x[3]
    x = exp_eval(vec)
    if x is not None:
        stats_dict['prob_exp'] = x[:2]
        stats_dict['conf_exp'] = x[2]
        stats_dict['opt_exp'] = x[3]
    diff = param[1:]-param[:-1]
    rise = param.max()-param.min()
    incr = diff.min() if diff.min() != 0. else 1
    run = param.max()/incr - param.min()/incr
    m = rise/run
    b = param.min()
    steps = (param.max() - b)/m +1 #[0,(max-b)/m] -> +1 include step 0 in the count
    if dive:
        diff = stats(diff,dive-1)
    else:
        diff = None
    
    stats_dict['slope'] = m
    stats_dict['steps'] = steps
    stats_dict['diff'] = diff
    stats_dict['var_incr'] = np.var(np.mod(param,incr))
    np.seterr(**old_setting)
    return stats_dict


def compress_stats(signal_metrics):
    ##### Cut out the energy_set,energy_dict,signals
    ##### compress the stats

    compressed_stats = dict()
    for sig_id,sig_stat in signal_metrics.items():
        dwells = stats(sig_stat['dwells'])
        arrivals = stats(sig_stat['arrivals'])
        absence = stats(sig_stat['absence'])
        freqs = stats(sig_stat['freqs'])
        bws = stats(sig_stat['bws'])
        span = stats(sig_stat['span'])

        compressed_stats[sig_id] = {
            'dwells': dwells,
            'arrivals': arrivals,
            'absence': absence,
            'freqs': freqs,
            'bws': bws,
            'span': span,
        }
    return compressed_stats

# def estimate_config(compressed_stats):


def parse_script_request(request:Dict,server_state:"ServerState",use_log=False):
    '''Create the process(es) needed to run the script request

    The request is expected to be structured as a dictionary
    with each key being the _unique_ key for a particular radio
    on this server. The value for that key is then a list with
    the first element being a seed vector for randomization,
    and all remaining elements being profiles that this radio,
    is able to transmit.
    '''
    import pprint
    from wfgen.profiles import get_all_profile_names
    from wfgen import logger_client, fake_log, c_logger
    if use_log:
        log_c = logger_client("{0!s}".format("parse_script_request"))
    else:
        log_c = fake_log("{0!s}".format("parse_script_request",cout=False))
    if 'runtime' not in request:
        runtime = 300.0
    else:
        runtime = request['runtime']
        del request['runtime']
    if 'flags' not in request:
        flags = 0 ### assuming a config
    else:
        flags = request['flags']
        del request['flags']
    if 'signal_limit' not in request:
        signal_limit = 10000
    else:
        signal_limit = request['signal_limit']
        del request['signal_limit']
    if 'toggles' in request:
        toggles = request['toggles']
        del request['toggles']
    else:
        toggles = dict()
    # if flags in [1,2]:
    #     from wfgen.profiles import get_traceback_profiles
    individual_radio_requests = [None]*len(request)
    #request = [{signal:signals[idx],timing:[signals[idx][time_start],signals[idx][time_stop]]},...]
    if DEBUG_TIMEOUT > 0 and flags in [1,2,3]:
        test = [quick_viz([y['signal'] for y in x[1:]] ,['modality','modulation','instance_name'],min(runtime,DEBUG_TIMEOUT)/100) for k,x in request.items()]
        for idx,item in enumerate(test):
            log_c.log(c_logger.level_t.DEBUG,'{}\n{}'.format(idx,item))
        expected = '\n'.join(test)
        with open("target_script_result.txt",'w') as fp:
            fp.write(expected)
    for k_idx,key in enumerate(request.keys()):
        # key is <args> to the radio
        # convert key to index
        radio_idx = [idx for idx,x in enumerate(server_state.radios.radios) if x['args'] == key]
        if not len(radio_idx):
            ## this server doesn't have this radio, so drop the radio requests
            continue
        radio_idx = radio_idx[0]
        individual_radio_requests[k_idx] = {
            "radio":radio_idx, ## radio index on the server
            'radio_args':key,  ## radio device args
            "seed":request[key][0] ## seed
        }
        if flags in [1,2,3]:
            ### This is a sponsor truth file
            for sig_info in request[key][1:]: ### all signal requests
                sig = sig_info["signal"]
                if 'energy_set' in  sig:       # delete these on the first pass through||| not needed yet
                    sig['energy_set'] = len(sig['energy_set'])
                if 'extras' in sig:            # delete these on the first pass through||| not needed yet
                    del sig['extras']
                # sig['profile'] = sig['wg_profile_name']
        individual_radio_requests[k_idx]['profiles'] = request[key][1:]

    individual_radio_requests = [x for x in individual_radio_requests if x is not None]
    log_c.log(c_logger.level_t.DEBUG,pprint.pformat(individual_radio_requests,indent=2))

    for irr in individual_radio_requests:
        for sig_info in irr['profiles']:
            profile = sig_info['signal']
            if profile['profile'] not in get_all_profile_names():
                print(profile['profile'],'-- not found')

        if irr['radio'] not in server_state.idle_radios:
            print("Cannot use radio:",irr['radio'],"|| did cleanup not work??")

    proc = mp.Process(target=scripted_run,args=(
        runtime,
        individual_radio_requests,
        flags,
        server_state.burst_instance,
        server_state.burst_instance+signal_limit,
        toggles,
        os.path.join(server_state.save_dir,server_state.json_proto),
        use_log
    ))
    proc.start()

    return proc


def scripted_run(runtime, available_radio_profiles, flags, initial_idx, final_idx,
                 toggles=dict(),
                 truth_template='', use_log=False):
    ''' Manage and maintain a scripted run, where each signal is its own process

    This should just keep creating work until runtime is over
    '''
    manager = mp.Manager()
    worker_lookups = manager.dict()
    worker_lookups['runtime'] = runtime
    worker_lookups['semaphore'] = manager.Semaphore(1)
    worker_lookups['start_time'] = None ## First worker to claim semaphore sets this
    worker_lookups['end_time'] = None  ## First worker to claim semaphore sets this
    worker_lookups['instance'] = initial_idx
    worker_lookups['instance_limit'] = final_idx
    worker_lookups['json_template'] = truth_template
    worker_lookups['flags'] = flags
    worker_lookups['toggles'] = toggles
    worker_lookups['use_log'] = use_log
    from wfgen import logger_client, fake_log, c_logger
    if use_log:
        log_c = logger_client("{0!s}".format("scripted_run"))
    else:
        log_c = fake_log("{0!s}".format("scripted_run",cout=False))


    # radio_idx = [x['radio'] for x in available_radio_profiles]
    radio_args = [x['radio_args'] for x in available_radio_profiles]
    radio_count = len(available_radio_profiles)
    worker_lookups['server_size'] = radio_count
    radio_seeds = [x['seed'] for x in available_radio_profiles]

    rngs = [None]*len(radio_seeds)
    seed_sets = [None]*len(radio_seeds)
    for idx,seed in enumerate(radio_seeds):
        rngs[idx] = np.random.default_rng(seed)
        seed_sets[idx] = rngs[idx].integers(np.iinfo(np.uint64).min,np.iinfo(np.int64).max,size=10)

    workers = [None]*radio_count
    if 'random_pick_overlapping_waveforms' in toggles and toggles['random_pick_overlapping_waveforms']:
        for ridx in range(radio_count):
            rng = rngs[ridx]
            ###### profiles without time constraints -- assume it can be played anytime
            holdouts = [x for x in available_radio_profiles[ridx]['profiles'] if x['timing'][0] is None]
            ###### profiles with time constraints -- sort by start time
            radio_profiles = sorted([x for x in available_radio_profiles[ridx]['profiles'] if x['timing'][0] is not None],key=lambda x: x['timing'][0])
            total_profiles = len(radio_profiles)
            handled_profiles = 0
            keeping_profiles = []
            while handled_profiles < total_profiles:
                current_time_window = radio_profiles[handled_profiles]['timing']
                pull_count = 1
                while radio_profiles[handled_profiles+pull_count]['timing'][0] < current_time_window[1]:
                    pull_count += 1
                    if handled_profiles+pull_count >= total_profiles:
                        break
                conflicted = radio_profiles[handled_profiles:handled_profiles+pull_count]
                keeping_profiles.append(rng.choice(conflicted))
                handled_profiles += pull_count
            available_radio_profiles[ridx]['profiles'] = keeping_profiles + holdouts
            log_c.log(c_logger.level_t.INFO,"RPOW Toggle results ({}) -> ({}) waveforms left".format(total_profiles,len(keeping_profiles)))

    early_terminate = 0
    def sig_handler(sig,frame):
        nonlocal early_terminate,workers
        # print("What ?? : ",signal.strsignal(sig))
        log_c.log(c_logger.level_t.INFO,"Early Exit handler from: "+signal.strsignal(sig))
        early_terminate += 1
        ### tell all alive workers to termiante
        for w in workers:
            if w is not None and w.is_alive():
                w.terminate()
    # sig_set = set(signal.Signals) - {signal.SIGKILL,signal.SIGSTOP}
    default_term_handler = signal.getsignal(signal.SIGTERM)
    default_int_handler = signal.getsignal(signal.SIGINT)
    def enable_sig_handler():
        sig_set = {signal.SIGINT,signal.SIGTERM}
        for sig in sig_set:
            signal.signal(sig,sig_handler)
    def disable_sig_handler():
        nonlocal default_term_handler,default_int_handler
        signal.signal(signal.SIGINT,default_int_handler)
        signal.signal(signal.SIGTERM,default_term_handler)

    enable_sig_handler()

    worker_n = 0
    def reasons_to_loop():
        nonlocal early_terminate,worker_lookups
        return (
            early_terminate == 0, # not told to exit
            worker_lookups['instance'] < worker_lookups['instance_limit'], ## still have more instances allotted
            worker_lookups['end_time'] is None or \
                (worker_lookups['end_time'] is not None and \
                    get_time() < worker_lookups['end_time']), ### if end_time hasn't
                        #### been set, keep going, but if it has been set, make sure we're not beyond it.
            get_time() < worker_lookups['start_time']+DEBUG_TIMEOUT if worker_lookups['start_time'] is not None else True #### DEBUG
        )
    while all(reasons_to_loop()):
        if any([x is None for x in workers]):
            ### we could start another radio worker... should we? just assume yes because something crashed
            for idx in range(radio_count):
                if workers[idx] is not None:
                    continue ## running
                disable_sig_handler()
                workers[idx] = mp.Process(target=scripted_worker,
                    args=(worker_n,radio_args[idx],
                            available_radio_profiles[idx]['profiles'],
                            seed_sets[idx],
                            worker_lookups))
                workers[idx].start()
                # print(worker_n,"launched with",len(available_radio_profiles[idx]['profiles']),"profiles")
                enable_sig_handler()
                worker_n += 1
        if any([not x.is_alive() for x in workers if x is not None]):
            for idx in range(radio_count):
                if workers[idx] is None:
                    continue
                if workers[idx].is_alive():
                    continue
                # print("Worker is done---",idx)
                workers[idx].join()
                workers[idx].close()
                workers[idx] = None
    # print("scripted_run",worker_n,reasons_to_loop(),(worker_lookups['end_time'],get_time()))
    log_c.log(c_logger.level_t.INFO,"scripted_worker{0} ending reasons: (EarlyTerm:{1},"
              "InstanceLimit:{2},Runtime:{3},DebugLimiter:{4})".format(*((worker_n,)+tuple([not x for x in reasons_to_loop()]))))
    # print("Scripted_run is done---",idx)
    for idx in range(radio_count):
        if workers[idx] is not None and workers[idx].is_alive():
            workers[idx].terminate()
    for idx in range(radio_count):
        if workers[idx] is not None:
            workers[idx].join()
            workers[idx].close()
            workers[idx] = None


def scripted_worker(worker_id, uhd_args, profiles, seed, worker_lookup):


    ################## Setup RNG
    rng = np.random.default_rng(seed)
    ################## Identify the device
    tail = uhd_args[uhd_args.find('serial')+7:]
    if(tail.find(',') > 0):
        dev_serial = tail[:tail.find(',')]
    else:
        dev_serial = tail

    ################## Handle initialization
    json_template = worker_lookup['json_template']
    runtime = worker_lookup['runtime']
    instance = None
    if 'toggles' in worker_lookup:
        toggles = worker_lookup['toggles']
    else:
        toggles = dict()
    server_size = worker_lookup['server_size']

    from wfgen import logger_client, fake_log, c_logger
    use_log = False if 'use_log' not in worker_lookup else worker_lookup['use_log']
    if use_log:
        log_c = logger_client("{0!s}".format("scripted_worker[{}]".format(worker_id)))
    else:
        log_c = fake_log("{0!s}".format("scripted_worker[{}]".format(worker_id),cout=False))

    log_c.log(c_logger.level_t.INFO,"Observed toggles: {}".format(toggles))
    log_c.log(c_logger.level_t.INFO,"Server Position : {} / {}".format(worker_id,server_size))

    signal_timing = [x['timing'] if x['timing'][0] is not None else None for x in profiles]
    # print(worker_id,"Sig timing:",len(signal_timing))
    log_c.log(c_logger.level_t.INFO,"Sig timing: {}".format(len(signal_timing)))
    profiles = [x['signal'] for x in profiles]
    # print(worker_id,"profiles:",len(profiles))
    log_c.log(c_logger.level_t.INFO,"profiles: {}".format(len(profiles)))
    timed_profiles = [[x,y] for x,y in zip(profiles,signal_timing) if y is not None]
    timed_profiles = sorted(timed_profiles,key=lambda x: x[1][0])
    if 'divide_configs' in toggles and toggles['divide_configs']:
        timed_profiles = timed_profiles[worker_id::server_size]
    # print(worker_id,"Timed Profiles:",len(timed_profiles))
    log_c.log(c_logger.level_t.INFO,"Timed Profiles: {}".format(len(timed_profiles)))
    random_profiles = [x for x,y in zip(profiles,signal_timing) if y is None]
    if 'divide_configs' in toggles and toggles['divide_configs']:
        random_profiles = random_profiles[worker_id::server_size]
    # print(worker_id,"Random Profiles:",len(random_profiles))
    log_c.log(c_logger.level_t.INFO,"Random Profiles: {}".format(len(random_profiles)))
    time_of_interest = [y for x,y in timed_profiles]
    # print(worker_id,"Time of interest:",len(time_of_interest))
    log_c.log(c_logger.level_t.INFO,"Time of interest: {}".format(len(time_of_interest)))
    sent_timed_indicies = []

    # print(signal_timing)
    # print(timed_profiles)
    # print(random_profiles)

    timed_iter = iter(timed_profiles)
    time_buffer = 0.1
    time_buffer_hopper = 19.0
    time_slack = 20.0

    early_terminate = 0
    early_exit_occured = False
    def sig_handler(sig,frame):
        nonlocal early_terminate,worker_id,early_exit_occured
        # print("I've been called (worker_id:{})".format(worker_id))
        early_terminate += 1
        early_exit_occured = True
    default_term_handler = signal.getsignal(signal.SIGTERM)
    default_int_handler = signal.getsignal(signal.SIGINT)


    def enable_sig_handler():
        sig_set = {signal.SIGINT,signal.SIGTERM}
        for sig in sig_set:
            signal.signal(sig,sig_handler)
    def disable_sig_handler():
        nonlocal default_term_handler,default_int_handler
        signal.signal(signal.SIGINT,default_int_handler)
        signal.signal(signal.SIGTERM,default_term_handler)

    enable_sig_handler()

    start_time_extracted = False
    def reasons_to_loop():
        nonlocal early_terminate,worker_lookup
        return (
            early_terminate == 0, ## was told to exit
            (get_time() < worker_lookup['end_time'] \
                if worker_lookup['end_time'] is not None else True) ## Loop if end_time isn't set
                ### otherwise make sure we're not overtime
        )

    def incr_instance():
        nonlocal instance,worker_lookup
        worker_lookup['semaphore'].acquire()
        if worker_lookup['instance'] >= worker_lookup['instance_limit']:
            worker_lookup['semaphore'].release()
            return False
        instance = worker_lookup['instance']
        worker_lookup['instance'] = instance + 1
        worker_lookup['semaphore'].release()
        return True

    system_start_time = None
    system_end_time = None
    signal_start_time = None
    signal_end_time = None
    patience = 60.0
    profile_config = None
    iteration_number = -1
    offset_time = get_time() if worker_lookup['start_time'] is None else worker_lookup['start_time']
    while all(reasons_to_loop()):
        # print(instance,worker_id,"A",runtime,worker_lookup['start_time'],worker_lookup['end_time'])
        # worker_lookup['semaphore'].acquire()
        # if worker_lookup['instance'] >= worker_lookup['instance_limit']:
        #     worker_lookup['semaphore'].release()
        #     break
        # instance = worker_lookup['instance']
        # worker_lookup['instance'] = instance + 1
        # worker_lookup['semaphore'].release()

        if profile_config is None and timed_profiles:
            ## make a timed profile if within the timing
            # profile_config,time_boundary = get_next_timed_set()
            try:
                profile_config,time_boundary = next(timed_iter)
                orig_config = deepcopy(profile_config)
                signal_start_time = None
                signal_end_time = None
                iteration_number += 1
            except StopIteration:
                pass
        if profile_config is None and random_profiles:
            ## make a random profile if there are any
            profile_config = rng.choice(profiles)
            time_boundary = None
            orig_config = deepcopy(profile_config)
            signal_start_time = None
            signal_end_time = None
            iteration_number += 1
        
        if profile_config is not None:
            if 'mode' not in profile_config:
                profile_config['mode'] = 'static'
            is_hopper = profile_config['mode'] == 'hopper'

            if time_boundary is not None:
                hard_start = system_start_time if system_start_time is not None else offset_time
                current_time = get_time() - hard_start
                if not (current_time >= time_boundary[0] - (time_buffer if not is_hopper else time_buffer_hopper)):
                    continue
                print("Advanced because of:",current_time,">=",time_boundary[0] - (time_buffer if not is_hopper else time_buffer_hopper))
                if current_time > hard_start+time_boundary[1]:
                    profile_config = None
                    continue
            print("TIMED BOUNDARY",is_hopper,time_boundary)
            print(profile_config)
            # print(profile_config)
            if not incr_instance():
                break
            profile_name = profile_config['profile']
            prof = extract_profile_by_name(profile_name)
            replay = profile_name in get_replay_profile_names()
            if replay:
                profile_config['mode'] = 'replay'
            elif 'instance_name' in profile_config:
                profile_config['mode'] = 'replay'
                # if 'bw' in profile_config and profile_config['bw'] < 1.0:
                #     profile_config['rate'] = profile_config['energy_bw']/profile_config['bw']
                #     profile_config['bw'] = profile_config['energy_bw']

            json_truth = json_template.format(serial=dev_serial,instance=instance)
            profile_config['json'] = json_truth
            # print(instance,worker_id,'B',prof,profile_config)

            try:
                profile_config = resolve_signal_choices(profile_config,rng)
            except Exception as e:
                #### most likely something I haven't implemented yet
                print(str(e))
                continue

            # print("Original Keys:",profile_config.keys())
            profile_config = translate_config(profile_config,rng,log_c)
            # print("Translated Keys:",profile_config.keys())
            import pprint
            # log_c.log(c_logger.level_t.DEBUG,pprint.pformat(profile_config))

            # print(instance,worker_id,'C',prof,profile_config)
            if 'duration' in profile_config and system_start_time is None:
                ### haven't started anything, so duration should be upper limited by runtime
                if profile_config['duration'] is not None and profile_config['duration'] > runtime:
                    profile_config['duration'] = runtime
            elif 'duration' in profile_config and system_start_time is not None:
                sanity_time_check = get_time()
                if profile_config['duration'] is not None and profile_config['duration'] - time_slack > runtime + system_start_time - sanity_time_check:
                    profile_config['duration'] = runtime + system_start_time - sanity_time_check

            launch_env = os.environ.copy()
            # print(instance, worker_id,profile_config)
            launch_command = prof.start("-a {0:s}".format(uhd_args),profile_config)
            # print(instance, worker_id,launch_command)
            disable_sig_handler()
            if 'json' not in launch_command:
                print("Hmm json wasn't given, skipping.")
                profile_config = None
                continue
            command = shlex.split(launch_command)
            if not command:
                profile_config = None
                continue
            log_c.log(c_logger.level_t.INFO,"starting new radio_task({}): {}".format(iteration_number,command))
            proc = subprocess.Popen(command,shell=False,env=launch_env)
            enable_sig_handler()
            crit_err_check = get_time()

            #################################################################
            if crit_err_check != True:
                ### Python side is up, let's wait for C-USRP to get going now
                while(not os.path.isfile(json_truth)):
                    # waiting for the usrp to spin up im the exec/proc
                    if get_time() - crit_err_check > patience:
                        crit_err_check = True
                        # Hmm C-USRP didn't get started in my patience
                        break
            # print(instance,worker_id,'E',[],crit_err_check)
            if crit_err_check == True:
                ### Something didn't start up correctly, tear it all down
                ### Uh oh, patience has ended
                if proc.poll() is None:
                    log_c.log(c_logger.level_t.DEBUG,"KILLING AT REASON 2")
                    proc.send_signal(signal.SIGINT)
                else:
                    log_c.log(c_logger.level_t.DEBUG,"IS DEAD REASON 2")
            else:
                ######## Everything is going well
                ### json has been created, so hopefully the system will start transmitting soon so rough est of start time
                sig_start_at = get_time() if signal_start_time is None else signal_start_time
                ######################
                if not start_time_extracted:
                    ### every worker will need to extract the start time and end time
                    worker_lookup['semaphore'].acquire()
                    if worker_lookup['start_time'] is None:
                        worker_lookup['start_time'] = system_start_time = sig_start_at + (0 if not is_hopper else time_buffer_hopper) 
                        worker_lookup['end_time'] = system_end_time = sig_start_at + runtime + (0 if not is_hopper else time_buffer_hopper) 
                    else:
                        system_start_time = worker_lookup['start_time']
                        system_end_time = worker_lookup['end_time']
                    worker_lookup['semaphore'].release()
                    start_time_extracted = True

                if time_boundary is not None:
                    signal_start_time = sig_start_at+1.0# + (0 if not is_hopper else time_buffer_hopper) #### A hopper signal will start [4,60]s after the program starts
                    if signal_end_time is None: ### most likely repeating a signal with different parameters, hold it's original end time
                        signal_end_time = signal_start_time + (time_boundary[1]-time_boundary[0]) #### todo fixme not signal_start_time, but rather json creation time
                        if signal_end_time > system_start_time + time_boundary[1]:
                            signal_end_time = system_start_time + time_boundary[1]
                else:
                    signal_start_time = sig_start_at+1.0# + (0 if not is_hopper else time_buffer_hopper) #### A hopper signal will start [4,60]s after the program starts
                    if signal_end_time is None: ### most likely repeating a signal with different parameters, hold it's original end time
                        signal_end_time = system_end_time
                ######################
                log_c.log(c_logger.level_t.DEBUG,"SystemStart({0}), SignalStart({1}), Current({2}), SignalEnd({3}) Target({4})".format(
                    0, signal_start_time-system_start_time, get_time()-system_start_time,#+ (0 if not is_hopper else time_buffer_hopper),
                    signal_end_time-system_start_time,time_boundary))
                early_exit_occured = False
                def additional_keep_looping():
                    return (reasons_to_loop() \
                        + (get_time() < signal_end_time,not early_exit_occured))
                while all(additional_keep_looping()):
                    #### let the signal run until signal duration is up
                    if proc.poll() is not None:
                        #### this process has ended on it's own??
                        log_c.log(c_logger.level_t.DEBUG,"IS DEAD REASON 0")
                        early_exit_occured = True
                        break
                # print(instance,worker_id,'F',additional_keep_looping(),early_exit_occured)
                # print("radio_task",instance,worker_id,"end",additional_keep_looping())###why did this radio_task end?
                log_c.log(c_logger.level_t.INFO,"radio_task ending reasons: (EarlyTerm:{0},"
                        "Runtime:{1},Duration:{2},EarlyExit:{3})".format(*[not x for x in additional_keep_looping()]))
                if proc.poll() is None:
                    ### time to wrap up now signal
                    log_c.log(c_logger.level_t.DEBUG,"KILLING AT REASON 1")
                    proc.send_signal(signal.SIGINT)
                else:
                    if not any(additional_keep_looping()):
                        log_c.log(c_logger.level_t.DEBUG,"IS DEAD REASON 3")
            # print(instance,worker_id,'G')
            proc.wait() ### wait for it to write out the truth
            # print(instance,worker_id,'H')
            if time_boundary is None:
                profile_config = None
                log_c.log(c_logger.level_t.INFO,"Finished current signal(1)")
            else:
                if get_time() < signal_end_time if signal_end_time is not None else float('inf'):#(signal_end_time-(time_buffer if not is_hopper else time_buffer_hopper)):
                    profile_config = deepcopy(orig_config)
                    log_c.log(c_logger.level_t.INFO,"Continuing with another iteration of the same signal")
                else:
                    
                    profile_config = None
                    log_c.log(c_logger.level_t.INFO,"Finished current signal(2) {} {} {}".format(get_time(),signal_end_time,(signal_end_time-(time_buffer if not is_hopper else time_buffer_hopper))))
            if profile_config is None:
                log_c.log(c_logger.level_t.INFO, "Ending radio task: {}".format(iteration_number))

    # print("scripted_worker",instance,worker_id,"end",reasons_to_loop())###why did this proc end?
    log_c.log(c_logger.level_t.INFO,"scripted_worker-instance{0} ending reasons: (EarlyTerm:{1},"
            "Runtime:{2})".format(*((instance,)+tuple([not x for x in reasons_to_loop()]))))
    # print("Scripted_worker is done---",worker_id)

def resolve_signal_choices(base_config,rng):
    resolved_config = dict()
    key_list = sorted(base_config.keys())
    for key in key_list:
        if key.startswith("rx_"):
            continue
        item = base_config[key]
        finalized = False
        while not finalized:
            if isinstance(item,list):
                item = rng.choice(item)
            elif isinstance(item,dict):
                if 'dist' in item:
                    if 'kind' in item and item['kind'] == 'static':
                        if(item['dist'] == "uniform"):
                            item = rng.uniform(item['min'],item['max'])
                        if(item['dist'] == "log_uniform"):
                            item = np.power(10.,rng.uniform(np.log10(item['min']),np.log10(item['max'])))
                        elif(item['dist'] == "gaussian"):
                            item = rng.normal(item['loc'],item['scale'])
                        elif(item['dist'] == "exponential"):
                            item = rng.exponential(item['scale'])
                        else:
                            raise RuntimeError("unknown dist requested")
                    else:
                        ## going to assume dynamic, which means the waveform needs the options
                        item = yaml.dump(item)
                else:
                    ## Assuming this can be ignored
                    print(type(item),item)
                    finalized = True
            elif isinstance(item,(int,float,str)) or item is None:
                finalized = True
        resolved_config[key] = item
    return resolved_config

def resolve_random_space(var,rng):
    if isinstance(var,dict):
        print(var)
        if 'dist' in var:
            if var['dist'] == "log_uniform":
                return np.power(10.,rng.uniform(np.log10(var['min']),np.log10(var['max'])))
            else:
                return rng.uniform(var['min'],var['max'])
        ## assuming uniform for now
        return rng.uniform(var['min'],var['max'])
    else:
        return var
def translate_config(config_a:Dict,rng:RNG_TYPE,log_c=None) -> Dict:
    from wfgen import c_logger
    # import pprint
    # pprint.pprint(config_a)
    config_w = deepcopy(config_a) ## working config
    def try_sanitize(cfg):
        if isinstance(cfg,dict):
            for k,v in cfg.items():
                cfg[k] = try_sanitize(v)
        elif isinstance(cfg,list):
            for i,v in enumerate(cfg):
                cfg[i] = try_sanitize(v)
        elif isinstance(cfg,str):
            is_num = True
            try:
                nv = float(cfg)
            except:
                is_num = False
            if is_num:
                if np.abs(nv - np.floor(nv)) > 5*np.finfo(np.single):
                    return nv
                try:
                    nv2 = int(nv)
                except:
                    return nv
                return nv2
            else:
                try:
                    ncfg = yaml.safe_load(cfg)
                except:
                    return cfg
                return ncfg
        return cfg
    print('before:',config_w)
    config_w = try_sanitize(config_w)
    print('after:',config_w)

    hopper = False
    def_rate = None
    if 'mode' in config_w and config_w['mode'].lower() in ['hopper','bursty']:
        mode = "HOPPER"
        hopper = True
        key_mapper = {
            ('band_center','freq_lo','freq_hi','span','rate'):('frequency','rate'),
            'gain':'gain',
            'bw':'bw',
            'idle':'loop_delay',
            ('duration','signal_limit'):('duration',),
            'mod_index':'mod_index',
            'cpfsk_type':'cpfsk_type',
            'symbol_rate':'symbol_rate',
            ('dwell','absence','period'):('period','dwell'),
            'linear_hop':'linear_hop',
            'mode':'hopper',
            'json':'json',
            'profile':'profile'
        }
        if 'rate' not in config_w:
            if 'energy_bw' in config_w:
                def_rate = config_w['energy_bw']/config_w['bw']
            else:
                def_rate = 40e6
    elif 'mode' in config_w and config_w['mode'] == 'replay':
        mode = "REPLAY"
        key_mapper = {
            ('center_freq','reference_freq','freq_hi','freq_lo','sample_rate'):('frequency','span','rate','bw'),
            ('power',):('gain',),
            ('energy_set',):('hopper','bursty','num_bursts','num_loops'),
            ('time_start','time_stop'):('duration','time_start','time_stop'),
            ('avg_period','avg_dwell'):('period','dwell'),
            'json':'json',
            'profile':'profile'
        }
        if log_c:
            log_c.log(c_logger.level_t.DEBUG,"Incoming keys: {0!s}".format(list(config_w.keys())))
        if 'sample_rate' not in config_w:
            if 'energy_bw' in config_w:
                def_rate = config_w['energy_bw']/config_w['bw']
            else:
                def_rate = 1e6
        else:
            def_rate = config_w['sample_rate']
    else:
        mode = "OTHER"
        key_mapper = {
            ('band_center','freq_lo','freq_hi','span','rate'):('frequency','rate'),
            'gain':'gain',
            'bw':'bw',
            'duration':'duration',
            'mod_index':'mod_index',
            'cpfsk_type':'cpfsk_type',
            'symbol_rate':'symbol_rate',
            'src_fq':'src_fq',
            'json':'json',
            'profile':'profile'
        }
        if 'rate' not in config_w:
            def_rate = 100e3


    config_b = {
        "frequency"     : None,
        "rate"          : None,
        "gain"          : None,
        "bw"            : None,
        "span"          : None,
        "loop_delay"    : None,
        "duration"      : None,
        "mod_index"     : None,
        "cpfsk_type"    : None,
        "symbol_rate"   : None,
        "period"        : None,
        "dwell"         : None,
        "linear_hop"    : None,
        "num_bursts"    : None,
        "num_loops"     : None,
        "time_start"    : None,
        "time_stop"     : None,
        'src_fq'        : None,
        "hopper"        : None,
        "bursty"        : None,
        "json"          : None,
        "profile"       : None,
    }


    for key_set in key_mapper.keys():
        process_this = False
        is_tuple=False
        if isinstance(key_set,tuple):
            process_this = any([x in config_w for x in key_set])
            is_tuple=True
        else:
            process_this = key_set in config_w
        if not process_this:
            if isinstance(key_mapper[key_set],tuple):
                for key_p in key_mapper[key_set]:
                    if key_p in config_b:
                        del config_b[key_p]
            elif key_mapper[key_set] in config_b:
                del config_b[key_mapper[key_set]]
            continue
        if is_tuple:
            if mode == "REPLAY" and isinstance(key_mapper[key_set],tuple):
                if 'center_freq' in key_set:
                    if 'center_freq' in config_w:
                        config_b['frequency'] = config_w['center_freq']*1e6
                    elif "reference_freq" in config_w:
                        config_b['frequency'] = config_w['reference_freq']*1e6
                    else:
                        config_b['frequency'] = 2.45e9
                    if 'sample_rate' not in config_w:
                        config_w['sample_rate'] = def_rate
                    config_b['rate'] = config_w['sample_rate']
                    if config_w['modality'] == 'frequency_agile':
                        config_b['span'] = int((config_w['freq_hi']-config_w['freq_lo'])*1e6)/config_b['rate']
                        config_b['bw'] = 0.7#*config_w['sample_rate']
                    else:
                        config_b['span'] = int((config_w['freq_hi']-config_w['freq_lo'])*1e6)/config_b['rate']
                        config_b['bw'] = config_b['span']
                    if config_b['bw'] < 0.001:
                        config_b['bw'] = 0.0011
                if 'power' in key_set:
                    config_b['gain'] = int(config_w['power']*85)
                if 'energy_set' in key_set:
                    config_b['bursty'] = (config_w['modality'] != 'frequency_agile') and config_w['energy_set'] > 1
                    config_b['hopper'] = (config_w['modality'] == 'frequency_agile')
                    ## FIXME: hack since bursty really isn't developed yet
                    if config_b['bursty'] and not config_b['hopper']:
                        config_b['channels'] = 1
                    config_b['hopper'] |= config_b['bursty']
                    config_b['num_bursts'] = config_w['energy_set']
                    config_b['num_loops'] = 1
                if 'time_start' in key_set:
                    config_b['time_start'] = config_w['time_start']
                    config_b['duration'] = config_w['time_stop']-config_w['time_start']
                    config_b['time_stop'] = config_w['time_stop']
                if 'avg_period' in key_set:
                    config_b['period'] = config_w['avg_period']
                    config_b['dwell'] = config_w['avg_dwell']
            elif mode == "HOPPER" and isinstance(key_mapper[key_set],tuple):
                if 'band_center' in key_set:
                    if ('band_center' in config_w and config_w['band_center'] is not None) and \
                    ('span' in config_w and config_w['span'] is not None):
                        f_bound = [config_w['band_center']-config_w['span']/2,
                                config_w['band_center']+config_w['span']/2]
                    elif ('freq_lo' in config_w and config_w['freq_lo'] is not None) and \
                        ('freq_hi' in config_w and config_w['freq_hi'] is not None):
                        f_bound = [config_w['freq_lo'],config_w['freq_hi']]
                    else:
                        raise ValueError("Must specify joint keys ('band_center','span') or ('freq_lo','freq_hi')")
                    if 'rate' in config_w and config_w['rate'] is not None:
                        if(f_bound[0]+config_w['rate']/2 >= f_bound[1]-config_w['rate']/2):
                            config_b['frequency'] = (f_bound[0]+f_bound[1])/2
                        else:
                            config_b['frequency'] = resolve_random_space({"min":f_bound[0]+config_w['rate']/2,"max":f_bound[1]-config_w['rate']/2},rng)
                        config_b['rate'] = config_w['rate']
                    else:
                        config_b['frequency'] = (f_bound[0]+f_bound[1])/2
                        config_b['rate'] = f_bound[1]-f_bound[0]
                if 'duration' in key_set:
                    ### duration should be the duration of the 'signal'
                    ### while signal_limit is how many of these it should send out
                    ### so total duration = signal_limit*duration + idle*(signal_limit-1)
                    if 'duration' in config_w:
                        idle = 0.05 if 'idle' not in config_w else config_w['idle']
                        signal_limit = 1 if 'signal_limit' not in config_w or config_w['signal_limit'] is None else config_w['signal_limit']
                        config_b['duration'] = signal_limit*(config_w["duration"]+idle)-idle
                    elif 'signal_limit' in config_w:
                        raise ValueError("'signal_limit' is specified and 'duration' isn't (hopper:{})".format('hopper' in config_b))
                    else:
                        print("Surprised occurance 3")
                if 'dwell' in key_set:
                    ### So how long should it transmit?
                    got_absence = 'absence' in config_w
                    got_dwell = 'dwell' in config_w
                    got_period = 'period' in config_w
                    if got_period and got_dwell:
                        config_b['period'] = config_w['period']
                        config_b['dwell'] = config_w['dwell']
                    elif got_period and got_absence:
                        config_b['period'] = config_w['period']
                        config_b['dwell'] = config_w['period']-config_w['absence']
                    elif got_dwell and got_dwell:
                        config_b['period'] = config_w['dwell'] + config_w['absence']
                        config_b['dwell'] = config_w['dwell']
                    elif got_period:
                        config_b['period'] = config_w['period']
                        config_b['dwell'] = config_w['period']
                    elif got_dwell:
                        config_b['period'] = config_w['dwell']
                        config_b['dwell'] = config_w['dwell']
                    elif got_absence:
                        config_b['period'] = config_w['absence']*2
                        config_b['dwell'] = config_w['absence']
                    else:
                        absence = 0
                        dwell = 0.02
                        period = 0.02
                        config_b["period"] = absence+dwell
                        config_b["dwell"] = dwell
            elif mode == "OTHER" and isinstance(key_mapper[key_set],tuple):
                if 'band_center' in key_set:
                    if ('band_center' in config_w and config_w['band_center'] is not None) and \
                    ('span' in config_w and config_w['span'] is not None):
                        f_bound = [config_w['band_center']-config_w['span']/2,
                                config_w['band_center']+config_w['span']/2]
                    elif ('freq_lo' in config_w and config_w['freq_lo'] is not None) and \
                        ('freq_hi' in config_w and config_w['freq_hi'] is not None):
                        f_bound = [config_w['freq_lo'],config_w['freq_hi']]
                    else:
                        raise ValueError("Must specify joint keys ('band_center','span') or ('freq_lo','freq_hi')")
                    if 'rate' in config_w and config_w['rate'] is not None:
                        if(f_bound[0]+config_w['rate']/2 >= f_bound[1]-config_w['rate']/2):
                            config_b['frequency'] = (f_bound[0]+f_bound[1])/2
                        else:
                            config_b['frequency'] = resolve_random_space({"min":f_bound[0]+config_w['rate']/2,"max":f_bound[1]-config_w['rate']/2},rng)
                        config_b['rate'] = config_w['rate']
                    else:
                        config_b['frequency'] = (f_bound[0]+f_bound[1])/2
                        config_b['rate'] = f_bound[1]-f_bound[0]
            else:
                for key_p in key_set:
                    if key_p in config_w:
                        config_w[key_p] = resolve_random_space(config_w[key_p],rng)
        else:
            if key_set == 'mode': ## special case
                config_b['hopper'] = config_w[key_set] == 'hopper'
                # config_b['replay'] = config_w[key_set] == 'replay'
                continue
            config_w[key_set] = resolve_random_space(config_w[key_set],rng)
    # if mode == 'REPLAY':
    #     if 'bursty' in config_b and config_b['bursty']:
    #         #### FIXME: tweaks for "BURSTY"
    #         if config_b['span'] > def_rate:
    #             def_rate = int(np.ceil(config_b['span']/1e6))*1e6
    #         config_b['bw'] = config_b['span']/def_rate
    #     if any([x in config_b for x in ['hopper','burst']]) and ('process' in config_w and 'tone' in config_w['process']):
    #         config_b['bw'] = 0.7 #### failsafe until bursty/hopper tone is available
    ## All working values are resolved (hopefully)
    import pprint
    for key_set in key_mapper.keys():
        if isinstance(key_set,tuple):
            process_this = any([x in config_b for x in key_mapper[key_set]])
        else:
            process_this = key_mapper[key_set] in config_b
        if not process_this:
            continue
        if isinstance(key_set,tuple):
            ### hopper must be TRUE here (at the moment)
            if key_set[0] == 'band_center':
                pass
            elif key_set[0] == "duration":
                pass
            elif key_set[0] == 'dwell':
                pass
            elif key_set[0] == 'center_freq':
                pass
            elif key_set[0] == "power":
                pass
            elif key_set[0] == "energy_set":
                pass
            elif key_set[0] == "time_start":
                pass
            elif key_set[0] == "avg_period":
                pass
            else:
                raise RuntimeError("Someone forgot to update the key_mapper here(2)")
        else:
            ### could be a hopper or continuous
            if key_set == 'band_center':
                if 'frequency' in config_w and int(config_w['band_center']) != int(config_w['frequency']):
                    warnings.warn(RuntimeWarning("'band_center' and 'frequency' defined and not equal, "
                            "setting 'frequency' to 'band_center'"))
                config_b[key_mapper[key_set]] = config_w[key_set]
            elif key_set == 'frequency':
                if 'band_center' in config_w and int(config_w['frequency']) != int(config_w['band_center']):
                    warnings.warn(RuntimeWarning("'band_center' and 'frequency' defined and not equal, "
                            "setting 'frequency' to 'band_center'"))
                else:
                    config_b[key_mapper[key_set]] = config_w[key_set]
            else:
                config_b[key_mapper[key_set]] = config_w[key_set]
    if 'bw' in config_b and config_b['bw'] is not None:
        if config_b['bw'] > 1.0:
            config_b['bw'] = config_b['bw'] / (config_b['rate'] if 'rate' in config_b else def_rate)
    if 'hopper' in config_b and config_b['hopper']:
        if not hasattr(locals(),'signal_limit') or (hasattr(locals(),'signal_limit') and signal_limit is None):
            signal_limit = 1
        if 'loop_delay' not in config_b:
            config_b['loop_delay'] = 0.05
        if 'linear_hop' not in config_b:
            config_b['linear_hop'] = None
        # pprint.pprint(config_b)
        if 'num_bursts' not in config_b or config_b['num_bursts'] is None:
            if 'duration' in config_b:
                if 'period' in config_b and config_b['period'] is None or 'period' not in config_b:
                    if ('dwell' in config_b and config_b['dwell'] is not None) and ('absence' in config_b and config_b['absence'] is not None):
                        config_b['period'] = config_b['dwell']+config_b['absence']
                    elif ('dwell' in config_b and config_b['dwell'] is not None):
                        config_b['period'] = config_b['dwell']
                    elif ('absence' in config_b and config_b['absence'] is not None):
                        config_b['period'] = config_b['absence']*2
                        config_b['dwell'] = config_b['absence']
                    idle = 0.05 if 'idle' not in config_w else config_w['idle']
                    dur = (config_b['duration'] +idle)/signal_limit - idle
                    period = config_b['period']
                    if log_c:
                        log_c.log(c_logger.level_t.DEBUG,"dur({}) period({})".format(dur,period))
                    config_b['num_bursts'] = int(np.ceil(dur//period))
                    config_b['num_loops'] = 1
    # log_c.log(c_logger.level_t.DEBUG,"Xlating - Input Config:\n{}".format(pprint.pformat(config_w,indent=2)))
    # log_c.log(c_logger.level_t.DEBUG,"Xlating - Output Config:\n{}".format(pprint.pformat(config_b,indent=2)))
    return config_b





def visualize_script(filepath,off_mark='_',on_mark='|',joiner='/',bins=100,name_toggle=0,time_limit=-1):
    runtime,energies,signals,sources,flags,toggles = load_script(filepath)
    if bins <= 0:
        bins = int(np.ceil(runtime)) # number of seconds of the test
    unit = (runtime if time_limit <= 0 or np.isinf(time_limit) else time_limit)/bins
    signals = sorted(signals,key=lambda x: x['time_start'] if 'time_start' in x else 0)
    if time_limit > 0 and not np.isinf(time_limit):
        signals = [x for x in signals if (x['time_start'] if 'time_start' in x else 0) < time_limit]
    sources = [(x['instance_name'],x['device_origin'],x['signal_set']) for x in sources]
    # print(os.path.basename(filepath),flags)
    if flags == 1:
        keys = ['process']
    elif flags in [2,3]:
        keys = ['modality','modulation']
    else:
        keys = []
    return quick_viz(signals,keys,unit,sources,off_mark,on_mark,joiner,bins,name_toggle,time_limit)
    # if name_toggle:
    #     name_toggle = 1 # device origin (some of these are blank)
    # else:
    #     name_toggle = 0 # instance name
    # lines = ['' for _ in range(len(signals))]
    # for idx,sig in enumerate(signals):
    #     if(sig['time_start'] > runtime):
    #         break
    #     name = sig['instance_name']
    #     src = [x[name_toggle] for x in sources if name in x[2]]
    #     if src and len(src[0]) == 0:
    #         src = [x[0] for x in sources if name in x[2]]
    #     start_point = int(np.round(sig['time_start']/unit))
    #     end_point = int(np.round(sig['time_stop']/unit))
    #     if(end_point == start_point):
    #         if start_point == bins:
    #             start_point -= 1
    #         else:
    #             end_point += 1
    #     if end_point > bins:
    #         end_point = bins
    #     line = off_mark*start_point + on_mark*(end_point-start_point) + off_mark*(bins-end_point)
    #     if keys or src:
    #         dev = ''
    #         k = ''
    #         if src:
    #             dev = '{0:<20s}'.format(src[0])
    #         if keys:
    #             k = '{0:<20s}'.format(joiner.join([sig[x] for x in keys]))
    #         lines[idx] = dev + k + line + " " + str(round(runtime,2))
    # return '\n'.join(lines)

def quick_viz(signals,keys,unit,sources=None,off_mark='_',on_mark='|',joiner='/',bins=100,name_toggle=0,time_limit=-1):
    if name_toggle:
        name_toggle = 1 # device origin (some of these are blank)
    else:
        name_toggle = 0 # instance name
    lines = ['' for _ in range(len(signals))]

    runtime = bins*unit if time_limit <= 0 or np.isinf(time_limit) else time_limit
    key_spacing = -1
    src_spacing = -1

    proto = 'inst{0:03d}'
    proto_idx = 0

    if sources:
        for idx,sig in enumerate(signals):
            if 'instance_name' in sig:
                name = sig['instance_name']
            else:
                proto.format(proto_idx)
                proto_idx += 1

            src = [x[name_toggle] for x in sources if name in x[2]]
            if src and len(src[0]) == 0:
                src = [x[0] for x in sources if name in x[2]]
            if src:
                dev = '{0!s}'.format(src[0])
                src_spacing = max(src_spacing,len(dev))
        src_spacing += 1
    if keys:
        for idx,sig in enumerate(signals):
            k = joiner.join([sig[x] for x in keys if x in sig])
            key_spacing = max(key_spacing,len(k))
        key_spacing += 1
    proto_idx = 0
    for idx,sig in enumerate(signals):
        if 'time_start' in sig and (sig['time_start'] > runtime):
            lines = lines[:idx]
            break
        if 'instance_name' in sig:
            name = sig['instance_name']
        else:
            proto.format(proto_idx)
            proto_idx += 1
        if sources:
            src = [x[name_toggle] for x in sources if name in x[2]]
            if src and len(src[0]) == 0:
                src = [x[0] for x in sources if name in x[2]]
        else:
            src = []
        start_point = int(np.round(sig['time_start']/unit)) if 'time_start' in sig else 0
        end_point = int(np.round(sig['time_stop']/unit)) if 'time_stop' in sig else int(unit*bins)
        if(end_point == start_point):
            if start_point == bins:
                start_point -= 1
            else:
                end_point += 1
        if end_point > bins:
            end_point = bins
        line = off_mark*start_point + on_mark*(end_point-start_point) + off_mark*(bins-end_point)
        if keys or src:
            dev = ''
            k = ''
            if src:
                dev = '{' + '0:<{}s'.format(src_spacing) + '}'
                dev = dev.format(src[0])
            if keys:
                k = '{' + '0:<{}s'.format(key_spacing) + '}'
                k = k.format(joiner.join([sig[x] for x in keys if x in sig]))
            lines[idx] = dev + k + line + " " + str(round(runtime,2))
        else:
            lines[idx] = line + " " + str(round(runtime,2))
    return '\n'.join(lines)

def needed_radios_for_script(filepath):
    viz = visualize_script(filepath,'_','|',bins=-1)
    if len(viz.split('\n')) == 1:
        return 1
    quant = np.vstack([np.frombuffer(x[40:].encode('latin-1'),np.uint8) for x in viz.split('\n')])
    quant[quant==ord('_')] = 0
    quant[quant==ord('|')] = 1
    try:
        nr = np.max(np.sum(quant,axis=0))
    except:
        print(quant.shape)
        raise
    return nr


class radio_plan_tracker(object):
    def __init__(self,radio_idx,dev):
        self.radio_idx = radio_idx
        self.dev = dev
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

def debug_server_scripting(uhd_args=[],script='truth_scripts/output-1a-truth.json'):
    if not isinstance(uhd_args,list):
        raise ValueError("'uhd_args' is expected in a list")
    from wfgen.server import ServerState
    from wfgen.profiles import get_traceback_profiles
    from wfgen.utils import get_interface
    server = ServerState('/tmp',get_interface(),50000,'172.21.192.86',56000,uhd_args,None,False,False)
    from wfgen.launcher import get_radios
    server.set_radios(get_radios(server.uhd_args))
    possible_radios = ['{0:s}'.format(x['args']) for x in server.radios.radios]
    possible_servers = [x for x in server.radios.radio_server_map]
    server_set = dict([(x,list()) for x in sorted(np.unique(possible_servers).tolist())])
    for ri in range(len(possible_radios)):
        server_set[possible_servers[ri]].append(possible_radios[ri])

    runtime,energies,signals,sources,flags,toggles = load_script(script)
    num_radios_neededd = needed_radios_for_script(script)
    og_profs = get_traceback_profiles(script)

    all_radios = []
    radio_servers = []
    for si in range(len(server_set)):
        all_radios.extend(server_set[si])
        radio_servers.extend([si for _ in server_set[si]])

    signals = sorted(signals,key=lambda x: x['time_start'])

    radio_trackers = [None]*len(all_radios)
    for idx,radio in enumerate(all_radios):
        radio_trackers[idx] = radio_plan_tracker(idx, all_radios[idx])

    radio_mapping = dict([(all_radios[_],[]) for _ in range(len(all_radios)) ])
    radio_fallbacks = dict()
    for sig_idx,sig in enumerate(signals):
        radio_idx = None # not assigned to a device yet
        time_window = [sig['time_start'],sig['time_stop']]

        sig_name = sig['instance_name']

        if 'device' in sig:
            # This is a VTNSI script format
            if sig['device'] in all_radios:
                radio_idx = all_radios.index(sig['device'])
                if not radio_trackers[radio_idx].available(*time_window):
                    radio_idx = None
                    continue
            prof_name = ''
            src = ''
            dev = ''
        else:
            # A sponsor truth file
            prof_name = og_profs[sig_name]
            src = [x for x in sources if sig_name in x['signal_set']][0]
            src_name = src['device_origin'] if len(src['device_origin']) else src['instance_name']
            if src_name in radio_fallbacks and radio_trackers[radio_fallbacks[src_name][0]].available:
                radio_idx,dev = radio_fallbacks[src_name]
            else:
                ## random deal to a radio
                possible = len(all_radios)
                r_idx = np.random.randint(possible)
                checking = 0
                while not radio_trackers[r_idx].available(sig['time_start'],sig['time_stop']):
                    r_idx = (r_idx + 1) % possible
                    checking += 1
                    if checking == possible:
                        r_idx = None
                        break
                if r_idx is not None:
                    dev = all_radios[r_idx]
                    radio_fallbacks[src_name] = (r_idx,dev)
                    radio_idx = r_idx
        if radio_idx is not None:
            radio_mapping[dev].append({'time':time_window,'profile':prof_name,'singal_index':sig_idx})
            radio_trackers[radio_idx].reserve(*time_window)

    return server,server_set,radio_trackers,radio_mapping
