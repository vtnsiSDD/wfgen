from .linmod import psk2, psk4, psk8, psk16, psk32, psk64,\
                    psk128, psk256, dpsk2, dpsk4, dpsk8, \
                    dpsk16, dpsk32, dpsk64, dpsk128, dpsk256, \
                    ask2, ask4, ask8, ask16, ask32, ask64, \
                    ask128, ask256, qam4, qam8, qam16, qam32, \
                    qam64, qam128, qam256, apsk4, apsk8, \
                    apsk16, apsk32, apsk64, apsk128, apsk256, \
                    bpsk, qpsk, ook, sqam32, sqam128, V29, \
                    arb16opt, arb32opt, arb64opt, arb128opt, \
                    arb256opt, arb64vt, pi4dqpsk, noise


from .fskmod import fsk2, fsk4, fsk8, fsk16, fsk32, fsk64, \
        fsk128, fsk256, cpfsk2, cpfsk4, cpfsk8, cpfsk16, \
        cpfsk32, cpfsk64, cpfsk128, cpfsk256, msk, scpfsk4, \
        scpfsk8, scpfsk16, scpfsk32, scpfsk64, scpfsk128, scpfsk256, gmsk, \
        gcpfsk4, gcpfsk8, gcpfsk16, gcpfsk32, gcpfsk64, gcpfsk128, gcpfsk256

from .afmod import am_constant, am_square, am_triangle, am_sawtooth, \
        am_sinusoid, am_wav_file, am_rand_uni, am_rand_gauss, fm_constant, fm_square, \
        fm_triangle, fm_sawtooth, fm_sinusoid, fm_wav_file, fm_rand_uni, fm_rand_gauss

from .tones import tone,multitone,emanation_exponential

from .ofdm import ofdm,wifi_ag,wifi_ac,lte

from ..utils import have_pygr

if have_pygr():
    from .analogs import analog_path,digital_path,analog_src


#######################################################


from .linmod import available_mods as linmod_available_mods
# from .linmod import available_options as linmod_available_options
# from .linmod import available_options_hopper as linmod_available_hopper_options
from .fskmod import available_mods as fskmod_available_mods
# from .fskmod import available_options as fskmod_available_options
# from .fskmod import available_options_hopper as fskmod_available_hopper_options
from .afmod import available_mods as afmod_available_mods
from .tones import available_mods as tones_available_mods
# from .tones import available_options as tones_available_options
from .ofdm import available_mods as ofdm_available_mods
from .ofdm import available_subcarrier_mods as ofdm_submods

from ._profile import get_all_profile_names,get_replay_profile_names,\
                    get_defined_profile_names, extract_profile_by_name,\
                    sanity_check_profiles,\
                    get_base_options,get_base_flags,resolve_band_freq_options,\
                    resolve_hopper_dwell_squelch_period_options,\
                    get_traceback_profiles


sanity_check_profiles()

del sanity_check_profiles


