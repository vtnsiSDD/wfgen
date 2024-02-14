

from .utils import encoder,decoder,MultiSocket,have_pygr
from . import server,client,utils,profiles
from .launcher import launch
from .c_logger import logger_client,fake_log,logger as c_logger

__version__ = "0.1.0"

BaseAudio = None
import os
if 'wg_preload_audio' in os.environ and os.environ["wg_preload_audio"]:
    if BaseAudio is None:
        print("Preloading audio into BaseAudio")
        BaseAudio = profiles.analogs.AudioCombiner(profiles.analogs.get_audio_files())
        print("Preloading complete")
del os

if have_pygr():
    from .utils import gen_pmt_header,parse_pmt_header