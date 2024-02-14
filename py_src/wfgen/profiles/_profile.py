
import json
import glob
import os
import numpy as np
from ..utils import have_pygr

''' Notes for profile usage

    For the most part, the profile is flexible in many ways that allow user more control.
    
    Two key concepts are continuous and bursty types of waveforms.
        - continuous : maintains carrier frequency and is always transmitting
                        (assuming it was on before it's actually online,
                        and still on after it's been stopped)
        - bursty : The only real difference is that this signal has observable
                   starts and stops.

    Continuous signals presume that the best way to handle transmission
        is that the information generated is 'stream' based
    Bursty signals presume that the best way to handle transmission
        is that the information generated is 'async' based

    So, continuous signals have the concept of:
        - carrier frequency
        - bandwidth (bw)
        - duration
    while, bursty signals also include the concepts of:
        - burst bandwidth
        - burst dwell
        - burst idle
        - span bandwidth
    Though, burst bandwidth and span bandwidth are the same
        when talking about a continuous signal
    But going from continuous signal bandwidth to a bursty setup
        is more better matched as the burst bandwidth

    So:
        - frequency
        - span
        - duration
        - bw
        - dwell
        - idle
    are all parameters that can operate within a range, so limits
        for random generation are needed (see below)

    Typical options that are being repeatedly used:
        Generator Params:
            - frequency : carrier frequency of the waveform
            - rate : the sample rate of the generated waveform
            - bw : float value in range [0,1] that is relative to the {rate}
            - modulation : the unqiue modulation to use
            - gain : the gain value used with the radio (B200: [0-72])

        Limit Params: [(low,high)|const,...]
            - freq_limits : the valid frequency ranges
                the waveform can be centered at
            - span_limits : the bandwidth over which the waveform can occur
                and still be identified as the same signal
                Ex: Bluetooth can be found between 2400MHz - 2482MHz
                    therefore the span is 82MHz (though for edge limits
                    we would say the span is 78MHz)
            - duration_limits : How long will the signal be transmitting
            - bw_limits : bw of the burst / continous signal
            - burst_dwell_limits : How long can the signal be on for
            - burst_idle_limits : How long can the signal be off for
'''

def get_base_options(style='static',sub_type=None):
    #### manual control over 'json'
    _static_options = ['band',"BW", "bw", "duration", "frequency", "gain", "rate"]
    if sub_type == 'analog':
        _static_options += ["mod_index","src_fq"]
    if sub_type == 'fsk':
        _static_options += ["phase_shape", "mod_index", "symbol_rate"]
    if sub_type in ['tone','tones']:
        _static_options += ['delta','magnitude','num_tones']
    _bursty_options = _static_options + ['dwell','squelch','period']
    _hopper_options = _bursty_options + ['num_bursts','span','num_channels','linear_hop',
                                         'loop_delay','num_loops']
    _ofdm_options = _hopper_options + ['nfft','used_carriers','cplen','min_syms','max_syms',
                                       'car_mod']
    if style == 'ofdm':
        return _ofdm_options
    elif style == 'bursty':
        return _bursty_options
    elif style == 'hopper':
        return _hopper_options
    else:
        return _static_options
def get_base_flags(style='static',sub_type=None):
    #### manual control over '-j' for 'json'
    _static_options = [None,'-B', '-b', '-d', '-f', '-g', '-r']
    if sub_type == 'analog':
        _static_options += ['-x','-F']
    if sub_type == 'fsk':
        _static_options += ['-P','-x','-R']
    if sub_type == 'tone':
        _static_options += [None,None,None]
    if sub_type == 'tones':
        _static_options += ['-E','-A','-t']
    _bursty_options = _static_options + ['-w','-q','-p']
    _hopper_options = _bursty_options + ['-H','-s','-k','-S',
                                         '-L','-l']
    _ofdm_options = _hopper_options + ['-n','-U','-c','-u','-v',None]
    if style == 'ofdm':
        return _ofdm_options
    elif style == 'bursty':
        return _bursty_options
    elif style == 'hopper':
        return _hopper_options
    else:
        return _static_options

