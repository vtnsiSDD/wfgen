

from ..utils import have_pygr

if have_pygr():
    from gnuradio import gr,filter,analog,digital,blocks
    from gnuradio.filter import firdes
    from gnuradio.fft import window

import glob,os,array,zmq,yaml,time

import numpy as np

from typing import Union,List

from pydub import AudioSegment

from fractions import Fraction

from tqdm import tqdm


def get_audio_files(source_dir=None):
    import os,glob
    if 'WFGEN_AUDIO_FOLDER' in os.environ and source_dir is None:
        source_dir = os.environ['WFGEN_AUDIO_FOLDER']
    elif source_dir is None:
        raise ValueError("Environment parameter 'WFGEN_AUDIO_FOLDER' not set, specify 'source_dir'")
    combined_files = glob.glob(os.path.join(source_dir,'combined_audio*.wav'))
    if len(combined_files) == 0:
        combined_files = glob.glob(os.path.join(source_dir,'*.wav'))
    return sorted(combined_files)

class analogs_available_mods:
    NONE=off=Off=OFF=none = None
    MANCHESTER=manchester=Manchester = 0
    BPSK=bpsk = 1
    DSBSC=dsbsc = 2
    DSB=dsb = 3
    TONE=tone=Tone = 4
    WHITENOISE=whitenoise=white_noise=White_Noise = 5

    @staticmethod
    def get_type(string: Union[str,int]):
        if string in  [analogs_available_mods.none,
                       analogs_available_mods.manchester,
                       analogs_available_mods.bpsk,
                       analogs_available_mods.dsbsc,
                       analogs_available_mods.dsb,
                       analogs_available_mods.tone,
                       analogs_available_mods.white_noise]:
            return string
        if string[0] in ['B','b']:
            return analogs_available_mods.BPSK
        if string[0] in ['M','m']:
            return analogs_available_mods.MANCHESTER
        if string[0] in ['D','d']:
            if string[-1] in ['B','b']:
                return analogs_available_mods.DSB
            return analogs_available_mods.DSBSC
        if string[0] in ['T','t']:
            return analogs_available_mods.TONE
        if string[0] in ['W','w']:
            return analogs_available_mods.WHITENOISE
        return analogs_available_mods.NONE

    @staticmethod
    def get_name(mod_type: Union[str,int,None]):
        if isinstance(mod_type,str) and mod_type.lower() in [
            "manchester","bpsk","dsbsc","dsb","tone","white_noise"]:
            return mod_type
        if mod_type == 0:
            return "manchester"
        if mod_type == 1:
            return "bpsk"
        if mod_type == 2:
            return "dsbsc"
        if mod_type == 3:
            return "dsb"
        if mod_type == 4:
            return "tone"
        if mod_type == 4:
            return "white_noise"
        return "off"


class analogs_path_types:
    tone = 0
    mono = 1
    stereo = 2
    rds = 3
    other = 4
    left = 5
    right = 6

    @staticmethod
    def get_type(string: Union[str,int]):
        if string in [analogs_path_types.tone,analogs_path_types.mono,
                      analogs_path_types.stereo,analogs_path_types.rds,
                      analogs_path_types.other]:
            return string
        if string[0] in ['t','T']:
            return analogs_path_types.tone
        if string[0] in ['m','M']:
            return analogs_path_types.mono
        if string[0] in ['s','S']:
            return analogs_path_types.stereo
        if string[0] in ['r','R']:
            if string[1] in ['d','D']:
                return analogs_path_types.rds
            else:
                return analogs_path_types.right
        if string[0] in ['l','L']:
            return analogs_path_types.left
        return analogs_path_types.other

    @staticmethod
    def get_name(path_type: Union[str,int]):
        if isinstance(path_type,str) and path_type.lower() in [
            "tone", "mono", "stereo", "rds", 'other','right','left']:
            return path_type
        if path_type == 0:
            return "tone"
        if path_type == 1:
            return "mono"
        if path_type == 2:
            return "stereo"
        if path_type == 3:
            return "rds"
        if path_type == 5:
            return "left"
        if path_type == 6:
            return "right"
        return 'other'


