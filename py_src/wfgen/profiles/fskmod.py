
import numpy as np
from ._profile import _profile
import multiprocessing as mp


available_mods = ["fsk2", "fsk4", "fsk8", "fsk16", "fsk32", "fsk64",
        "fsk128", "fsk256", "cpfsk2", "cpfsk4", "cpfsk8", "cpfsk16",
        "cpfsk32", "cpfsk64", "cpfsk128", "cpfsk256", "msk", "scpfsk4",
        "scpfsk8", "scpfsk16", "scpfsk32", "scpfsk64", "scpfsk128", "scpfsk256", "gmsk",
        "gcpfsk4", "gcpfsk8", "gcpfsk16", "gcpfsk32", "gcpfsk64", "gcpfsk128", "gcpfsk256"]

# __all__ = available_mods

class _fskmod(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(_fskmod,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_fskmod'
        self.mod = mod if mod is not None else 'fsk4'
        self.options = profiles.get_base_options('static','fsk') + ['json']
        self.option_flags = profiles.get_base_flags('static','fsk') + ['-j']
        self.defaults = { ## This are set to the defaults in 'wfgen_fskmod'
            'bands':[(100e6,6e9)],
            ###'freq_limits':[(100e6,6e9)], controlled with 'bands' now
            'span_limits':[(5e6,100e6)],
            'dwell_limits':[(0.02,float('inf'))],
            'squelch_limits':[(0.001,2.0)],
            'bw_limits':[(20e3,5e6)],
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
            'modulation': 'fsk4',   # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        }
        self._config = None
    def config(self,**kwargs):
        self._config = kwargs ### being lazy here
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
            opts = profiles.get_base_options('hopper','fsk') + ['json']
            flgs = profiles.get_base_flags('hopper','fsk') + ['-j']
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        self.stats = self.defaults.copy()
        if self._config is not None:
            self.stats.update(self._config)
        if 'symbol_rate' in params and 'bw' in params:
            if hopping:
                print("Can only specify one of 'symbol_rate' or 'bw', dropping 'symbol_rate'")
                del params['symbol_rate']
            else:
                print("Can only specify one of 'symbol_rate' or 'bw', dropping 'bw'")
                del params['bw']
        options = [x for x in opts if x not in ["symbol_rate","phase_shape"]]
        flags = [x for x in flgs if x not in ['-R','-P']]
        if 'symbol_rate' in params:
            del self.stats['bw']
            bw_idx = options.index('bw')
            options[bw_idx] = opts[opts.index('symbol_rate')] #'bw' <- 'symbol_rate'
            flags[bw_idx] = flgs[flgs.index('-s')] # '-b' <- '-s'

        # print(1,params)
        params, self.stats = profiles.resolve_band_freq_options(params,self.stats)
        # print(2,params)
        skip_keys = ['band']
        if hopping:
            params, self.stats = profiles.resolve_hopper_dwell_squelch_period_options(params,self.stats)
            if 'duration' in params and 'num_loops' in params:
                if params['duration'] is not None and params['num_loops'] is not None:
                    del params['num_loops']
            if 'duration' in params and params['duration'] is not None:
                skip_keys.append('num_loops')
        # print(3,params)


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
        if 'phase_shape' not in params:
            skip_keys.append('phase_shape')

        cmd = ' '.join([self.base_command,radio_args])
        # for z in zip(options,flgs):
        #     print(z)
        for idx,opt in enumerate(options):
            if opt in skip_keys:
                continue
            # print(flgs[idx],opt)
            if opt in ["linear_hop"]:
                if opt in params:
                    if params[opt]:
                        print("A")
                        cmd += ' {0:s}'.format(flgs[idx])
                else:
                    if self.stats[opt]:
                        print("B")
                        cmd += ' {0:s}'.format(flgs[idx])
            else:
                if opt in params:
                    if params[opt] is not None:
                        print("C")
                        cmd += ' {0:s} {1}'.format(flags[idx],params[opt])
                        self.stats[opt] = params[opt]
                else:
                    if self.stats[opt] is not None:
                        print("D")
                        cmd += ' {0:s} {1}'.format(flags[idx],self.stats[opt])
        cmd += ' -M {}'.format(mod)
        print("RUNNING WITH THIS:\n",cmd,sep='')
        self.stats['modulation'] = mod
        return cmd
    @staticmethod
    def get_options(mode='static'):
        from wfgen import profiles
        if mode == 'static':
            return profiles.get_base_options('static','fsk')
        return profiles.get_base_options('hopper','fsk')

class fsk2(_fskmod):
    def __init__(self,**kwargs):
        super(fsk2,self).__init__('fsk2',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk2',   # Modulation being transmitted
            'bw': 0.427,            # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class fsk4(_fskmod):
    def __init__(self,**kwargs):
        super(fsk4,self).__init__('fsk4',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk4',   # Modulation being transmitted
            'bw': 0.735,            # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class fsk8(_fskmod):
    def __init__(self,**kwargs):
        super(fsk8,self).__init__('fsk8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk8',   # Modulation being transmitted
            'bw': 0.7675,           # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class fsk16(_fskmod):
    def __init__(self,**kwargs):
        super(fsk16,self).__init__('fsk16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk16',  # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class fsk32(_fskmod):
    def __init__(self,**kwargs):
        super(fsk32,self).__init__('fsk32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 10e6,           # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk32',  # Modulation being transmitted
            'bw': 0.6335,           # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class fsk64(_fskmod):
    def __init__(self,**kwargs):
        super(fsk64,self).__init__('fsk64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk64',  # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class fsk128(_fskmod):
    def __init__(self,**kwargs):
        super(fsk128,self).__init__('fsk128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk128', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class fsk256(_fskmod):
    def __init__(self,**kwargs):
        super(fsk256,self).__init__('fsk256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'fsk256', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': -1,
        })
class cpfsk2(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk2,self).__init__('cpfsk2',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk2', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class cpfsk4(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk4,self).__init__('cpfsk4',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk4', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class cpfsk8(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk8,self).__init__('cpfsk8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk8', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class cpfsk16(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk16,self).__init__('cpfsk16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk16',# Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class cpfsk32(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk32,self).__init__('cpfsk32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk32',# Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class cpfsk64(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk64,self).__init__('cpfsk64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk64',# Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class cpfsk128(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk128,self).__init__('cpfsk128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk128',# Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class cpfsk256(_fskmod):
    def __init__(self,**kwargs):
        super(cpfsk256,self).__init__('cpfsk256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'cpfsk256',# Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 1.0,
            'cpfsk_type': 0,
        })
class msk(_fskmod):
    def __init__(self,**kwargs):
        super(msk,self).__init__('msk',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'msk',    # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class scpfsk4(_fskmod):
    def __init__(self,**kwargs):
        super(scpfsk4,self).__init__('scpfsk4',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'scpfsk4',   # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class scpfsk8(_fskmod):
    def __init__(self,**kwargs):
        super(scpfsk8,self).__init__('scpfsk8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'scpfsk8',   # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class scpfsk16(_fskmod):
    def __init__(self,**kwargs):
        super(scpfsk16,self).__init__('scpfsk16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'scpfsk16',  # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class scpfsk32(_fskmod):
    def __init__(self,**kwargs):
        super(scpfsk32,self).__init__('scpfsk32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'scpfsk32',  # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class scpfsk64(_fskmod):
    def __init__(self,**kwargs):
        super(scpfsk64,self).__init__('scpfsk64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'scpfsk64',  # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class scpfsk128(_fskmod):
    def __init__(self,**kwargs):
        super(scpfsk128,self).__init__('scpfsk128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'scpfsk128', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class scpfsk256(_fskmod):
    def __init__(self,**kwargs):
        super(scpfsk256,self).__init__('scpfsk256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'scpfsk256', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 0,
        })
class gmsk(_fskmod):
    def __init__(self,**kwargs):
        super(gmsk,self).__init__('gmsk',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gmsk',   # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })
class gcpfsk4(_fskmod):
    def __init__(self,**kwargs):
        super(gcpfsk4,self).__init__('gcpfsk4',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gcpfsk4',  # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })
class gcpfsk8(_fskmod):
    def __init__(self,**kwargs):
        super(gcpfsk8,self).__init__('gcpfsk8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gcpfsk8',  # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })
class gcpfsk16(_fskmod):
    def __init__(self,**kwargs):
        super(gcpfsk16,self).__init__('gcpfsk16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gcpfsk16', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })
class gcpfsk32(_fskmod):
    def __init__(self,**kwargs):
        super(gcpfsk32,self).__init__('gcpfsk32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gcpfsk32', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })
class gcpfsk64(_fskmod):
    def __init__(self,**kwargs):
        super(gcpfsk64,self).__init__('gcpfsk64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gcpfsk64', # Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })
class gcpfsk128(_fskmod):
    def __init__(self,**kwargs):
        super(gcpfsk128,self).__init__('gcpfsk128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gcpfsk128',# Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })
class gcpfsk256(_fskmod):
    def __init__(self,**kwargs):
        super(gcpfsk256,self).__init__('gcpfsk256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'gcpfsk256',# Modulation being transmitted
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'.
            'mod_index': 0.5,
            'cpfsk_type': 3,
        })