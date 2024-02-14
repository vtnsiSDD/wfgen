
'''run_random radios 0,1 bands 2.4e9,2.5e9 duration_limits 2.0,5.0 burst_dwell_limits 0.05,0.5 bw_limits 50e3,1e6 span_limits 10e6,20e6 profiles qpsk,fsk16'''
'''run_random radios 0,1 bands 5.04e9,5.06e9 duration_limits 2.0,5.0 burst_dwell_limits 1.0,2.0 bw_limits 10e3,1e6 profiles bpsk'''
'''run_random radios 0,1 bands 5.04e9,5.06e9 duration_limits 2.0,5.0 burst_dwell_limits 1.0,2.0 bw_limits 10e3,1e6 profiles qpsk'''
'''run_random radios 0,1 bands 2.45e9,2.47e9 duration_limits 1.0,2.0 bw_limits 200e3,1e6 profiles bpsk'''

import numpy as np
import multiprocessing as mp
from datetime import datetime, timezone
from typing import Dict
import signal
import os
import subprocess
import yaml
import shlex
import time

try:
    from ..profiles import get_all_profile_names,extract_profile_by_name
except ImportError:
    from wfgen.profiles import get_all_profile_names,extract_profile_by_name


_source_limits = ['gain_limits','digital_gain_limits','digital_cycle_limits']
_signal_limits = ['freq_limits','span_limits','duration_limits']
_energy_limits = ['bw_limits','burst_dwell_limits','burst_idle_limits']
_per_profile_toggles = ['disable_source_overwrite','disable_signal_overwrite',
                        'disable_energy_overwrite','extra_key_map']



class NeedRange(object):
    def __init__(self,range_lh):
        self.r = range_lh
    def __getitem__(self,index):
        return self.r[index]

class RandomizedRun(object):
    def __init__(self,duration,bands,profiles=None,randomizations=dict(),rng=None,seed=None):
        self.duration = duration # how long is the run going to continue
        self.bands = bands # list of pairs(low,high) in terms of operation bands

        # This should be a list of strings identifying each profile available to the
        #  run, but if None is the value, assume all profiles are in play
        self.profiles = profiles if profiles is not None else get_all_profile_names()

        # randomizations can be more complex so consider this aspect of the API in flux
        ## First pass assumptions
        ##  randomizations should be a dictionary whose keys are the profile names
        ##  found in self.profiles, with the general key of 'default' that will overwrite
        ##  all signals when an option isn't provided
        ##
        ##    Profiles are assumed to have
        ##        NOTES: mode 'const' is not really tested for anything yet and should
        ##                be expected to have some variation if used at all
        ##        ------- Signal Level Bounds (_source_limits)
        ##        - freq_limits ------------> bands in [(low,high)|const,...] format for operation
        ##        - span_limits ------------> bandwidth of the signal in [(low,high)|const,...] format for operation
        ##        - duration_limits --------> time in seconds that the signal can exist for
        ##            in [(low,high)|const,...] format for operation (const is not operational yet)
        ##        ------- Energy Level Bounds (_signal_limits)
        ##        - bw_limits --------------> bandwidth of waveform in [(low,high)|const,...] 
        ##            format for operation (const is not operational yet)
        ##        - burst_dwell_limits -----> time in seconds that a signal burst can stay on for
        ##            in [(low,high)|const,...] format for operation
        ##        - burst_idle_limits ------> time in seconds that a signal can be off between bursts
        ##            in [(low,high)|const,...] format for operation
        ##        ------- Source/Radio Bounds (_energy_limits)
        ##        - gain_limits ------------> USRP Gain for the radio in [(low,high)|,...] format for operation
        ##        - digital_gain_limits ----> tweaks to the digital gain allowed in dB in 
        ##            [(low,high)|const,...] format for operation
        ##        - digital_cycle_limits ---> time limit over which the digital gain manipulate exists
        ##            before repeating in [(low,high)|const,...] format for operation
        ##
        ##
        ##            ---- More tweaks should be expected as the program develops
        ##
        self.randomizations = randomizations

        self._sanitize_randomization()

        if rng is None:
            rng = np.random.default_rng(seed)

        self._rng = rng ### for repeatability
    
    def _sanitize_randomization(self):
        ### Need to do some checks to make sure hierarchical constraints don't conflict
        pass

    @staticmethod
    def _np_cleaned(np_ish):
        try:
            return np_ish.item()
        except:
            return np_ish

    def get_randomized_profile(self,profile_name=None):
        if profile_name is None:
            profile_name = self.np_cleaned(self._rng.choice(self.profiles))
        profile_limits = self.randomizations['default'].copy()
        if profile_name in self.randomizations:
            profile_limits.update(self.randomizations[profile_name])
        return RandomizeProfile(profile_name,profile_limits)