def resolve_band_freq_options(runtime_params, stored_params):
    # print(runtime_params)
    # print(stored_params)
    from ..run_modes.randomize import wise_choice
    if 'frequency' in runtime_params:
        ## TX at this frequency then, ignore bands
        if 'band' in runtime_params:
            del runtime_params['band']
        if 'bands' in runtime_params:
            del runtime_params['bands']
        stored_params['band'] = None
        if 'bands' in stored_params:
            del stored_params['bands']
    elif 'band' in runtime_params:
        if isinstance(runtime_params['band'],(list,tuple)):
            runtime_params['bands'] = runtime_params['band']
            del runtime_params['band']
        else:
            if ',' not in runtime_params['band']:
                raise RuntimeError("'band' expects values in the form 'low,high' and no comma found")
            ## The band is set, don't try to choose from bands
            runtime_params['frequency'] = wise_choice('frequency',[int(float(x)) for x in runtime_params['band'].split(',')])
        runtime_params,stored_params = resolve_band_freq_options(runtime_params,stored_params)
    elif 'bands' in runtime_params:
        if not isinstance(runtime_params['bands'],(list,tuple)):
            runtime_params['band'] = runtime_params['bands']
            del runtime_params['bands']
        else:
            result = wise_choice('band',runtime_params['bands'])
            if isinstance(result,str):
                runtime_params['band'] = result
            elif isinstance(result,(int,float)):
                runtime_params['frequency'] = result
            else:
                raise RuntimeError("Logic error handling the band/freq keyword")
        runtime_params,stored_params = resolve_band_freq_options(runtime_params,stored_params)
    return runtime_params, stored_params

def resolve_hopper_dwell_squelch_period_options(runtime_params,stored_params):
    # print(runtime_params)
    # print(stored_params)
    from ..run_modes.randomize import wise_choice
    if 'period' in runtime_params:
        if 'dwell' in runtime_params and 'squelch' in runtime_params:
            del runtime_params['squelch']
            stored_params['squelch'] = None
    elif 'dwell' in runtime_params and 'squelch' in runtime_params:
        runtime_params['dwell'] = wise_choice('dwell',runtime_params['dwell'])
        runtime_params['squelch'] = wise_choice('squelch',runtime_params['squelch'])
        stored_params['period'] = None
    elif 'dwell' in runtime_params and 'squelch' not in runtime_params:
        runtime_params['dwell'] = wise_choice('dwell',runtime_params['dwell'])
        stored_params['squelch'] = None
        stored_params['period'] = None
    elif 'dwell' not in runtime_params and 'squelch' in runtime_params:
        runtime_params['squelch'] = wise_choice('squelch',runtime_params['squelch'])
        stored_params['dwell'] = None
        stored_params['period'] = None
    else:
        # period, dwell, and squelch are all not in the runtime params
        if 'period' in stored_params:
            if 'dwell' and 'squelch' in stored_params:
                stored_params['squelch'] = None
        elif 'dwell' in stored_params and 'squelch' in stored_params:
            stored_params['dwell'] = wise_choice('dwell',stored_params['dwell'])
            stored_params['squelch'] = wise_choice('squelch',stored_params['squelch'])
            stored_params['period'] = None
        elif 'dwell' in stored_params and 'squelch' not in stored_params:
            stored_params['dwell'] = wise_choice('dwell',stored_params['dwell'])
            stored_params['period'] = None
            stored_params['squelch'] = None
        elif 'dwell' not in stored_params and 'squelch' in stored_params:
            stored_params['squelch'] = wise_choice('squelch',stored_params['squelch'])
            stored_params['dwell'] = None
            stored_params['period'] = None

    return runtime_params, stored_params