class AudioClient(object):
    def __init__(self,endpoint=None,offset=None,silent=False):
        if endpoint is None:
            if 'WG_AUDIO_SERVER' not in os.environ:
                print("Endpoint must be manually set, 'WG_AUDIO_SERVER' not found in environ")
            else:
                endpoint = os.environ['WG_AUDIO_SERVER']
        self.endpoint = endpoint

        self.ctx = None
        self.socket = None
        self.length = None
        self.nfiles = None
        self.fs = None
        self.dims = None
        self.offset = 0 if offset is None else offset
        self.silent = silent

    def __len__(self):
        if self.length is None:
            if self.socket is None:
                self.connect()
            self.socket.send(yaml.dump({'get':'len'}).encode('utf-8'))
            poller = zmq.Poller()
            poller.register(self.socket,zmq.POLLIN)
            limiter = 0
            while True:
                socks = dict(poller.poll(100))##ms
                if self.socket in socks:
                    response = self.socket.recv_multipart()
                    self.length = int.from_bytes(response[0],'little')
                    break
                else:
                    limiter += 1
                    if not self.silent:
                        print('.',end='',flush=True)
                if limiter >= 100:
                    if not self.silent:
                        print("Server Timeout")
                    return 0
        return self.length

    def get_nsamps(self,samples):
        if not isinstance(samples,(int,float)):
            raise ValueError("Must specify the number of samples as an int")
        if self.socket is None:
            self.connect()
        samples = int(samples)
        self.socket.send(yaml.dump({'start':int(self.offset),'len':int(samples)}).encode('utf-8'))
        poller = zmq.Poller()
        poller.register(self.socket,zmq.POLLIN)
        limiter = 0
        while True:
            socks = dict(poller.poll(100))##ms
            if self.socket in socks:
                response = self.socket.recv_multipart()
                if len(response[0]) < samples//8:
                    print(response[0].decode('utf-8'))
                    return None
                samples = np.frombuffer(response[0],np.single).reshape(-1,2)
                self.offset += samples.shape[0]
                self.offset = int(self.offset % len(self))
                break
            else:
                limiter += 1
                if not self.silent:
                    print('.',end='',flush=True)
            if limiter >= 100:
                if not self.silent:
                    print("Server Timeout")
                samples = None
                break
        return samples

    def close(self):
        self.disconnect()
        if self.ctx is not None:
            self.ctx.term()

    def disconnect(self):
        if self.socket is not None:
            self.socket.setsockopt(zmq.LINGER,0)
            self.socket.close()

    def connect(self):
        if self.ctx is not None:
            self.close()
        self.ctx = zmq.Context()
        self.socket = self.ctx.socket(zmq.REQ)
        self.socket.connect(self.endpoint)

    @property
    def shape(self):
        if self.dims is None:
            if self.socket is None:
                self.connect()
            self.socket.send(yaml.dump({'get':'shape'}).encode('utf-8'))
            poller = zmq.Poller()
            poller.register(self.socket,zmq.POLLIN)
            limiter = 0
            while True:
                socks = dict(poller.poll(100))##ms
                if self.socket in socks:
                    response = self.socket.recv_multipart()
                    self.dims = tuple(yaml.safe_load(response[0]))
                    break
                else:
                    limiter += 1
                    if not self.silent:
                        print('.',end='',flush=True)
                if limiter >= 100:
                    if not self.silent:
                        print("Server Timeout")
                    break
        return self.dims

    @property
    def file_count(self):
        if self.nfiles is None:
            if self.socket is None:
                self.connect()
            self.socket.send(yaml.dump({'get':'file_count'}).encode('utf-8'))
            poller = zmq.Poller()
            poller.register(self.socket,zmq.POLLIN)
            limiter = 0
            while True:
                socks = dict(poller.poll(100))##ms
                if self.socket in socks:
                    response = self.socket.recv_multipart()
                    self.nfiles = int.from_bytes(response[0],'little')
                    break
                else:
                    limiter += 1
                    if not self.silent:
                        print('.',end='',flush=True)
                if limiter >= 100:
                    if not self.silent:
                        print("Server Timeout")
                    break
        return self.nfiles

    @property
    def sample_rate(self):
        if self.fs is None:
            if self.socket is None:
                self.connect()
            self.socket.send(yaml.dump({'get':'sample_rate'}).encode('utf-8'))
            poller = zmq.Poller()
            poller.register(self.socket,zmq.POLLIN)
            limiter = 0
            while True:
                socks = dict(poller.poll(100))##ms
                if self.socket in socks:
                    response = self.socket.recv_multipart()
                    self.fs = float(int.from_bytes(response[0],'little'))
                    break
                else:
                    limiter += 1
                    if not self.silent:
                        print('.',end='',flush=True)
                if limiter >= 100:
                    if not self.silent:
                        print("Server Timeout")
                    break
        return self.fs