def np_cleaned(f):
    #### NP items don't serialize too well, so if they're items, return them to python domain
    def wrapper(*args,**kwargs):
        value = f(*args,**kwargs)
        try:
            value = value.item()
        except:
            try:
                value = value.tolist()
            except:
                pass
        return value
    return wrapper

@np_cleaned
def wise_choice(key,limits,rng=None):
    #### might not need the 'key' but leave for ease
    if limits is None:
        return None
    if isinstance(limits,(list,tuple)) and len(limits) == 0:
        return limits
    
    if rng is None:
        rng = np.random.default_rng()

    if isinstance(limits,(list,tuple)) and any([isinstance(x,(list,tuple)) for x in limits]):
        return wise_choice(key,np_cleaned(rng.choice)(limits),rng)

    ## So at this point, assumptions
    #### limits != None
    #### limits != list(0) | tuple(0)
    #### limits != list(list|tuple) | tuple(list|tuple)
    if isinstance(limits,(list,tuple)) and len(limits) != 2:
        if key == 'profiles':
            if len(limits) == 1:
                return limits[0]
            elif len(limits) > 1:
                return rng.choice(limits)
        raise ValueError("DEBUG THIS -- not sure if this should be possible for here")
    if isinstance(limits,(list,tuple)) and isinstance(limits[0],int):
        return rng.integers(limits[0],limits[1],endpoint=True)
    if isinstance(limits,(list,tuple)) and isinstance(limits[0],float):
        return rng.uniform(limits[0],limits[1])
    if isinstance(limits,(list,tuple)) and isinstance(limits[0],str):
        return rng.choice(limits)
    return limits

class RandomizeProfile(object):
    _required_limits = _source_limits + _signal_limits + _energy_limits
    def __init__(self,profile=None,limits=None):
        self.profile = profile
        if any([x not in limits for x in self._required_limits]):
            missing = [x for x in self._required_limits if x not in limits]
            raise ValueError("Missing limit keys: {!s}".format(missing))
        self.random_limits = limits
        if profile is None or limits is None:
            self.initialized = False
        else:
            self.initialized = True
        self.seed = None

    @classmethod
    def from_yaml(cls, yaml_str:str):
        guts = yaml.safe_load(yaml_str)
        if 'profile' not in guts or 'random_limits' not in guts:
            raise ValueError("The yaml string is missing keywords 'profile' or 'random_limits'")
        cls.proflie = guts['profile']
        cls.random_limits = guts['random_limits']
        if cls.profile is not None and cls.random_limits is not None:
            cls.initialized = True
        else:
            cls.initialized = False
        if 'seed' not in guts:
            cls.set_rng()
        else:
            cls.set_rng(seed=guts["seed"])
        return cls

    def to_yaml(self,seed=None):
        guts = {'profile':self.profile,'random_limits':self.random_limits}
        if seed == True:
            ## pass on the seed
            guts['seed'] = self.seed
        elif seed is not None:
            guts['seed'] = seed
        yaml_str = yaml.safe_dump(guts)
        return yaml_str

    def set_rng(self,rng=None,seed=None):
        if seed is None:
            seed = self.seed
        if rng is None:
            rng = np.random.default_rng(seed)
        self._rng = rng

    def create_random_profile(self,rng=None):
        if self.initialized == False:
            return None
        if rng is None:
            rng = self._rng
        profile = extract_profile_by_name(self.profile)()
        for key in self._required_limits:
            value = wise_choice(key,self.random_limits[key],rng)
            if value is not None:
                setattr(profile,key,value)
        return profile


