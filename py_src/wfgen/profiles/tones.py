
import numpy as np
from ._profile import _profile
import multiprocessing as mp

available_mods = ['tone','multitone','emanation_exponential']

class tone(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(tone,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_tone'
        self.mod = 'tone'
        self.options = profiles.get_base_options('static','tone') + ['json']
        self.option_flags = profiles.get_base_flags('static','tone') + ['-j']
        self.defaults = { ## This are set to the defaults in 'wfgen_linmod'
            'bands':[(100e6,6e9)],
            # 'freq_limits':[(100e6,6e9)], ### controlled with band now
            'span_limits':[(5e6,100e6)],
            'dwell_limits':[(0.02,float('inf'))],####### These two should match here
            'squelch_limits':[(0.1,2.0)],
            'bw_limits':[(5e3,5e6)],
            'band': (2.4e9,2.5e9),
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'dwell': -1,
            'absence': -1,
            'period': -1,
            'squelch': -1,
            'modulation': 'tone',   # Modulation being transmitted
        }
        self._config = None
    def config(self,**kwargs):
        self._config = kwargs
    def start(self,radio_args,params=dict()):
        from wfgen import profiles
        opts = self.options
        flgs = self.option_flags
        hopping = False
        if "hopper" in params and params['hopper']:
            ############# This is a frequency hopping version
            hopping = True
            self.base_command = 'wfgen_fhssgen'
            self.defaults['rate'] = 1e6
            self.defaults['bw'] = 0.025
            self.defaults['dwell'] = 0.05
            self.defaults['num_bursts'] = 10
            self.defaults['span'] = 0.9
            self.defaults['num_channels'] = -1
            self.defaults['loop_delay'] = 0.5
            self.defaults['num_loops'] = 5
            self.defaults['linear_hop'] = None
            opts = profiles.get_base_options('hopper') + ['json']
            flgs = profiles.get_base_flags('hopper') + ['-j']
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        self.stats = self.defaults.copy()
        if self._config is not None:
            self.stats.update(self._config)

        params, self.stats = profiles.resolve_band_freq_options(params,self.stats)
        skip_keys = []
        if hopping:
            params, self.stats = profiles.resolve_hopper_dwell_squelch_period_options(params,self.stats)
            if 'duration' in params and 'num_loops' in params:
                if params['duration'] is not None and params['num_loops'] is not None:
                    del params['num_loops']
            if 'duration' in params and params['duration'] is not None:
                skip_keys.append('num_loops')

        if 'bw' in params:
            if 'BW' in params:
                del params['BW']
            if 'BW' in self.stats:
                del self.stats['BW']
            skip_keys.append('BW')
        elif 'BW' in params:
            skip_keys.append('bw')
        elif 'bw' not in params and 'BW' not in params:
            skip_keys.append('BW')
        if 'duration' not in params:
            skip_keys.append('duration')
        cmd = ' '.join([self.base_command,radio_args])
        for idx,opt in enumerate(opts):
            if opt in skip_keys:
                continue
            if opt in ["linear_hop"]:
                if opt in params:
                    if params[opt]:
                        cmd += ' {0:s}'.format(flgs[idx])
                else:
                    if self.stats[opt]:
                        cmd += ' {0:s}'.format(flgs[idx])
            else:
                if opt in ['delta','magnitude','num_tones']:
                    continue
                if opt in params:
                    if params[opt] is not None:
                        cmd += ' {0:s} {1}'.format(flgs[idx],params[opt])
                        self.stats[opt] = params[opt]
                else:
                    if opt in self.stats and self.stats[opt] is not None:
                            cmd += ' {0:s} {1}'.format(flgs[idx],self.stats[opt])
        # if hopping:
        cmd += ' -M {}'.format(mod)
        print("RUNNING WITH THIS:\n",cmd,sep='')
        self.stats['modulation'] = mod
        return cmd
    def get_options(self):
        return self.options

class multitone(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(multitone,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_multitone'
        self.mod = 'multitone'
        self.options = profiles.get_base_options('static','tones') + ['json']
        self.option_flags = profiles.get_base_flags('static','tones') + ['-j']
        self.defaults = { ## This are set to the defaults in 'wfgen_linmod'
            'bands':[(100e6,6e9)],
            # 'freq_limits':[(100e6,6e9)], ### controlled with 'bands' now
            'span_limits':[(5e6,100e6)],
            'duration_limits':[(0.02,float('inf'))],####### These two should match here
            'bw_limits':[(5e3,5e6)],
            'burst_dwell_limits':[(2.0,float('inf'))],####### These two should match here
            'burst_idle_limits':[(0.1,2.0)],
            'band': (2.4e9,2.5e9),  # Band of operation
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 65,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'dwell': -1,
            'absence': -1,
            'period': -1,
            'squelch': -1,
            'num_tones': 8,
            'deltas': [0.8*(idx/7-0.5) for idx in range(8)],
            'magnitudes': [1.0 for _ in range(8)],
            'modulation': 'multitone'# Modulation being transmitted
        }
        self._config = None
    def config(self,**kwargs):
        self._config = kwargs
    def start(self,radio_args,params=dict()):
        from wfgen import profiles
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        self.stats = self.defaults.copy()
        if self._config is not None:
            self.stats.update(self._config)
        limiter = int(params['num_tones']) if 'num_tones' in params else self.stats['num_tones']
        if 'delta' in params:
            if not isinstance(params['delta'],list) and limiter > 1:
                return "Invalid use of 'delta' : expected to use multiple 'delta' keywords"
            elif not isinstance(params['delta'],list) and limiter == 1:
                params['delta'] = [params['delta']]
            if max([abs(float(x)) for x in params['delta']]) > 0.5:
                return "Invalid use of 'delta' : max(abs(delta)) > 0.5"
        if 'magnitude' in params:
            if not isinstance(params['magnitude'],list):
                return "Invalid use of 'magnitude' : expected to use multiple 'magnitude' keywords"
        if 'num_tones' in params and 'delta' not in params:
            if limiter > 1:
                params['delta'] = [0.8*(idx/(limiter-1)-0.5) for idx in range(limiter)]
            else:
                params['delta'] = [0.0]
        if 'num_tones' in params and 'magnitude' not in params:
            params['magnitude'] = [1.0 for _ in range(limiter)]
        if all([x in params for x in ['delta','magnitude']]) and any([len(params[x] if x in params else self.stats[x])!=limiter for x in ['delta','magnitude']]):
            # raise RuntimeError("Tone length mismatch found")
            print("Tone length mismatch found")
            return None

        params, self.stats = profiles.resolve_band_freq_options(params,self.stats)

        cmd = ' '.join([self.base_command,radio_args])
        for idx,opt in enumerate(self.options):
            repeater = False
            if opt in ['delta','magnitude']:
                repeater = True
            if opt in params:
                if params[opt] is not None:
                    if repeater == True:
                        for opt_idx in range(len(params[opt])):
                            cmd += ' {0:s} {1}'.format(self.option_flags[idx],params[opt][opt_idx])
                        self.stats[''.join([opt,'s'])] = params[opt]
                    else:
                        cmd += ' {0:s} {1}'.format(self.option_flags[idx],params[opt])
                        self.stats[opt] = params[opt]
            else:
                if repeater == True:
                    key = ''.join([opt,'s'])
                    for opt_idx in range(len(self.stats[key])):
                        cmd += ' {0:s} {1}'.format(self.option_flags[idx],self.stats[key][opt_idx])
                else:
                    if self.stats[opt] is not None:
                        cmd += ' {0:s} {1}'.format(self.option_flags[idx],self.stats[opt])
        self.stats['modulation'] = mod
        # if hopping:
        cmd += ' -M {}'.format(mod)
        print("RUNNING WITH THIS:\n",cmd,sep='')
        return cmd
    def get_options(self):
        return self.options

class emanation_exponential(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(emanation_exponential,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_multitone'
        self.mod = 'emanation_exponential'
        self.options = profiles.get_base_options('static','tones') + ['json']
        self.option_flags = profiles.get_base_flags('static','tones') + ['-j']
        self.defaults = { ## This are set to the defaults in 'wfgen_linmod'
            'bands':[(100e6,6e9)],
            #'freq_limits':[(100e6,6e9)],
            'span_limits':[(5e6,100e6)],
            'duration_limits':[(0.02,float('inf'))],####### These two should match here
            'bw_limits':[(5e3,5e6)],
            'burst_dwell_limits':[(2.0,float('inf'))],####### These two should match here
            'burst_idle_limits':[(0.1,2.0)],
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'num_tones': 5,
            'delta': 0.01,
            'dwell': -1,
            'absence': -1,
            'period': -1,
            'squelch': -1,
            'magnitude': .5,
            'modulation': 'emanation_exponential'# Modulation being transmitted
        }
        self._config = None
    def config(self,**kwargs):
        self._config = kwargs
    def start(self,radio_args,params=dict()):
        from wfgen import profiles
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        self.stats = self.defaults.copy()
        if self._config is not None:
            self.stats.update(self._config)
        limiter = int(params['num_tones']) if 'num_tones' in params else self.stats['num_tones']
        if 'delta' not in params:
            base = [(x-(limiter-1)/2)*self.stats['delta']*(0.8/(limiter-1)) for x in range(limiter)]
            self.stats['deltas'] = base
        else:
            if isinstance(params['delta'],list):
                return "Invalid use of 'delta' : can only use once"
            if limiter * float(params['delta']) >= 1.0:
                return "Invalid use of 'delta' : delta*num_tones < 1"
            base = [(x-(limiter-1)/2)*float(params['delta'])*(0.8/(limiter-1)) for x in range(limiter)]
            self.stats['deltas'] = base
        if 'magnitude' not in params:
            base = list(range(limiter//2-(1-limiter%2),0-(1-limiter%2),-1)) + list(range(limiter//2+limiter%2))
            self.stats['magnitudes'] = [self.stats['magnitude']**x for x in base]
        else:
            if isinstance(params['magnitude'],list):
                return "Invalid use of 'magnitude' : can only use once"
            if float(params['magnitude']) > 1.0:
                return "Invalid use of 'magnitude' : magnitude <= 1.0"
            base = list(range(limiter//2-(1-limiter%2),0-(1-limiter%2),-1)) + list(range(limiter//2+limiter%2))
            self.stats['magnitudes'] = [float(params['magnitude'])**x for x in base]

        params, self.stats = profiles.resolve_band_freq_options(params,self.stats)

        cmd = ' '.join([self.base_command,radio_args])
        for idx,opt in enumerate(self.options):
            tweak = False
            if opt in ['delta','magnitude']:
                opt += 's'
                tweak = True
            if opt in params:
                if params[opt] is not None:
                    if tweak == True:
                        for opt_idx in range(len(params[opt])):
                            cmd += ' {0:s} {1}'.format(self.option_flags[idx],params[opt][opt_idx])
                        self.stats[opt] = params[opt]
                    else:
                        cmd += ' {0:s} {1}'.format(self.option_flags[idx],params[opt])
                        self.stats[opt] = params[opt]
            else:
                if tweak == True:
                    for opt_idx in range(len(self.stats[opt])):
                        cmd += ' {0:s} {1}'.format(self.option_flags[idx],self.stats[opt][opt_idx])
                else:
                    if self.stats[opt] is not None:
                        cmd += ' {0:s} {1}'.format(self.option_flags[idx],self.stats[opt])
        self.stats['modulation'] = mod
        # if hopping:
        cmd += ' -M {}'.format(mod)
        print("RUNNING WITH THIS:\n",cmd,sep='')
        return cmd
    def get_options(self):
        return self.options