class AudioCombiner(object):
    def __init__(self,source_dir:Union[str,List[str]]=None,use_base=True):
        if source_dir is None:
            self.source_dir = get_audio_files()
        else:
            if isinstance(source_dir,str):
                source_dir = [source_dir]
            self.source_dir = source_dir
        self.source_files = []
        self.must_find = False
        self.found = False
        self.src_fusion = None
        if not use_base:
            self._find_sources()
        else:
            import wfgen as wg
            Base = wg.BaseAudio
            if Base is None:
                self._find_sources()
            else:
                self.found = Base.found
                self.src_fusion = Base.src_fusion
                self.source_files = [x for x in Base.source_files]

    def _find_sources(self):
        ####
        # Check if the combined data for this folder already exists 
        ####
        new_sounds = False
        if self.source_dir:
            for sd in self.source_dir:
                if os.path.isdir(sd):
                    combined_audio_file = os.path.join(sd,'combined_audio*.wav')
                    possible_srcs = glob.glob(os.path.join(sd,'*.wav'))
                    if len(glob.glob(combined_audio_file)):
                        combined_srcs = glob.glob(combined_audio_file)
                        for caf in combined_srcs:
                            del possible_srcs[possible_srcs.index(caf)]
                        compare_time = min([os.path.getmtime(x) for x in combined_srcs])
                        time_check = [os.path.getmtime(x) > compare_time for x in possible_srcs]
                        if not any(time_check):
                            possible_srcs = combined_srcs
                        else:
                            new_sounds = True
                    else:
                        new_sounds = True
                    if possible_srcs:
                        self.source_files += possible_srcs
                elif os.path.isfile(sd):
                    self.source_files.append(sd)
        self.source_files = sorted(self.source_files)
        if(self.must_find and len(self.source_files) < 1):
            raise ValueError("No 'wav' source files found")
        else:
            self.found = True
        if not new_sounds:
            self.fuse(interleave_silence_ms=0)

    def __len__(self):
        if self.src_fusion is None:
            return 0
        return int(self.src_fusion.frame_count())

    @property
    def shape(self):
        if self.src_fusion is None:
            return tuple
        return (self.src_fusion.frame_count(),self.src_fusion.channels)

    @property
    def file_count(self):
        return len(self.source_files)

    @property
    def sample_rate(self):
        if self.src_fusion is None:
            return 0
        return self.src_fusion.frame_rate

    def fuse(self,fs=48000.0,interleave_silence_ms=1000,verbose=False):
        if not self.found:
            self.must_find = True
            self._find_sources()
        self.src_fusion = AudioSegment.empty()
        for idx,sf in enumerate(self.source_files):
            if(verbose):
                print(os.path.basename(sf))

            ##### Open wav file
            wf = AudioSegment.from_file(sf,format='wav')
            # print("\n".join(dir((wf))))

            ##### Make sure using a stereo format
            if(verbose):
                print('channels',wf.channels)
            if wf.channels == 1:
                wf = AudioSegment.from_mono_audiosegments(wf,wf)
                if(verbose):
                    print('new channels',wf.channels)

            ##### Keep audio at a constant rate for all sources
            if(verbose):
                print('frame_rate',wf.frame_rate)
            if wf.frame_rate != int(fs):
                wf = wf.set_frame_rate(int(fs))
                if(verbose):
                    print('new frame_rate',wf.frame_rate)

            ##### Trim off silence by FS reference
            def detect_leading_silence(sound,silence_threshold=-50.0,chunk_size=10,pbar=None):
                """
                Returns the millisecond/index that the leading silence ends.

                audio_segment - the segment to find silence in
                silence_threshold - the upper bound for how quiet is silent in dFBS
                chunk_size - chunk size for interating over the segment in ms

                Taken from pydub.silence
                """
                trim_ms = 0 # ms
                assert chunk_size > 0 # to avoid infinite loop
                iterations = int(np.ceil(len(sound)/10))
                if pbar is not None:
                    step = 1/(2*iterations)
                # while sound[trim_ms:trim_ms+chunk_size].dBFS < silence_threshold and trim_ms < len(sound):
                #     trim_ms += chunk_size
                for idx in range(iterations):
                    if sound[idx*chunk_size:(idx+1)*chunk_size].dBFS >= silence_threshold:
                        trim_ms = idx*chunk_size
                    if pbar is not None:
                        pbar.update(step)
                    if trim_ms > 0:
                        break

                # if there is no end it should return the length of the segment
                return min(trim_ms, len(sound))
            def clip_leading_silence(sound,silence_threshold=-40,chunk_size=50,pbar=None):
                new_sound = sound[detect_leading_silence(sound,silence_threshold,chunk_size,pbar=pbar):]
                return new_sound._spawn(new_sound.get_array_of_samples())
            def clip_silence(sound):
                chunk_size = 50 #ms
                iterations = int(np.ceil(len(sound)/chunk_size))
                pbar = tqdm(total=1.0)
                sound = clip_leading_silence(sound.reverse(),pbar=pbar).reverse()
                pbar.n = 0.5-1/(2*iterations)
                pbar.update(1/(2*iterations))
                sound = clip_leading_silence(sound,pbar=pbar)
                pbar.n = 1-1/(2*iterations)
                pbar.update(1/(2*iterations))
                return sound._spawn(sound.get_array_of_samples()),pbar

            orig_len = len(wf)
            wf,pbar = clip_silence(wf)
            new_len = len(wf)
            if verbose:
                print("Audio clipped from {}ms to {}ms".format(orig_len,new_len))

            ##### Normalize FS of the wav file
            iterations = int(np.ceil(new_len/50))
            step_size = 1.0/iterations
            pbar.update(-1.0)
            dbpwr = list()
            for idx in range(iterations):
                dbpwr.append(wf[50*idx:50*(idx+1)].dBFS)
                pbar.update(step_size)
            pbar.n = 1-step_size
            pbar.update(step_size)
            pbar.close()
            dbfs = np.array(dbpwr,np.float64)
            amp2_fs = np.power(10.,dbfs/10.0)
            avg_power = np.mean(amp2_fs)
            avg_db = 10.*np.log10(avg_power)
            gain = 0-avg_db
            wf = wf.apply_gain(gain)

            ##### Collect all changes
            wf = wf._spawn(wf.get_array_of_samples())
            self.src_fusion += wf
            if idx < len(self.source_files)-1 and interleave_silence_ms > 0:
                self.src_fusion += AudioSegment.silent(duration=interleave_silence_ms)#ms

    def export_fused(self,filepath=os.path.join(os.environ["WFGEN_AUDIO_FOLDER"] if "WFGEN_AUDIO_FOLDER" in os.environ else ".","combined_audio.wav"),format='wav'):
        if self.src_fusion is None:
            raise RuntimeError("No source to export")
        byte_depth = self.src_fusion.sample_width
        channels = self.src_fusion.channels
        time_len = self.src_fusion.duration_seconds
        count = self.src_fusion.frame_count()
        rate = self.src_fusion.frame_rate
        report_size = byte_depth * channels * count
        if(report_size < (np.iinfo(np.uint32).max*0.9)):
            self.src_fusion.export(filepath,format=format)
        else:
            n_files = int(np.ceil(report_size/(np.iinfo(np.uint32).max*0.9)))
            proto = filepath[:-4]+"{0:04d}"+filepath[-4:]
            chunk_len = int(np.ceil(count / n_files))
            for f_idx in range(n_files):
                wav_chunk = self.src_fusion.get_sample_slice(
                    int(chunk_len*f_idx), int(chunk_len*(1+f_idx))
                )
                wav_chunk.export(proto.format(f_idx),format='wav')

    def get_numpy(self,norm=True,scale_to=0.9,scale_by='mono'):
        if self.src_fusion is None:
            raise RuntimeError("No source to return")
        src = self.src_fusion.split_to_mono()
        l = np.array(src[0].get_array_of_samples(),dtype=np.single)
        r = np.array(src[1].get_array_of_samples(),dtype=np.single)
        scale = 1.0
        if norm:
            if isinstance(scale_by,str) and scale_by.lower() in ['mono','stereo']:
                scale = max(np.max(np.abs(l+r)),np.max(np.abs(l-r)))/scale_to
            else:
                scale = max(np.max(np.abs(l)),np.max(np.abs(r)))/scale_to
        return np.vstack([l,r]).T.copy()/scale