def parse_random_run_reqest(request:Dict,server_state:"ServerState"):
    #### debugging
    import pprint
    pprint.pprint(request)
    #### run settings
    if 'bands' not in request:
        request['bands'] = [[100e6,6e9]]
    if 'runtime' not in request:
        request['runtime'] = 300.0
        print("Random Run 'runtime' wasn't set, setting to 5 minutes")
    if 'profiles' not in request:
        request['profiles'] = None ### don't need to pass this to anyone -- assume all
        possible_checks = get_all_profile_names()
    else:
        possible_checks = request['profiles']
    if 'seed' not in request:
        request['seed'] = None
    if 'instance_limit' not in request:
        request['instance_limit'] = 1000
    elif request['instance_limit'] is None:
        request['instance_limit'] = 1000
    elif not isinstance(request['instance_limit'],int):
        try:
            request['instance_limit'] = int(request["instance_limit"])
        except:
            print("Invalid instance_limit provided in request")
            return None

    #### Check the server can make such a run
    if 'radios' not in request or request['radios'] is None:
        request['radios'] = [x for x in server_state.idle_radios]
    if any([x in server_state.active_radios for x in request['radios']]) or any([x < 0 or x > len(server_state.radios) for x in request['radios']]):
        return "Cannot claim the reqesuted radios"

    if any([x not in get_all_profile_names() for x in request['profiles']]):
        return "Cannot run the requested profiles {0!s}".format([x for x in request['profiles'] if x not in get_all_profile_names()])

    server_state.activate_radios(request['radios'])
    radio_args = [server_state.radios.radios[x]['args'] for x in request['radios']]


    defaults = dict() if 'profile_defaults' not in request else request['profile_defaults']
    check_from = request if 'profile_defaults' not in request else defaults
    #### source limits
    if 'gain_limits' not in check_from:
        defaults['gain_limits'] = 50
    if 'digital_gain_limits' not in check_from:
        defaults['digital_gain_limits'] = 0.0
    if 'digital_cycle_limits' not in check_from:
        defaults['digital_cycle_limits'] = 2.0
    if check_from is request:
        defaults['gain_limits'] = request['gain_limits']
        defaults['digital_gain_limits'] = request['digital_gain_limits']
        defaults['digital_cycle_limits'] = request['digital_cycle_limits']

    #### signal limits
    if 'freq_limits' not in check_from:
        defaults['freq_limits'] = request['bands']
    if 'span_limits' not in check_from:
        defaults['span_limits'] = [[5e3,20e6]]
    if 'duration_limits' not in check_from:
        max_time = np.array([request['runtime']]).max()
        try:
            max_time = max_time.item()
        except:
            pass
        defaults['duration_limits'] = [[0.1,max_time]]
    else:
        ### make sure time limit is reasonable
        max_time = np.array([request['runtime']]).max()
        try:
            max_time = max_time.item()
        except:
            pass
        sanity = np.array([check_from['duration_limits']],dtype=np.float32)
        orig_shape = sanity.shape
        sanity = sanity.flatten()
        for idx in range(sanity.size):
            if sanity[idx] > max_time:
                sanity[idx] = max_time
        sanity = sanity.reshape(orig_shape)
        sanity = sanity[0]
        if isinstance(sanity,np.ndarray):
            sanity = sanity.tolist()
        check_from['duration_limits'] = sanity
    if check_from is request:
        defaults['freq_limits'] = request['freq_limits'] if request['freq_limits'] is not None else request['bands']
        defaults['span_limits'] = request['span_limits']
        defaults['duration_limits'] = request['duration_limits']

    #### energy limits
    if 'bw_limits' not in check_from:
        defaults['bw_limits'] = [[5e3,50e3],[1e6,2e6]]
    if 'burst_dwell_limits' not in check_from:
        defaults['burst_dwell_limits'] = [[0.05,1],[10,20]]
    if 'burst_idle_limits' not in check_from:
        defaults['burst_idle_limits'] = [[0.5,1.0],[1.0,2.0]]
    if check_from is request:
        defaults['bw_limits'] = request['bw_limits']
        defaults['burst_dwell_limits'] = request['burst_dwell_limits']
        defaults['burst_idle_limits'] = request['burst_idle_limits']

    request['profile_defaults'] = defaults

    for pp in possible_checks:
        print(pp) ## debugging
        # if pp not in results:
        #     continue

    proc = mp.Process(target=random_run,args=(
                radio_args,
                request,
                server_state.burst_instance,
                os.path.join(server_state.save_dir,server_state.json_proto)))
    proc.start()
    server_state.increment_instance(request['instance_limit'])

    return proc