class _profile(object):
    ''' Abstract class for a profile from which all profiles should be derived

    More Details TBD.
    '''
    def __init__(self,name,**kwargs):
        self._name = name
        self.proc = None
        self.stats = None
        self._use_log = True if 'use_log' in kwargs and kwargs['use_log'] else False
        from wfgen import logger_client,fake_log
        self.log_c = logger_client(name) if self._use_log else fake_log(name,cout=False)
        
        if "silent" not in kwargs or not kwargs["silent"]:
            try:
                print("Making a {0:s}_{1:04d} profile".format('_'.join(name.split('_')[:-1]),int(name.split('_')[-1])),end='-->')
            except (NameError,ValueError):
                print("Making a {0:s} profile".format(name))
    def start(self,radio_args,params):
        '''Return the command to start this profile

        Assumes something higher up is taking care of radio specifics
        '''
        print("'start' function has not been overridden properly in this({0}) profile".format(self._name))
        raise RuntimeError("Abstract Method not defined")
    def get_name(self):
        '''What should this profile be referred to.

        This can be overwritten for things like psk2 and bpsk such that
        they are the same, regardless of which profile is called if both
        exist.
        '''
        return self._name
    def get_stats(self):
        ''' Get signal level details needed for truth recording

        This function should return a list of dictionaries that include
        what the 'signal' is as well as the relavent 'energy' reports

        If None is returned, that means the profile hasn't been used/developed
        '''
        if self.stats is None:
            print("'get_stats' function found stats as None -- is this overridden in the profile?")
        return self.stats
    def get_options(self):
        '''Return a list of options that can be used with this profile

        Allows for further customization from the cli to tweak things as needed,
        hopefully this will prevent N versions of bpsk for example.
        '''
        return []


class _lookup_profile(object):
    def __init__(self,lookup_key='profiles'):
        self.lookup_dir = os.path.join(os.path.abspath(os.path.dirname(__file__)),'lookups')
        self.possible_profile_files = glob.glob(os.path.join(self.lookup_dir,'*.json'))
        self.profile_keys = [os.path.splitext(os.path.basename(x))[0] for x in self.possible_profile_files]
        self.raw_dicts = dict()
        for fpath,fkey in zip(self.possible_profile_files,self.profile_keys):
            with open(fpath,'r') as fp:
                self.raw_dicts[fkey] = json.load(fp)
            if lookup_key not in self.raw_dicts[fkey]:
                del self.raw_dicts[fkey]
        self.unique_profile_names = sorted([s["name"] for k,d in self.raw_dicts.items() for s in d[lookup_key]])
        self.profiles = dict([(n,None) for n in self.unique_profile_names])
        for k,d in self.raw_dicts.items():
            for s in d[lookup_key]:
                active_profile = s['name']
                p = s.copy()
                p['profile_source_file'] = k
                self.profiles[active_profile] = p

lookup_profiles = _lookup_profile()

def get_traceback_profiles(script=None):
    if script is not None:
        base_file = os.path.basename(script)
    traceback_profiles = dict()
    for prof,info in lookup_profiles.profiles.items():
        if script is None:
            origins = [z for y in [x['signals'] for x in info['traceback']] for z in y]
        else:
            origins = [z for y in [x['signals'] for x in info['traceback'] if x['file'] == base_file] for z in y]
        for k in origins:
            if k in traceback_profiles:
                if isinstance(traceback_profiles[k],str):
                    traceback_profiles[k] = [traceback_profiles[k],prof]
                else:
                    traceback_profiles[k].append(prof)
            else:
                traceback_profiles[k] = prof
    for k in traceback_profiles.keys():
        if isinstance(traceback_profiles[k],list):
            new_v = np.unique(traceback_profiles[k]).tolist()
            if len(new_v) == 1:
                new_v = new_v[0]
            traceback_profiles[k] = new_v
    return traceback_profiles


def _get_other_profiles():
    from .fskmod import available_mods as fsk_profiles
    from .linmod import available_mods as lin_profiles
    from .afmod import available_mods as af_profiles
    from .tones import available_mods as tone_profiles
    from argparse import Namespace
    return Namespace(**{
        'lin_profiles':lin_profiles,
        'fsk_profiles':fsk_profiles,
        'af_profiles':af_profiles,
        'tone_profiles':tone_profiles
    })