class _InfoPath(object):
    def __init__(self,input_fs,output_fs,bw,rate,fc_shift,waveform,dp_type,gain=None,gain_dB=None):
        self.input_fs = input_fs
        self.output_fs = output_fs
        self.bw = bw # assumed to be double-sided bandwidth
        self.rate = rate
        if not any([gain is None,gain_dB is None]):
            raise RuntimeError("Parameter 'gain' or 'gain_dB' must be defined")
        if all([gain is not None,gain_dB is not None]):
            raise RuntimeError("Only 'gain' or 'gain_dB' can be defined")
        self.gain = gain if gain is not None else 10**(gain_dB/20)
        self.fc_shift = fc_shift
        self.waveform = waveform
        self.dp_type = dp_type

    def __str__(self):
        out = ' | '.join([
            f"{self.__class__.__name__}(".rjust(12) + f"{self.input_fs:.1f}".ljust(9) if self.input_fs is not None else "None".ljust(9),
            f"{self.output_fs:.1f}".ljust(9) if self.output_fs is not None else "None".ljust(9),
            f"{self.bw:.1f}".ljust(8) if self.bw is not None else "None".ljust(8),
            f"{self.rate:.1f}".ljust(8) if self.rate is not None else "None".ljust(8),
            f"{self.gain:.3f}".ljust(4) if self.gain is not None else "None".ljust(4),
            f"{self.fc_shift:.1f}".ljust(9) if self.fc_shift is not None else "None".ljust(9),
            f"{self.waveform if isinstance(self.waveform,str) else analogs_available_mods.get_name(self.waveform)}".ljust(10),
            f"{self.dp_type if isinstance(self.dp_type, str) else analogs_path_types.get_name(self.dp_type)})".ljust(6),
        ])
        return out
    
    def make_dict(self):
        out = dict()
        out["path_type"] = str(self.__class__.__name__)
        out["input_fs"] = self.input_fs
        out["output_fs"] = self.output_fs
        out["bw"] = self.bw
        out["rate"] = self.rate
        out["gain"] = self.gain
        out["fc_shift"] = self.fc_shift
        out["waveform"] = self.waveform if isinstance(self.waveform,str) else analogs_available_mods.get_name(self.waveform)
        out["dp_type"] = self.dp_type if isinstance(self.dp_type, str) else analogs_path_types.get_name(self.dp_type)
        return out

    def sanitize(self):
        pass