random_setup_keys = ['bands', 'runtime', 'seed', 'instance_limit', 'profiles', 'radios', 'profile_defaults']
profile_defaults_keys = _source_limits + _signal_limits + _energy_limits
def random_run(radios, run_setup, starting_instance=0, truth_template=''):
    ### Since still getting more useful executables up and running this is going to be more complex than
    ###  it really needs to be long term

    ## Allow each radio to be independently mananged for now (if ever getting to multi-agent radios.. revist)
    max_bursts = run_setup['instance_limit']
    seed = None if 'seed' not in run_setup else run_setup['seed']
    profiles = run_setup['profiles']
    runtime = run_setup['runtime']
    bands = run_setup['bands']

    manager = mp.Manager()
    worker_lookups = manager.dict()
    worker_lookups['instance'] = starting_instance
    worker_lookups['instance_limit'] = starting_instance+max_bursts
    worker_lookups['barrier'] = manager.Barrier(len(radios))
    worker_lookups['semaphore'] = manager.Semaphore(1)
    worker_lookups['start_time'] = None ## First worker to claim semaphore sets this
    worker_lookups['end_time'] = None  ## First worker to claim semaphore sets this
    worker_lookups['json_template'] = truth_template
    worker_lookups['default_params'] = run_setup['profile_defaults']
    worker_lookups['profile_specifics'] = dict([
        (x,run_setup[x]) for x in \
        (profiles if profiles is not None else get_all_profile_names()) \
        if x in run_setup
    ])
    worker_lookups['startup_patience_limit'] = 20.0 #seconds

    rng = np.random.default_rng(seed)
    workers = [None]*len(radios)

    early_terminate = 0
    def sig_handler(sig,frame):
        nonlocal early_terminate,workers
        print("What ?? : ",signal.strsignal(sig))
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
        nonlocal early_terminate,starting_instance,max_bursts,worker_lookups
        return (
            early_terminate == 0,
            worker_lookups['instance'] < starting_instance+max_bursts,
            worker_lookups['end_time'] is None or \
                (worker_lookups['end_time'] is not None and \
                    datetime.now(timezone.utc).timestamp() < worker_lookups['end_time'])
        )
    while all(reasons_to_loop()):
        if any([x is None for x in workers]):
            ### room to start another radio
            for idx in range(len(radios)):
                if workers[idx] is not None:
                    continue
                worker_lookups['seed_{0:d}'.format(worker_n)] = [
                    rng.integers(0,np.iinfo(np.uint64).max,
                        dtype=np.uint64).item() for _ in range(10)]

                disable_sig_handler() ## don't carry the interrupt handler over
                workers[idx] = mp.Process(target=random_radio_run_worker,
                    args=(worker_n,radios[idx],profiles,runtime,bands,worker_lookups))
                workers[idx].start()
                enable_sig_handler()
                worker_n += 1
        if any([not x.is_alive() for x in workers]):
            #### some signal has finished (should it??), cleanup to start a new one
            for idx in range(len(radios)):
                if workers[idx].is_alive():
                    continue
                workers[idx].join()
                workers[idx].close()
                workers[idx] = None
    print(worker_n,'hmmm',reasons_to_loop())
    for idx in range(len(radios)):
        if workers[idx] is not None and workers[idx].is_alive():
            workers[idx].terminate()
    for idx in range(len(radios)):
        if workers[idx] is not None:
            workers[idx].join()
            workers[idx].close()
            workers[idx] = None


