
import numpy as np
from ._profile import _profile
import multiprocessing as mp
from copy import deepcopy


available_mods = ['psk2', 'psk4', 'psk8', 'psk16', 'psk32', 'psk64', 'psk128', 'psk256',
          'dpsk2', 'dpsk4', 'dpsk8', 'dpsk16', 'dpsk32', 'dpsk64', 'dpsk128',
          'dpsk256', 'ask2', 'ask4', 'ask8', 'ask16', 'ask32', 'ask64', 'ask128',
          'ask256', 'qam4', 'qam8', 'qam16', 'qam32', 'qam64', 'qam128', 'qam256',
          'apsk4', 'apsk8', 'apsk16', 'apsk32', 'apsk64', 'apsk128', 'apsk256',
          'bpsk', 'qpsk', 'ook', 'sqam32', 'sqam128', 'V29', 'arb16opt', 'arb32opt',
          'arb64opt', 'arb128opt', 'arb256opt', 'arb64vt', 'pi4dqpsk', 'noise']

class _linmod(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(_linmod,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_linmod'
        self.mod = mod if mod is not None else 'qpsk'
        self.options = profiles.get_base_options('static') + ['json']
        self.option_flags = profiles.get_base_flags('static') + ['-j']
        self.defaults = { ## This are set to the defaults in 'wfgen_linmod'
            'bands':[(100e6,6e9)],
            ####'freq_limits' controlled by 'bands' now
            'span_limits':[(5e6,100e6)],
            'dwell_limits':[(0.02,float('inf'))],####### These two should match here
            'bw_limits':[(5e3,5e6)],
            'squelch_limits':[(0.1,2.0)],
            'band': (2.4e9,2.5e9),  # Band of operation
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qpsk',   # Modulation being transmitted
            'dwell': -1,
            'absence': -1,
            'period': -1,
            'squelch': 0,
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
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
            self.defaults['duration'] = None
            self.defaults['linear_hop'] = None
            opts = profiles.get_base_options('hopper') + ['json']
            flgs = profiles.get_base_flags('hopper') + ['-j']
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        self.stats = deepcopy(self.defaults)
        if self._config is not None:
            self.stats.update(deepcopy(self._config))

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
            try:
                if opt in skip_keys:
                    continue
                if flgs[idx] is None:
                    continue
                # print('{0:<3s}'.format(str(idx)),
                #       '{0:<20s}'.format(str(opt)),
                #       '{0:<6s}'.format(str(opt in params)),
                #       '{0:<6s}'.format(str(opt in self.stats)),
                #       '{0:<6s}'.format(str(opt in self.defaults)),
                #       '{0:<16s}'.format(str(params[opt] if opt in params else None)),
                #       '{0:<16s}'.format(str(self.stats[opt] if opt in self.stats else None)),
                #       '{0:<16s}'.format(str(self.defaults[opt] if opt in self.defaults else None)))
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
            except KeyError as e:
                print("params:",sorted(list(params.keys())))
                print("self.stats:",sorted(list(self.stats.keys())))
                raise e
        cmd += ' -M {}'.format(mod)
        print("RUNNING WITH THIS:\n",cmd,sep='')
        self.stats['modulation'] = mod
        return cmd
    @staticmethod
    def get_options(mode='static'):
        from wfgen import profiles
        if mode == 'static':
            return profiles.get_base_options('static')
        return profiles.get_base_options('hopper')

class noise(_linmod):
    def __init__(self,**kwargs):
        super(noise,self).__init__('noise',**kwargs)
        self._name = 'noise'
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,            # Sample Rate for generation
            'gain': 65,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'noise',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })

class psk2(_linmod):
    def __init__(self,**kwargs):
        super(psk2,self).__init__('psk2',**kwargs)
        self._name = 'bpsk'
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'bpsk',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class psk4(_linmod):
    def __init__(self,**kwargs):
        super(psk4,self).__init__('psk4',**kwargs)
        self._name = 'qpsk'
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qpsk',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class psk8(_linmod):
    def __init__(self,**kwargs):
        super(psk8,self).__init__('psk8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'psk8',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class psk16(_linmod):
    def __init__(self,**kwargs):
        super(psk16,self).__init__('psk16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'psk16',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class psk32(_linmod):
    def __init__(self,**kwargs):
        super(psk32,self).__init__('psk32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'psk32',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class psk64(_linmod):
    def __init__(self,**kwargs):
        super(psk64,self).__init__('psk64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'psk64',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class psk128(_linmod):
    def __init__(self,**kwargs):
        super(psk128,self).__init__('psk128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'psk128',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class psk256(_linmod):
    def __init__(self,**kwargs):
        super(psk256,self).__init__('psk256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'psk256',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class dpsk2(_linmod):
    def __init__(self,**kwargs):
        super(dpsk2,self).__init__('dpsk2',**kwargs)
        self._name = "dbpsk"
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dbpsk',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class dpsk4(_linmod):
    def __init__(self,**kwargs):
        super(dpsk4,self).__init__('dpsk4',**kwargs)
        self._name = "dqpsk"
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dqpsk',
            'bw': 0.675,           # Bandwidth percentage relative to 'rate'
        })
class dpsk8(_linmod):
    def __init__(self,**kwargs):
        super(dpsk8,self).__init__('dpsk8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dpsk8',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class dpsk16(_linmod):
    def __init__(self,**kwargs):
        super(dpsk16,self).__init__('dpsk16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dpsk16',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class dpsk32(_linmod):
    def __init__(self,**kwargs):
        super(dpsk32,self).__init__('dpsk32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dpsk32',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class dpsk64(_linmod):
    def __init__(self,**kwargs):
        super(dpsk64,self).__init__('dpsk64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dpsk64',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class dpsk128(_linmod):
    def __init__(self,**kwargs):
        super(dpsk128,self).__init__('dpsk128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dpsk128',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class dpsk256(_linmod):
    def __init__(self,**kwargs):
        super(dpsk256,self).__init__('dpsk256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'dpsk256',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class ask2(_linmod):
    def __init__(self,**kwargs):
        super(ask2,self).__init__('ask2',**kwargs)
        self._name = 'bpsk'
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'bpsk',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class ask4(_linmod):
    def __init__(self,**kwargs):
        super(ask4,self).__init__('ask4',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ask4',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class ask8(_linmod):
    def __init__(self,**kwargs):
        super(ask8,self).__init__('ask8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ask8',
            'bw': 0.5,              # Bandwidth percentage relative to 'rate'
        })
class ask16(_linmod):
    def __init__(self,**kwargs):
        super(ask16,self).__init__('ask16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ask16',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class ask32(_linmod):
    def __init__(self,**kwargs):
        super(ask32,self).__init__('ask32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ask32',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class ask64(_linmod):
    def __init__(self,**kwargs):
        super(ask64,self).__init__('ask64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ask64',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class ask128(_linmod):
    def __init__(self,**kwargs):
        super(ask128,self).__init__('ask128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ask128',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class ask256(_linmod):
    def __init__(self,**kwargs):
        super(ask256,self).__init__('ask256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ask256',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class qam4(_linmod):
    def __init__(self,**kwargs):
        super(qam4,self).__init__('qam4',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qpsk',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class qam8(_linmod):
    def __init__(self,**kwargs):
        super(qam8,self).__init__('qam8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qam8',
            'bw': 0.16875,          # Bandwidth percentage relative to 'rate'
        })
class qam16(_linmod):
    def __init__(self,**kwargs):
        super(qam16,self).__init__('qam16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qam16',
            'bw': 0.675,              # Bandwidth percentage relative to 'rate'
        })
class qam32(_linmod):
    def __init__(self,**kwargs):
        super(qam32,self).__init__('qam32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qam32',
            'bw': 0.675,              # Bandwidth percentage relative to 'rate'
        })
class qam64(_linmod):
    def __init__(self,**kwargs):
        super(qam64,self).__init__('qam64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qam64',
            'bw': 0.675,              # Bandwidth percentage relative to 'rate'
        })
class qam128(_linmod):
    def __init__(self,**kwargs):
        super(qam128,self).__init__('qam128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 200e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qam128',
            'bw': 0.84375,          # Bandwidth percentage relative to 'rate'
        })
class qam256(_linmod):
    def __init__(self,**kwargs):
        super(qam256,self).__init__('qam256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qam256',
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
        })
class apsk4(_linmod):
    def __init__(self,**kwargs):
        super(apsk4,self).__init__('apsk4',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'apsk4',
            'bw': 0.3375,           # Bandwidth percentage relative to 'rate'
        })
class apsk8(_linmod):
    def __init__(self,**kwargs):
        super(apsk8,self).__init__('apsk8',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 500e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'apsk8',
            'bw': 0.4,              # Bandwidth percentage relative to 'rate'
        })
class apsk16(_linmod):
    def __init__(self,**kwargs):
        super(apsk16,self).__init__('apsk16',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'apsk16',
            'bw': 0.2,              # Bandwidth percentage relative to 'rate'
        })
class apsk32(_linmod):
    def __init__(self,**kwargs):
        super(apsk32,self).__init__('apsk32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'apsk32',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class apsk64(_linmod):
    def __init__(self,**kwargs):
        super(apsk64,self).__init__('apsk64',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'apsk64',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class apsk128(_linmod):
    def __init__(self,**kwargs):
        super(apsk128,self).__init__('apsk128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'apsk128',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class apsk256(_linmod):
    def __init__(self,**kwargs):
        super(apsk256,self).__init__('apsk256',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'apsk256',
            'bw': 0.1,              # Bandwidth percentage relative to 'rate'
        })
class bpsk(_linmod):
    def __init__(self,**kwargs):
        super(bpsk,self).__init__('bpsk',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'bpsk',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class qpsk(_linmod):
    def __init__(self,**kwargs):
        super(qpsk,self).__init__('qpsk',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 2e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'qpsk',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
class ook(_linmod):
    def __init__(self,**kwargs):
        super(ook,self).__init__('ook',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 200e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'ook',
            'bw': 0.84375,          # Bandwidth percentage relative to 'rate'
        })
class sqam32(_linmod):
    def __init__(self,**kwargs):
        super(sqam32,self).__init__('sqam32',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'sqam32',
            'bw': 0.16875,          # Bandwidth percentage relative to 'rate'
        })
class sqam128(_linmod):
    def __init__(self,**kwargs):
        super(sqam128,self).__init__('sqam128',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 1e6,            # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'sqam128',
            'bw': 0.16875,          # Bandwidth percentage relative to 'rate'
        })
class V29(_linmod):
    def __init__(self,**kwargs):
        super(V29,self).__init__('V29',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'V29',
            'bw': 0.05,              # Bandwidth percentage relative to 'rate'
        })
class arb16opt(_linmod):
    def __init__(self,**kwargs):
        super(arb16opt,self).__init__('arb16opt',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'arb16opt',
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
        })
class arb32opt(_linmod):
    def __init__(self,**kwargs):
        super(arb32opt,self).__init__('arb32opt',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'arb32opt',
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
        })
class arb64opt(_linmod):
    def __init__(self,**kwargs):
        super(arb64opt,self).__init__('arb64opt',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'arb64opt',
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
        })
class arb128opt(_linmod):
    def __init__(self,**kwargs):
        super(arb128opt,self).__init__('arb128opt',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'arb128opt',
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
        })
class arb256opt(_linmod):
    def __init__(self,**kwargs):
        super(arb256opt,self).__init__('arb256opt',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'arb256opt',
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
        })
class arb64vt(_linmod):
    def __init__(self,**kwargs):
        super(arb64vt,self).__init__('arb64vt',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'arb64vt',
            'bw': 0.7,              # Bandwidth percentage relative to 'rate'
        })
class pi4dqpsk(_linmod):
    def __init__(self,**kwargs):
        super(pi4dqpsk,self).__init__('pi4dqpsk',**kwargs)
        self.defaults.update({
            'frequency': 2.46e9,    # Carrier Frequency
            'rate': 100e3,          # Sample Rate for generation
            'gain': 50.0,           # TX Gain setting
            'gain_range': 0.0,      # Should the gain change at all over a transmission? 0 is No
            'gain_cycle': 2,        # Gain will cycle over this many seconds if enabled
            'modulation': 'pi4dqpsk',
            'bw': 0.675,            # Bandwidth percentage relative to 'rate'
        })