class AnalogPath(_InfoPath):
    def __init__(self,input_fs,output_fs,bw,fc_shift,waveform,dp_type,gain=None,gain_dB=None):
        super().__init__(input_fs,output_fs,bw,None,fc_shift,waveform,dp_type,gain,gain_dB)

class DigitalPath(_InfoPath):
    def __init__(self,input_fs,output_fs,rate,fc_shift,waveform,dp_type,gain=None,gain_dB=None):
        super().__init__(input_fs,output_fs,None,rate,fc_shift,waveform,dp_type,gain,gain_dB)


if have_pygr():
    import pmt
    from datetime import datetime,timezone

    def get_time():
        return datetime.now(timezone.utc).timestamp()

    class analog_src(gr.sync_block):
        def __init__(self,modes=["mono"],client_port=None,data_key='',offset=None,
                    ptt_off_range=None,source_file=None,source_folder=None,verbose=False):
            if modes is None or len(modes) < 1:
                raise ValueError("Modes needs to be defined in a list of minimum length 1")
            gr.sync_block.__init__(self,"AudioSrcTweaker",[],[np.float32]*len(modes))
            self.modes = modes
            self.source = AudioClient(None if client_port is None else 'tcp://127.0.10.10:{}'.format(client_port))
            self._mode = 0
            if self.source.endpoint is None or len(self.source)==0:
                if source_file is None and source_folder is None:
                    if "WFGEN_AUDIO_FOLDER" not in os.environ:
                        raise ValueError("No source specified")
                    self.source = AudioCombiner(get_audio_files())
                else:
                    if source_file is not None and not isinstance(source_file,list):
                        source_file = [source_file]
                    elif source_file is None:
                        source_file = []
                    if source_folder is not None and not isinstance(source_folder,list):
                        source_folder = [source_folder]
                    elif source_folder is None:
                        source_folder = []
                    self.source = AudioCombiner(source_file+source_folder)
                self._mode = 1
                self.audio = self.source.get_numpy()
                if offset is None:
                    self.offset = np.random.randint(self.audio.shape[0])
            else:
                if offset is None:
                    self.offset = np.random.randint(self.source.shape[0])
                self.source.offset = self.offset
            self.ptt_range = ptt_off_range
            self.sample_rate = self.source.sample_rate
            self.data_key = data_key
            self.in_key = pmt.intern("in")
            self.msg_q = []
            self.active_buf = None
            self.consumed = None
            self.message_port_register_in(self.in_key)
            self.set_msg_handler(self.in_key,self.msg_handler)
            self.verbose=verbose
            self.initial = get_time()
            self.down_time_end = None
            self.down_time_start = None
            
        def msg_handler(self,msg):
            if not pmt.is_pair(msg):
                print("invalid msg structure expecting-> (() . #[])")
                return #invalid msg structure
            cdr = pmt.cdr(msg)
            print(cdr)
            if pmt.is_blob(cdr):
                request = np.array(pmt.u8vector_elements(cdr),dtype=np.uint8)
                self.msg_q.append(int(request.view(np.uint64)[0]))

        def prep_next(self):
            if len(self.msg_q) == 0:
                return False
            request_size = self.msg_q[0]
            del self.msg_q[0]
            if self._mode == 0:
                stereo_audio = self.source.get_nsamps(request_size)
            else:
                stereo_audio = self.audio[self.offset:self.offset+request_size]
                if stereo_audio.shape[0] < request_size:
                    stereo_audio = np.concatenate([stereo_audio,self.audio[:request_size-stereo_audio.shape[0]]])
            self.active_buf = np.zeros((len(self.modes),request_size),np.single)
            for idx,mode in enumerate(self.modes):
                if mode == "mono":
                    self.active_buf[idx,:] = stereo_audio[:,0]+stereo_audio[:,1]
                elif mode == "stereo":
                    self.active_buf[idx,:] = stereo_audio[:,0]-stereo_audio[:,1]
                elif mode == "left":
                    self.active_buf[idx,:] = stereo_audio[:,0]
                elif mode == "right":
                    self.active_buf[idx,:] = stereo_audio[:,1]
            self.consumed = 0
            if self.verbose:
                print(self.nitems_written(0),self.active_buf.shape,get_time()-self.initial)
            return True

        def work(self,input_items,output_items):
            nout = len(output_items[0])
            if self.active_buf is None:
                if not self.prep_next():
                    return 0
                self.down_time_end = get_time()
                if self.verbose and self.down_time_start is not None:
                    print("down time:",self.down_time_end-self.down_time_start)
            available = self.active_buf.shape[1]-self.consumed
            xfer = min(available,nout)
            for idx in range(len(self.modes)):
                output_items[idx][:xfer] = self.active_buf[idx,self.consumed:self.consumed+xfer]
            self.consumed += xfer
            if self.consumed == self.active_buf.shape[1]:
                self.active_buf = None
                self.down_time_start = get_time()
            return xfer

    class analog_path(gr.hier_block2):
        def __init__(self,data_path:AnalogPath,step_size:int=None):
            if data_path.output_fs is None or data_path.input_fs is None:
                raise ValueError("in/out sample rate are not defined")
            ## bw/in_fs/out_fs is the two-sided rate
            gr.hier_block2.__init__(self,
                "Analog Path",
                gr.io_signature(1,1,gr.sizeof_float),
                gr.io_signature(1,1,gr.sizeof_float))
            self.sample_rate_out = data_path.output_fs ##### should be the waveforms rate
            self.sample_rate_in = data_path.input_fs   ##### either the output rate or double-sided bw
            self.modulation = analogs_available_mods.get_type(data_path.waveform)
            self.dp_type = analogs_path_types.get_type(data_path.dp_type)
            if self.dp_type == analogs_path_types.tone:
                self.bandwidth = 1e3
            else:
                if data_path.bw is not None and data_path.bw < data_path.input_fs:
                    self.bandwidth = data_path.bw
                else:
                    ## Really shouldn't get here I think
                    self.bandwidth = data_path.input_fs ### assuming double-sided bw
            self.center_freq = data_path.fc_shift if data_path.fc_shift > 0. else 0 # could be negative
            if self.center_freq > self.sample_rate_out/2:
                print("Fc is out of bandwidth, assuming alias is ok")
            if self.bandwidth/2 + self.center_freq > self.sample_rate_out/2:
                print("This config will alias")
            if self.modulation not in \
                    [analogs_available_mods.dsb,analogs_available_mods.dsbsc,
                    analogs_available_mods.tone,analogs_available_mods.off,
                    analogs_available_mods.white_noise]:
                raise ValueError("Invalid modulation for analog path")
            self.gain = 1.0 if data_path.gain is None else data_path.gain
            off = analogs_available_mods.off
            tone = analogs_available_mods.tone
            if self.modulation in [off, tone]:
                self.snk = blocks.null_sink(gr.sizeof_float)
                self.src = blocks.null_source(gr.sizeof_float) if self.modulation == off \
                    else analog.sig_source_f(self.sample_rate_out, analog.GR_SIN_WAVE, self.center_freq,
                                            self.gain,0,0)
                if self.modulation == tone and step_size is not None:
                    self.src.set_min_output_buffer(step_size)
                self.connect((self,0),(self.snk,0))
                self.connect((self.src,0),(self,0))
            else:
                resample = True
                if self.sample_rate_out == self.sample_rate_in:
                    resample = False
                shift = False
                if self.center_freq > 0:
                    shift = True
                alias_handing = False #### this is limited to when the above prints don't trigger
                if self.bandwidth <= self.sample_rate_in: ### sanity more than anything
                    alias_handing = True
                add_tone = False
                if self.modulation == analogs_available_mods.dsb:
                    add_tone = True

                self.bw = self.bandwidth/self.sample_rate_in

                self.resamp = None
                self.lp = None
                self.shifter = None
                self.multiplier = None
                self.adder = None
                if alias_handing and self.bw < 0.8:
                    #### bw is less than the input sample rate, filter just as a sanity
                    xcut = min(self.bw/2,0.45)
                    xband = 0.05
                    self.lp_taps = firdes.low_pass(1,       ## gain
                                                1,       ### fs
                                                xcut,    #cut off
                                                xband,   #xband
                                                window.WIN_KAISER, #window
                                                6.76     #beta
                                                )
                    # print("lp",1,self.bw,xcut,xband)
                    self.lp = filter.interp_fir_filter_fff(1,self.lp_taps)
                    if step_size is not None:
                        self.lp.set_min_output_buffer(step_size)
                else:
                    ####hmmm... bw > fs_in????
                    xband = 0.1 # 0.5-0.4
                    xcut = 0.5-xband/2
                    self.lp_taps = firdes.low_pass(1,       ## gain
                                                1,       ### fs
                                                xcut,    #cut off
                                                xband,   #xband
                                                window.WIN_KAISER, #window
                                                6.76     #beta
                                                )
                    self.lp = filter.interp_fir_filter_fff(1,self.lp_taps)
                    if step_size is not None:
                        self.lp.set_min_output_buffer(step_size)
                if resample:
                    # frac = Fraction("{0:.8e}".format(self.sample_rate_out/self.sample_rate_in))
                    # frac = frac.limit_denominator(100)
                    rate = self.sample_rate_out/self.sample_rate_in
                    # print("resamp:",rate,end=' ')
                    import math
                    div = math.gcd(int(self.sample_rate_in),int(self.sample_rate_out))
                    interp = self.sample_rate_out/div
                    decim = self.sample_rate_in/div
                    xw = 0.1 if rate >= 1.0 else 0.1*rate
                    mxb = 0.45 if rate >= 1.0 else (rate-xw)/2
                    # print(decim,interp,mxb,xw,window.WIN_KAISER,7.0)
                    self.rational_taps = np.array(
                        firdes.low_pass(decim,interp,mxb,xw,window.WIN_KAISER,7.0))
                    # print("apdrrs:",interp,decim)
                    self.resamp = filter.rational_resampler_fff(
                        interpolation=int(interp),
                        decimation=int(decim),
                        taps=self.rational_taps.tolist(),
                        fractional_bw=0) # one sided
                    if step_size is not None:
                        self.resamp.set_min_output_buffer(step_size)
                if shift or add_tone:
                    self.shifter = analog.sig_source_f(self.sample_rate_out, analog.GR_SIN_WAVE, self.center_freq, self.gain * (0.5**0.5 if add_tone else 1.0), 0, 0)
                    if step_size is not None:
                        self.shifter.set_min_output_buffer(step_size)
                    if shift:
                        self.multiplier = blocks.multiply_vff(1)
                        if step_size is not None:
                            self.multiplier.set_min_output_buffer(step_size)
                    elif self.gain != 2.0**0.5:
                        self.multiplier = blocks.multiply_const_ff(self.gain * (0.5**0.5 if add_tone else 1.0))
                        if step_size is not None:
                            self.multiplier.set_min_output_buffer(step_size)
                elif self.gain != 1.0:
                    self.multiplier = blocks.multiply_const_ff(self.gain)
                    if step_size is not None:
                        self.multiplier.set_min_output_buffer(step_size)
                if add_tone:
                    self.adder = blocks.add_vff(1)
                    if step_size is not None:
                        self.adder.set_min_output_buffer(step_size)

                self.block_list = [self,]                                       # sig rate in_fs; bw[0,in_fs]
                if self.lp is not None:
                    self.block_list.append(self.lp)                             # sig rate in_fs; bw[0,path.bw]
                if self.resamp is not None:
                    self.block_list.append(self.resamp)                         # sig rate out_fs; bw[0,path.bw]
                if self.shifter is not None:
                    ### tone exists as (a*tone | a/2*tone (add_tone))
                    if self.adder is not None:
                        ### tone exists as a/2*tone
                        if self.multiplier is not None:
                            self.block_list.append(self.multiplier)             # sig rate out_fs; bw[0,path.bw]; sig*?
                            self.connect((self.shifter,0),(self.multiplier,1))  # sig rate out_fs; bw[0,path.bw]; a/2*sig*tone
                        self.block_list.append(self.adder)                      # sig rate out_fs; bw[0,path.bw]; a/2*sig*tone+?
                        self.connect((self.shifter,0),(self.adder,1))           # sig rate out_fs; bw[0,path.bw]; a*tone*(sig/2+1/2)
                    elif self.multiplier is not None:
                        ### tone exists as a*tone
                        self.block_list.append(self.multiplier)                 # sig rate out_fs; bw[0,path.bw]; sig * ?
                        self.connect((self.shifter,0),(self.multiplier,1))      # sig rate out_fs; bw[0,path.bw]; a*tone*sig
                    else:
                        ### tone exists as a*tone
                        raise RuntimeError("major logic error")
                elif self.multiplier is not None:
                    self.block_list.append(self.multiplier)                     # sig rate out_fs; bw[0,path.bw]; a*sig
                self.block_list.append(self)
                for idx in range(len(self.block_list)-1):
                    self.connect((self.block_list[idx],0),(self.block_list[idx+1],0))

        def set_min_output_buffer(self,*args,**kwargs):
            pass



    class digital_path(gr.hier_block2):
        def __init__(self,data_path:DigitalPath,step_size=None):
            ## bw is the two-sided bw
            if data_path.output_fs is None or data_path.input_fs is None:
                raise ValueError("in/out sample rate are not defined")
            gr.hier_block2.__init__(self,
                "Digital Path",
                gr.io_signature(1,1,gr.sizeof_char),
                gr.io_signature(1,1,gr.sizeof_float))
            
            self.sample_rate_out = data_path.output_fs
            self.sample_rate_in = data_path.input_fs
            self.rate = data_path.rate
            self.modulation = analogs_available_mods.get_type(data_path.waveform)
            if self.modulation not in \
                    [analogs_available_mods.manchester,analogs_available_mods.bpsk]:
                raise ValueError("Invalid modulation for digital path")
            if data_path.bw is None:
                if self.modulation == analogs_available_mods.manchester:
                    self.bandwidth = 4*self.rate
                else:
                    self.bandwidth = 2*self.rate
            else:
                self.bandwidth = data_path.bw
            self.center_freq = data_path.fc_shift if data_path.fc_shift > 0. else 0
            self.dp_type = data_path.dp_type
            if self.center_freq > self.sample_rate_out/2:
                print("Fc is out of bandwidth, assuming alias is ok for data_path")
            if self.bandwidth/2 + self.center_freq > self.sample_rate_out/2:
                print("This data_path will alias")

            self.shifter = None

            self.gain = 1.0 if data_path.gain is None else data_path.gain
            self.block_list = [self,]
            if self.modulation == analogs_available_mods.manchester:
                self.mapper = digital.map_bb([1,2])
                if step_size is not None:
                    self.mapper.set_min_output_buffer(step_size)
                self.block_list.append(self.mapper)
                self.bitter = blocks.unpack_k_bits_bb(2)
                if step_size is not None:
                    self.bitter.set_min_output_buffer(step_size)
                self.block_list.append(self.bitter)
                self.symbler = digital.chunks_to_symbols_bf([-1,1],1)
                if step_size is not None:
                    self.symbler.set_min_output_buffer(step_size)
                self.block_list.append(self.symbler)
                # frac = Fraction("{0:.8e}".format((self.sample_rate_out)/(2*self.rate)))
                # interp = frac.numerator
                # decim = frac.denominator
                # if decim > 1:
                #     interp = int(interp/decim)
                rate = self.sample_rate_out/(2*self.sample_rate_in) ## resamp rate -> self.rate == baud rate
                import math
                in_rate = 2*self.sample_rate_in
                out_rate = self.sample_rate_out
                while in_rate != int(in_rate):
                    in_rate *= 10
                while out_rate != int(out_rate):
                    out_rate *= 10
                div = math.gcd(int(in_rate),int(out_rate))
                interp = int(out_rate/div)
                decim = int(in_rate/div)
                self.taps = firdes.root_raised_cosine(1,interp,1.0,1.0,min(interp*11,160*11))
                self.interp = filter.interp_fir_filter_fff(interp,self.taps)
                if step_size is not None:
                    self.interp.set_min_output_buffer(step_size)
                self.block_list.append(self.interp)
            elif self.modulation == analogs_available_mods.bpsk:
                self.symbler = digital.chunks_to_symbols_bf([-1,1],1)
                if step_size is not None:
                    self.symbler.set_min_output_buffer(step_size)
                self.block_list.append(self.symbler)
                self.taps_rrc = firdes.root_raised_cosine(1,4,1,0.35,45)
                self.rrc = filter.interp_fir_filter_fff(4,self.taps_rrc)
                if step_size is not None:
                    self.rrc.set_min_output_buffer(step_size)
                self.block_list.append(self.rrc)
                # frac = Fraction("{0:.8e}".format((self.sample_rate_out)/(4*self.rate)))
                # interp = frac.numerator
                # decim = frac.denominato
                rate = self.sample_rate_out/(self.sample_rate_in) ## resamp rate -> self.rate == baud rate
                import math
                in_rate = self.sample_rate_in
                out_rate = self.sample_rate_out
                while in_rate != int(in_rate):
                    in_rate *= 10
                while out_rate != int(out_rate):
                    out_rate *= 10
                div = math.gcd(int(in_rate),int(out_rate))
                interp = int(out_rate/div)
                decim = int(in_rate/div)
                xw = 0.1 if rate >= 1.0 else 0.1*rate
                mxb = 0.45 if rate >= 1.0 else (rate-xw)/2
                # self.rational_taps = np.array(
                #     firdes.low_pass(decim,interp,mxb,xw,window.WIN_KAISER,7.0))
                self.rat_taps = np.array(
                    firdes.low_pass(decim,interp,mxb,xw,window.WIN_HAMMING))
                # print("dpdrrs:",interp,decim)
                self.interp = filter.rational_resampler_fff(
                    interpolation=interp,
                    decimation=decim,
                    taps=self.rat_taps,
                    fractional_bw=0) # one sided
                if step_size is not None:
                    self.interp.set_min_output_buffer(step_size)
                self.block_list.append(self.interp)
            else:
                self.block_list.append((blocks.char_to_float(1,1.0)))
                if step_size is not None:
                    self.block_list[-1].set_min_output_buffer(step_size)
            if self.center_freq > 0.:
                self.shifter = analog.sig_source_f(self.sample_rate_out, analog.GR_SIN_WAVE,
                                                self.center_freq,self.gain, 0, 0)
                if step_size is not None:
                    self.shifter.set_min_output_buffer(step_size)
                self.block_list.append((blocks.multiply_vff(1)))
                if step_size is not None:
                    self.block_list[-1].set_min_output_buffer(step_size)
                self.connect((self.shifter,0),(self.block_list[-1],1))
            self.block_list.append(self)
            for idx in range(len(self.block_list)-1):
                self.connect((self.block_list[idx],0),(self.block_list[idx+1],0))

        def set_min_output_buffer(self,*args,**kwargs):
            pass

else:
    class analog_path:
        pass
    class digital_path:
        pass
    class analog_src:
        pass