def filter_choice(key, value_choices, limiter):
    level_filter = 0
    print("DEBUG filter::::",key,value_choices,limiter,end=' :: ')
    if isinstance(value_choices,(list,tuple)):
        if isinstance(value_choices[0],(list,tuple)):
            new_values = [[y for y in x] for x in value_choices]
            level_filter = 2
        else:
            new_values = [x for x in value_choices]
            level_filter = 1
    else:
        new_values = value_choices
    def l2_filter(choices,limits):
        if isinstance(limits,(list,tuple)) and isinstance(limits[0],(list,tuple)):
            for x,choice_range in enumerate(choices):
                ### should be [low,high]
                if any([choice_range[0] >= limit[0] and choice_range[1] <= limit[1] for limit in limits]):
                    #### this full choice range is in the limits
                    continue
                else:
                    #### being lazy just force it to take on the first limit for now
                    choices[x] = limits[0]
        elif isinstance(limits,(list,tuple)):
            for x,choice_range in enumerate(choices):
                if (choice_range[0] >= limits[0] and choice_range[1] <= limits[1]):
                    #### this full choice range is in the limits
                    continue
                else:
                    #### being lazy, just force it to be the limits
                    choices[x] = limits
        else:
            # Limit is scalar?
            choices = limits
        print(choices)
        return choices
    def l1_filter(choices,limits):
        if isinstance(limits,(list,tuple)) and isinstance(limits[0],(list,tuple)):
            if any([choices[0] >= limit[0] and choices[1] <= limit[0] for limit in limits]):
                #### this full choice range is in the limits
                pass
            else:
                #### being lazy
                choices = limits[0]
        elif isinstance(limits,(list,tuple)):
            if choices[0] >= limits[0] and choices[1] <= limits[1]:
                #### this full choice range is in the limits
                pass
            else:
                #being lazy
                choices = [x for x in limits]
        else:
            choices = limits
        return choices
    def l0_filter(choices,limits):
        if isinstance(limits,(list,tuple)) and isinstance(limits[0],(list,tuple)):
            if any([choices >= limit[0] and choices <= limit[1] for limit in limits]):
                pass # good to go
            else:
                ### lazy
                choices = limits[0]
        elif isinstance(limits,(list,tuple)):
            if choices >= limits[0] and choices <= limits[1]:
                pass # good to go
            else:
                choices = limits
        else:
            choices = limits
        return choices
    if key == 'freq_limits':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # freq_limits is set to scalar
    elif key == 'bw_limits':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # bw_limits is set to scalar
    elif key == 'duration_limits':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # duration_limits is set to scalar
    elif key == 'bw_limits':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # duration_limits is set to scalar
    elif key == 'burst_dwell_limits':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # burst_dwell_limits is set to scalar
    elif key == 'burst_idle_limits':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # burst_idle_limits is set to scalar
    elif key == 'frequency':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # frequency is set to scalar
    elif key == 'rate':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # rate is set to scalar
    elif key == 'gain':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # gain is set to scalar
    elif key == 'gain_range':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # gain_range is set to scalar
    elif key == 'gain_cycle':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # gain_cycle is set to scalar
    elif key == 'bw':
        if level_filter == 2:
            new_values = l2_filter(new_values,limiter)
        elif level_filter == 1:
            new_values = l1_filter(new_values,limiter)
        else:
            pass # bw is set to scalar
    else:
        try:
            ### maybe this will work
            if level_filter == 2:
                new_values = l2_filter(new_values,limiter)
            elif level_filter == 1:
                new_values = l1_filter(new_values,limiter)
            else:
                pass # value is set to scalar
        except:
            raise RuntimeError("Don't know how to filter the {} key at the moment".format(key))
    return new_values

def narrow_choice(key, rng, value_choices):
    debug_out = "DEBUG narrow:::: {} {} :: ".format(key,value_choices)
    if isinstance(value_choices, (tuple,list)) and isinstance(value_choices[0], (tuple,list)):
        if len(value_choices) == 1:
            return value_choices[0]
        return rng.choice(value_choices).item()
    elif isinstance(value_choices, (tuple,list)) and isinstance(value_choices[0], int):
        if value_choices[0] == value_choices[1]:
            return value_choices[0]
        return rng.integers(value_choices[0],value_choices[1]).item()
    elif isinstance(value_choices, (tuple,list)) and isinstance(value_choices[0], float):
        return rng.uniform(value_choices[0],value_choices[1])
    print(debug_out,value_choices)
    return value_choices ## guessing this is scalar at this point

