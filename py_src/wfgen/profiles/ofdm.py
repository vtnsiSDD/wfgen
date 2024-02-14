
import numpy as np
from ._profile import _profile
import multiprocessing as mp
from copy import deepcopy


available_mods = ['ofdm','wifi_ag','wifi_ac','lte']
available_subcarrier_mods = ['psk2', 'psk4', 'psk8', 'psk16', 'psk32', 'psk64', 'psk128', 'psk256',
          'dpsk2', 'dpsk4', 'dpsk8', 'dpsk16', 'dpsk32', 'dpsk64', 'dpsk128',
          'dpsk256', 'ask2', 'ask4', 'ask8', 'ask16', 'ask32', 'ask64', 'ask128',
          'ask256', 'qam4', 'qam8', 'qam16', 'qam32', 'qam64', 'qam128', 'qam256',
          'apsk4', 'apsk8', 'apsk16', 'apsk32', 'apsk64', 'apsk128', 'apsk256',
          'bpsk', 'qpsk', 'ook', 'sqam32', 'sqam128', 'V29', 'arb16opt', 'arb32opt',
          'arb64opt', 'arb128opt', 'arb256opt', 'arb64vt', 'pi4dqpsk']

class _ofdmmod(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(_ofdmmod,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_wbofdmgen'
        self.mod = mod if mod is not None else 'ofdm'
        self.car_mod = kwargs['car_mod'] if 'car_mod' in kwargs and kwargs['car_mod'] is not None else 'qpsk'
        self.options = profiles.get_base_options('ofdm') + ['json']
        self.option_flags = profiles.get_base_flags('ofdm') + ['-j']
        self.defaults = { ## This are set to the defaults in 'wfgen_linmod'
            'bands':[(100e6,6e9)],
            ####'freq_limits' controlled by 'bands' now
            'span_limits':[(5e6,100e6)],
            'dwell_limits':[(0.02,float('inf'))],####### These two should match here
            'bw_limits':[(5e3,5e6)],
            'squelch_limits':[(0.1,2.0)],
            'band': (2.4e9,2.5e9),  # Band of operation
            'frequency': 2.46e9,    # Carrier Frequency
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ofdm',   # Modulation being transmitted
            'dwell': 0.05,
            'absence': 0.95,
            'period': 0.1,
            'squelch': 0,
            'bw': 0.82,              # Bandwidth percentage relative to 'rate'
            'rate': 1e6,
            'dwell': 0.05,
            'num_bursts': 10,
            'span': 0.9,
            'nfft': 64,
            'used_carriers': 52,
            'cplen': 16,
            'num_channels': 1,
            'car_mod': "qpsk",
            'min_syms': 500,
            'max_syms': 500,
            'loop_delay': 0.5,
            'num_loops': 5,
            'duration': None,
            'linear_hop': None,
        }
        self._config = None
    def config(self,**kwargs):
        self._config = kwargs ### being lazy here
    def start(self,radio_args,params=dict()):
        from wfgen import profiles
        opts = self.options
        flgs = self.option_flags
        hopping = True
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        if self.car_mod is None:
            car_mod = self.defaults['car_mod']
        else:
            car_mod = self.car_mod
        self.stats = deepcopy(self.defaults)
        if self._config is not None:
            self.stats.update(deepcopy(self._config))

        params, self.stats = profiles.resolve_band_freq_options(params,self.stats)
        skip_keys = ['loop_delay','num_loops']
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
            try:
                if opt in skip_keys:
                    continue
                # print('{0:<3s}'.format(str(idx)),
                #       '{0:<20s}'.format(str(opt)),
                #       '{0:<6s}'.format(str(opt in params)),
                #       '{0:<6s}'.format(str(opt in self.stats)),
                #       '{0:<6s}'.format(str(opt in self.defaults)),
                #       '{0:<16s}'.format(str(params[opt] if opt in params else None)),
                #       '{0:<16s}'.format(str(self.stats[opt] if opt in self.stats else None)),
                #       '{0:<16s}'.format(str(self.defaults[opt] if opt in self.defaults else None)))
                if opt in ['car_mod']:
                    if opt in params:
                        car_mod = params[opt]
                    continue
                if opt in ["linear_hop"]:
                    if opt in params:
                        if params[opt]:
                            cmd += ' {0:s}'.format(flgs[idx])
                    else:
                        if self.stats[opt]:
                            cmd += ' {0:s}'.format(flgs[idx])
                else:
                    if opt in params:
                        if params[opt] is not None:
                            cmd += ' {0:s} {1}'.format(flgs[idx],params[opt])
                            self.stats[opt] = params[opt]
                    else:
                        if self.stats[opt] is not None:
                            # print(opt,opt in self.stats,flgs,idx)
                            cmd += ' {0:s} {1}'.format(flgs[idx],self.stats[opt])
            except (KeyError,TypeError) as e:
                print("option:",opt,"index:",idx)
                print("params:",sorted(list(params.keys())))
                print("self.stats:",sorted(list(self.stats.keys())))
                raise e
        cmd += ' -M {}'.format(car_mod)
        print("RUNNING WITH THIS:\n",cmd,sep='')
        self.stats['modulation'] = mod
        self.stats['car_mod'] = car_mod
        return cmd
    @staticmethod
    def get_options(mode='static'):
        from wfgen import profiles
        return profiles.get_base_options("ofdm")


class ofdm(_ofdmmod):
    def __init__(self,**kwargs):
        super(ofdm,self).__init__('ofdm',**kwargs)
        self._name = 'ofdm'

class wifi_ag(_ofdmmod):
    def __init__(self,**kwargs):
        super(wifi_ag,self).__init__('wifi_ag',**kwargs)
        self._name = 'wifi_ag'
        self.defaults.update({
            'rate': 5e6,            # Sample Rate for generation
            'gain': 65,           # TX Gain setting
            'modulation': 'wifi_ag',
            'bw': 0.82,            # Bandwidth percentage relative to 'rate'
        })

class wifi_ac(_ofdmmod):
    def __init__(self,**kwargs):
        super(wifi_ac,self).__init__('wifi_ac',**kwargs)
        self._name = 'wifi_ac'
        self.defaults.update({
            'rate': 20e6,            # Sample Rate for generation
            'gain': 65,           # TX Gain setting
            'modulation': 'wifi_ac',
            'used_carriers': 56,
            'bw': 0.89,            # Bandwidth percentage relative to 'rate'
        })

class lte(_ofdmmod):
    def __init__(self,**kwargs):
        super(lte,self).__init__('lte',**kwargs)
        self._name = 'lte'
        self.defaults.update({
            'rate': 20e6,            # Sample Rate for generation
            'gain': 65,           # TX Gain setting
            'modulation': 'lte',
            'nfft': 1024,
            'used_carriers': 600,
            'cplen': 144,
            'bw': 0.59,            # Bandwidth percentage relative to 'rate'
        })