def get_all_profile_names():
    replay_profiles = get_replay_profile_names()
    ns = _get_other_profiles()
    all_profiles = (replay_profiles 
        + [x for x in ns.lin_profiles]
        + [x for x in ns.fsk_profiles]
        + [x for x in ns.af_profiles]
        + [x for x in ns.tone_profiles])
    return all_profiles

def get_replay_profile_names():
    return [x for x in lookup_profiles.unique_profile_names]

def get_defined_profile_names():
    ns = _get_other_profiles()
    all_profiles = ([] 
        + [x for x in ns.fsk_profiles]
        + [x for x in ns.lin_profiles]
        + [x for x in ns.af_profiles]
        + [x for x in ns.tone_profiles])
    return all_profiles

class replay_profile(_profile):
    def __init__(self,name,signal_space):
        super().__init__(name,use_log=True)
        self.mod_file = None
        self.mod_p = None
        # print(signal_space)

        self.likely_mod = '_'.join(name.split('_')[:-1])
        if self.likely_mod == 'dbpsk':
            self.likely_mod = 'dpsk2'
        if self.likely_mod == 'dqpsk':
            self.likely_mod = 'dpsk4'
        if self.likely_mod == 'gfsk':
            self.likely_mod = 'cpfsk2'
        if self.likely_mod == 'pi_dqpsk4':
            self.likely_mod = 'pi4dqpsk'
        if self.likely_mod == 'fm_analog':
            self.likely_mod = 'fm_wav_file'
        if self.likely_mod == 'am_analog':
            self.likely_mod = 'am_wav_file'

        ns = _get_other_profiles()

        if self.likely_mod in ns.lin_profiles:
            from . import linmod
            self.mod_file = linmod
            self.mod_p = getattr(self.mod_file,self.likely_mod)()

        if self.likely_mod in ns.fsk_profiles:
            from . import fskmod
            self.mod_file = fskmod
            self.mod_p = getattr(self.mod_file,self.likely_mod)()

        if self.likely_mod in ns.af_profiles:
            from . import afmod
            self.mod_file = afmod
            self.mod_p = getattr(self.mod_file,self.likely_mod)()

        if self.likely_mod in ns.tone_profiles:
            from . import tones
            self.mod_file = tones
            self.mod_p = getattr(self.mod_file,self.likely_mod)()

        from wfgen import c_logger
        if self.mod_p is not None:
            pass
            ### updates
        else:
            from . import linmod
            print('???? This is just going to be qpsk')
            self.log_c.log(c_logger.level_t.WARNING,"{}---substitution--->qpsk".format(name))
            self.mod_p = getattr(linmod,'qpsk')()

    @property
    def base_command(self):
        if self.mod_p is None:
            raise RuntimeError("No base command found for '{0}'".format(self.get_name()))
        return self.mod_p.base_command

    def start(self,*args):
        if self.mod_p is None:
            super().start(*args)
        return self.mod_p.start(*args)

    def get_stats(self,*args):
        if self.mod_p is None:
            return super().get_stats(*args)
        return self.mod_p.get_stats(*args)

    def get_options(self,*args):
        if self.mod_p is None:
            return super().get_options(*args)
        return self.mod_p.get_options(*args)


def extract_profile_by_name(profile_name):
    if profile_name not in get_all_profile_names():
        raise ValueError("Profile({}) is not found presently".format(profile_name))
    if profile_name in get_defined_profile_names():
        ns = _get_other_profiles()
        if profile_name in ns.lin_profiles:
            from . import linmod
            return getattr(linmod,profile_name)()
        if profile_name in ns.fsk_profiles:
            from . import fskmod
            return getattr(fskmod,profile_name)()
        if profile_name in ns.af_profiles:
            from . import afmod
            return getattr(afmod,profile_name)()
        if profile_name in ns.tone_profiles:
            from . import tones
            return getattr(tones,profile_name)()
    else:
        return replay_profile(profile_name,lookup_profiles.profiles[profile_name])


def sanity_check_profiles():
    import numpy as np
    pns = get_all_profile_names()
    upns = np.unique(pns)
    assert len(pns) == len(upns)


if __name__ == "__main__":
    lookup_profile = _lookup_profile()