def random_radio_run_worker(worker_id,uhd_args,profiles,runtime,run_bands,shared_dict):
    # shared_dict['semaphore'].acquire()
    # if shared_dict['start_time'] is None:
    #     now = datetime.now(timezone.utc)
    #     start_time = now.timestamp()
    #     end_time = start_time + runtime
    #     shared_dict['start_time'] = start_time
    #     shared_dict['end_time'] = end_time
    #     shared_dict['semaphore'].release()
    # else:
    #     start_time = shared_dict['start_time']
    #     end_time = shared_dict['end_time']
    #     shared_dict['semaphore'].release()

    #### Effectively running server's start radio now
    seed = shared_dict['seed_{0:d}'.format(worker_id)]
    print(worker_id,-1,seed,uhd_args)
    rng = np.random.default_rng(seed)
    tail = uhd_args[uhd_args.find('serial')+7:]
    if(tail.find(',') > 0):
        dev_serial = tail[:tail.find(',')]
    else:
        dev_serial = tail

    if profiles is None:
        profiles = get_all_profile_names()
    run_defaults = shared_dict['default_params'].copy()

    source_limits = dict([(x,y) for x,y in run_defaults.items() if x in _source_limits])
    source_limits['patience'] = shared_dict['startup_patience_limit']
    json_template = shared_dict['json_template']
    profile_specifics = shared_dict['profile_specifics']
    patience = wise_choice('patience',source_limits['patience'],rng)


    early_terminate = 0
    early_exit_occured = False
    def sig_handler(sig,frame):
        nonlocal early_terminate,worker_id,early_exit_occured
        print("I've been called (worker_id:{})".format(worker_id))
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
        nonlocal early_terminate,shared_dict
        return (
            early_terminate == 0,
            (datetime.now(timezone.utc).timestamp() < shared_dict['end_time'] \
                if shared_dict['end_time'] is not None else True)
        )
    while all(reasons_to_loop()):
        ### Ok, radio selected, so just start sending signals
        print(worker_id,'A',runtime,shared_dict['start_time'],shared_dict['end_time'])
        shared_dict['semaphore'].acquire()
        if shared_dict['instance'] >= shared_dict['instance_limit']:
            shared_dict['semaphore'].release()
            break
        instance = shared_dict['instance']
        shared_dict['instance'] = instance + 1
        shared_dict['semaphore'].release()
        #### chose a signal
        profile_name = wise_choice('profiles',profiles,rng)
        profile = extract_profile_by_name(profile_name)
        this_run = run_defaults.copy()
        specifics = profile_specifics[profile_name] if profile_name in profile_specifics else dict()
        preference = profile.defaults.copy() ### Get the defaults of the profile
        print(worker_id,'B')

        run_params = preference

        ow_energy = True if 'disable_energy_overwrite' not in specifics else not specifics['disable_energy_overwrite']
        ow_signal = True if 'disable_signal_overwrite' not in specifics else not specifics['disable_signal_overwrite']
        ow_source = True if 'disable_source_overwrite' not in specifics else not specifics['disable_source_overwrite']
        extra_key_map = list() if 'extra_key_map' not in specifics else specifics['extra_key_map']

        # if not ow_source:
            # print("Not overwriting source") ### not really needed initially
        ##### Assuming source needs to always be overwritten for now
        src_default_override = {##being lazy assuming b2XX for now
            "gain_limits" : [30,60],
            "digital_gain_limits" : 0.0,
            "digital_cycle_limits" : 2.0,
        }
        for src_key in _source_limits:
            if src_key in specifics:
                run_params[src_key] = specifics[src_key]
            elif src_key in this_run:
                run_params[src_key] = this_run[src_key]
            elif src_key not in run_params:
                run_params[src_key] = src_default_override[src_key]

        if ow_signal:
            for sig_key in _signal_limits:
                if sig_key in specifics:
                    run_params[sig_key] = specifics[sig_key]
                elif sig_key in this_run:
                    run_params[sig_key] = this_run[sig_key]

        if ow_energy:
            for eng_key in _energy_limits:
                if eng_key in specifics:
                    run_params[eng_key] = specifics[eng_key]
                elif sig_key in this_run:
                    run_params[eng_key] = this_run[eng_key]


        print(worker_id,'C')
        json_truth = json_template.format(serial=dev_serial,instance=instance)
        
        #### Time to make some choices
        mapping_keys = [
            ('gain','gain_limits'),
            ('gain_range','digital_gain_limits'),
            ('gain_cycle','digital_cycle_limits'),
            ('frequency','freq_limits'),
            ('span','span_limits'),
            ('duration','duration_limits'),
            ('dwell_range','burst_dwell_limits'),   ### NeedRange modify
            ('idle_range','burst_idle_limits')      ### NeedRange modify
        ] + extra_key_map
        ########## (['rate','bw'],['bw_limits',uhd_args]),#### this will likely be tweaked
        ### need a unique way to set BW and SampleRate


        # for signal_key in signal_level_musts:
        #     if signal_key not in run_params:
        #         continue ## doubt this will happen, but just in case
        #     value = run_params[signal_key]
        #     if signal_key == 'freq_limits':
        #         value = filter_choice(signal_key,value,run_bands)
        #     if signal_key == 'duration_limits':
        #         value = filter_choice(signal_key,value,(0,shared_dict['end_time']-datetime.now(timezone.utc).timestamp()))
        #     if isinstance(value,(tuple,list)):
        #         value = [x for x in value]
        #         if signal_key in ['frequency','rate','bw']:
        #             if signal_key == 'frequency':
        #                 value = filter_choice(signal_key,value,run_params['freq_limits'])
        #             if signal_key == 'rate':
        #                 value = filter_choice(signal_key,value,run_params['bw_limits'])
        #             if signal_key == 'bw':
        #                 value = filter_choice(signal_key,value,run_params['bw_limits'])
        #         value = narrow_choice(signal_key,rng,value)
        #     #### this should be a valid value for the key now>>>>>>> hopefully
        #     run_params[signal_key] = value
        for mk_param,mk_lookup in mapping_keys:
            if mk_param in ['rate','bw']:
                ## oh boy
                print("sorry----ignoring this request for now")
                pass
            try:
                run_params[mk_param] = wise_choice(mk_lookup,run_params[mk_lookup],rng)
            except Exception as e:
                print("Can't seem to handle the ({},{}) mapping key right now".format(mk_param,mk_lookup))
                try:
                    print("    Can lookup the value as ({})".format(run_params[mk_lookup]))
                except:
                    pass
                raise e
        run_params['json'] = json_truth


        ##### handle BW and SampleRate here now
        ### being lazy for now assuming b2XX devices
        bandwidth = wise_choice('bandwidth',run_params['bw_limits'],rng)
        rate = min(2*bandwidth,25e6) ## max bw for b210 with fc32 datatype
        run_params['rate'] = max(rate,220e3) ## min bw for b210 with fc32 datatype
        run_params['bw'] = 0.9 if bandwidth > 25e6 else bandwidth/run_params['rate']
        #######################################

        print(worker_id,'D',json_truth)

        sig_dur = run_params['duration']
        launch_env = os.environ.copy()
        launch_command = profile.start("-a {0:s}".format(uhd_args),run_params)
        print(worker_id,'==',launch_command)
        disable_sig_handler()
        command = shlex.split(launch_command)
        proc = subprocess.Popen(command,shell=False,env=launch_env)
        enable_sig_handler()
        crit_err_check = datetime.now(timezone.utc).timestamp()
        if crit_err_check != True:
            while(not os.path.isfile(json_truth)):
                # waiting for the usrp to spin up im the exec/proc
                if datetime.now(timezone.utc).timestamp() - crit_err_check > patience:
                    crit_err_check = True
                    break
        print(worker_id,'E',sig_dur,crit_err_check)
        if crit_err_check == True:
            ### Uh oh, patience has ended
            if proc.poll() is None:
                proc.send_signal(signal.SIGINT)
        else:
            sig_start_at = datetime.now(timezone.utc).timestamp()
            ######################
            if not start_time_extracted:
                ### every worker will need to extract the start time and end time
                shared_dict['semaphore'].acquire()
                if shared_dict['start_time'] is None:
                    shared_dict['start_time'] = start_time = sig_start_at
                    shared_dict['end_time'] = end_time = sig_start_at + runtime
                else:
                    start_time = shared_dict['start_time']
                    end_time = shared_dict['end_time']
                shared_dict['semaphore'].release()
                start_time_extracted = True
            ######################
            early_exit_occured = False
            def additional_keep_looping():
                return (reasons_to_loop() \
                    + (datetime.now(timezone.utc).timestamp() < sig_start_at+sig_dur,))
            while all(additional_keep_looping()):
                #### let the signal run until signal duration is up
                if proc.poll() is not None:
                    #### this process has ended on it's own??
                    early_exit_occured = True
                    break
            print(worker_id,'F',additional_keep_looping(),early_exit_occured)
            if proc.poll() is None:
                ### time to wrap up now signal
                proc.send_signal(signal.SIGINT)
        print(worker_id,'G')
        proc.wait() ### wait for it to write out the truth
        print(worker_id,'H')

    print(worker_id,"end",reasons_to_loop())###why did this proc end?
