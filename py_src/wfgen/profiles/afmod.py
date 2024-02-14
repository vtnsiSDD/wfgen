
import numpy as np
from ._profile import _profile
import multiprocessing as mp
from copy import deepcopy

available_mods = ["am_constant","am_square","am_triangle","am_sawtooth","am_sinusoid","am_wav_file","am_rand_uni","am_rand_gauss",
                  "fm_constant","fm_square","fm_triangle","fm_sawtooth","fm_sinusoid","fm_wav_file","fm_rand_uni","fm_rand_gauss"]

class _ammod(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(_ammod,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_am'
        self.mod = mod if mod is not None else 'am_wav_file'
        self.options = profiles.get_base_options('static','analog') + ['json']
        self.option_flags = profiles.get_base_flags('static','analog') + ['-j']
        self.defaults = {
            'bands':[(100e6,6e9)],
            'span_limits':[(5e6,100e6)],
            'dwell_limits':[0.02,float('inf')],
            'squelch_limits':[(0.1,2.0)],
            'band':(2.4e9,2.5e9),
            'frequency': 2.46e9,
            'rate': 380e3,
            'gain': 65,
            'gain_range': 0.0,
            'gain_cycle': 2,
            'dwell': -1,
            'absence': -1,
            'period': -1,
            'squelch': -1,
            'src_fq': 1e3,
            'bw': 0.7,
            'mod_index': 0.75,
            'wf_freq': 20000
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
            opts = profiles.get_base_options('hopper','analog') + ['json']
            flgs = profiles.get_base_flags('hopper','analog') + ['-j']
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        self.stats = deepcopy(self.defaults)
        if self._config is not None:
            self.stats.update(deepcopy(self._config))
        
        params, self.stats = profiles.resolve_band_freq_options(params,self.stats)
        skip_keys = ['band']
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
                if opt in params:
                    if params[opt] is not None:
                        cmd += ' {0:s} {1}'.format(flgs[idx],params[opt])
                        self.stats[opt] = params[opt]
                else:
                    if self.stats[opt] is not None:
                        cmd += ' {0:s} {1}'.format(flgs[idx],self.stats[opt])
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


class _fmmod(_profile):
    def __init__(self,mod=None,**kwargs):
        from wfgen import profiles
        super(_fmmod,self).__init__(self.__class__.__name__,**kwargs)
        self.base_command = 'wfgen_fm'
        self.mod = mod if mod is not None else 'fm_wav_file'
        self.options = profiles.get_base_options('static','analog') + ['json']
        self.option_flags = profiles.get_base_flags('static','analog') + ['-j']
        self.defaults = {
            'bands':[(100e6,6e9)],
            'span_limits':[(5e6,100e6)],
            'dwell_limits':[0.02,float('inf')],
            'squelch_limits':[(0.1,2.0)],
            'band':(2.4e9,2.5e9),
            'frequency': 2.46e9,
            'rate': 380e3,
            'gain': 50.0,
            'gain_range': 0.0,
            'gain_cycle': 2,
            'dwell': float('inf'),
            'squelch': 0,
            'src_fq': 5,
            'bw': 0.7,
            'mod_index': 0.25,
            'wf_freq': 0.2
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
            opts = profiles.get_base_options('hopper','analog') + ['json']
            flgs = profiles.get_base_flags('hopper','analog') + ['-j']
        if self.mod is None:
            mod = self.defaults['modulation']
        else:
            mod = self.mod
        self.stats = deepcopy(self.defaults)
        if self._config is not None:
            self.stats.update(deepcopy(self._config))
        
        params, self.stats = profiles.resolve_band_freq_options(params,self.stats)
        skip_keys = ['band']
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
                        cmd += ' {0:s} {1}'.format(flgs[idx],self.stats[opt])
        cmd += ' -M {}'.format(mod)
        print("RUNNING WITH THIS:\n",cmd,sep='')
        self.stats['modulation'] = mod
        print(cmd)
        return cmd
    @staticmethod
    def get_options(mode='static'):
        from wfgen import profiles
        if mode == 'static':
            return profiles.get_base_options('static')
        return profiles.get_base_options('hopper')

class am_constant(_ammod):
    def __init__(self,**kwargs):
        super(am_constant,self).__init__('am_constant',**kwargs)

class am_square(_ammod):
    def __init__(self,**kwargs):
        super(am_square,self).__init__('am_square',**kwargs)

class am_triangle(_ammod):
    def __init__(self,**kwargs):
        super(am_triangle,self).__init__('am_triangle',**kwargs)

class am_sawtooth(_ammod):
    def __init__(self,**kwargs):
        super(am_sawtooth,self).__init__('am_sawtooth',**kwargs)

class am_sinusoid(_ammod):
    def __init__(self,**kwargs):
        super(am_sinusoid,self).__init__('am_sinusoid',**kwargs)

class am_wav_file(_ammod):
    def __init__(self,**kwargs):
        super(am_wav_file,self).__init__('am_wav_file',**kwargs)

class am_rand_uni(_ammod):
    def __init__(self,**kwargs):
        super(am_rand_uni,self).__init__('am_rand_uni',**kwargs)

class am_rand_gauss(_ammod):
    def __init__(self,**kwargs):
        super(am_rand_gauss,self).__init__('am_rand_gauss',**kwargs)



class fm_constant(_fmmod):
    def __init__(self,**kwargs):
        super(fm_constant,self).__init__('fm_constant',**kwargs)

class fm_square(_fmmod):
    def __init__(self,**kwargs):
        super(fm_square,self).__init__('fm_square',**kwargs)

class fm_triangle(_fmmod):
    def __init__(self,**kwargs):
        super(fm_triangle,self).__init__('fm_triangle',**kwargs)

class fm_sawtooth(_fmmod):
    def __init__(self,**kwargs):
        super(fm_sawtooth,self).__init__('fm_sawtooth',**kwargs)

class fm_sinusoid(_fmmod):
    def __init__(self,**kwargs):
        super(fm_sinusoid,self).__init__('fm_sinusoid',**kwargs)

class fm_wav_file(_fmmod):
    def __init__(self,**kwargs):
        super(fm_wav_file,self).__init__('fm_wav_file',**kwargs)

class fm_rand_uni(_fmmod):
    def __init__(self,**kwargs):
        super(fm_rand_uni,self).__init__('fm_rand_uni',**kwargs)

class fm_rand_gauss(_fmmod):
    def __init__(self,**kwargs):
        super(fm_rand_gauss,self).__init__('fm_rand_gauss',**kwargs)
