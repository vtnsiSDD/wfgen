
#ifdef __cplusplus
#include <iostream>
#endif
#include "modulation.hh"

const struct signal_modulation_s signal_modulation_list[SIGNAL_MODULATION_LIST_LEN] =
{
  // name_label      family_label        scheme
    {"ask",          "ask",              LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"ask4",         "ask",              LIQUID_MODEM_ASK4,      LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"ask8",         "ask",              LIQUID_MODEM_ASK8,      LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"ask16",        "ask",              LIQUID_MODEM_ASK16,     LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"apsk",         "apsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"apsk16",       "apsk",             LIQUID_MODEM_APSK16,    LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"apsk32",       "apsk",             LIQUID_MODEM_APSK32,    LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"apsk64",       "apsk",             LIQUID_MODEM_APSK64,    LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk",          "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk2",         "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK2,      LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk4",         "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK4,      LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk8",         "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK8,      LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk16",        "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK16,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk32",        "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK32,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk64",        "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK64,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk128",       "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK128,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fsk256",       "fsk",              LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_FSK256,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk",        "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk2",       "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK2,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk4",       "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK4,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk8",       "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK8,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk16",      "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK16,   LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk32",      "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK32,   LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk64",      "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK64,   LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk128",     "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK128,  LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"cpfsk256",     "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_CPFSK256,  LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"scpfsk4",      "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK4,      LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"scpfsk8",      "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK8,      LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"scpfsk16",     "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK16,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"scpfsk32",     "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK32,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"scpfsk64",     "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK64,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"scpfsk128",    "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK128,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"scpfsk256",    "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK256,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gcpfsk4",      "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK4,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gcpfsk8",      "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK8,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gcpfsk16",     "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK16,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gcpfsk32",     "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK32,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gcpfsk64",     "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK64,    LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gcpfsk128",    "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK128,   LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gcpfsk256",    "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK256,   LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"psk",          "psk",              LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"bpsk",         "psk",              LIQUID_MODEM_BPSK,      LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"qpsk",         "psk",              LIQUID_MODEM_QPSK,      LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"psk8",         "psk",              LIQUID_MODEM_PSK8,      LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"psk16",        "psk",              LIQUID_MODEM_PSK16,     LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"psk32",        "psk",              LIQUID_MODEM_PSK32,     LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"ook",          "ook",              LIQUID_MODEM_OOK,       LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"am_analog",    "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"am_constant",  "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_CONSTANT,  LIQUID_NOISE_UNKNOWN}, /////// this is a tone effectively
    {"am_square",    "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_SQUARE,    LIQUID_NOISE_UNKNOWN},//// radar?? emanation??
    {"am_triangle",  "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_TRIANGLE,  LIQUID_NOISE_UNKNOWN},//// radar?? emanation??
    {"am_sawtooth",  "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_SAWTOOTH,  LIQUID_NOISE_UNKNOWN},//// radar?? emanation??
    {"am_sinusoid",  "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_SINUSOID,  LIQUID_NOISE_UNKNOWN},//// radar?? emanation??
    {"am_wav_file",  "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_WAV_FILE,  LIQUID_NOISE_UNKNOWN},
    {"am_rand_gauss","am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_RAND_UNI,  LIQUID_NOISE_UNKNOWN},////one sided noise double side band
    {"am_rand_uni",  "am_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_RAND_GAUSS,LIQUID_NOISE_UNKNOWN},
    {"msk",          "cpfsk",            LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_MSK2,      LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"gmsk",         "gfsk",             LIQUID_MODEM_UNKNOWN,   LIQUID_MODEM_GMSK2,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fm_analog",    "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"fm_constant",  "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_CONSTANT,  LIQUID_NOISE_UNKNOWN}, /////// this is a tone effectively
    {"fm_square",    "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_SQUARE,    LIQUID_NOISE_UNKNOWN},
    {"fm_triangle",  "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_TRIANGLE,  LIQUID_NOISE_UNKNOWN},
    {"fm_sawtooth",  "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_SAWTOOTH,  LIQUID_NOISE_UNKNOWN},
    {"fm_sinusoid",  "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_SINUSOID,  LIQUID_NOISE_UNKNOWN},
    {"fm_wav_file",  "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_WAV_FILE,  LIQUID_NOISE_UNKNOWN},
    {"fm_rand_gauss","fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_RAND_UNI,  LIQUID_NOISE_UNKNOWN},
    {"fm_rand_uni",  "fm_analog",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_RAND_GAUSS,LIQUID_NOISE_UNKNOWN},
    {"ppm",          "pulse_mod",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_PPM,       LIQUID_NOISE_UNKNOWN},
    {"pwm",          "pulse_mod",        LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_AM_PWM,       LIQUID_NOISE_UNKNOWN},
    {"chirp",        "chirp",            LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_FM_CHIRP,     LIQUID_NOISE_UNKNOWN},
    {"qam",          "qam",              LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"qam8",         "qam",              LIQUID_MODEM_QAM8,      LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"qam16",        "qam",              LIQUID_MODEM_QAM16,     LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"qam32",        "qam",              LIQUID_MODEM_QAM32,     LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"qam64",        "qam",              LIQUID_MODEM_QAM64,     LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"qam128",       "qam",              LIQUID_MODEM_QAM128,    LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"qam256",       "qam",              LIQUID_MODEM_QAM256,    LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
    {"noise",        "noise",            LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_AWGN},
    {"awgn",         "noise",            LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_AWGN},
    {"",             "unknown",          LIQUID_MODEM_UNKNOWN,   LIQUID_FSK_UNKNOWN,     LIQUID_ANALOG_UNKNOWN,      LIQUID_NOISE_UNKNOWN},
};

#ifdef __cplusplus
// get index of above map given label name as input
unsigned int signal_modulation_map_label_to_index(std::string _name_label)
{
    for (uint8_t i=0U; i<SIGNAL_MODULATION_LIST_LEN; i++) {
        if (strcmp(_name_label.c_str(), signal_modulation_list[i].name_label)==0)
            return i;
    }
    return SIGNAL_MODULATION_LIST_LEN-1; // unknown
}
#endif

// get index of above map given modulation type as input
unsigned int signal_modulation_map_ms_to_index(modulation_scheme _ms)
{
    if(_ms == LIQUID_MODEM_UNKNOWN) return SIGNAL_MODULATION_LIST_LEN-1;//unknown
    for (uint8_t i=0U; i<SIGNAL_MODULATION_LIST_LEN; i++) {
        if (_ms == signal_modulation_list[i].scheme)
            return i;
    }
    return SIGNAL_MODULATION_LIST_LEN-1; // unknown
}

// get index of above map given modulation type as input
unsigned int signal_modulation_map_msf_to_index(fsk_scheme _ms)
{
    if(_ms == LIQUID_FSK_UNKNOWN) return SIGNAL_MODULATION_LIST_LEN-1;//unknown
    for (uint8_t i=0U; i<SIGNAL_MODULATION_LIST_LEN; i++) {
        if (_ms == signal_modulation_list[i].f_scheme)
            return i;
    }
    return SIGNAL_MODULATION_LIST_LEN-1; // unknown
}

// get index of above map given modulation type as input
unsigned int signal_modulation_map_msa_to_index(analog_scheme _ms)
{
    if(_ms == LIQUID_ANALOG_UNKNOWN) return SIGNAL_MODULATION_LIST_LEN-1;//unknown
    for (uint8_t i=0U; i<SIGNAL_MODULATION_LIST_LEN; i++) {
        if (_ms == signal_modulation_list[i].a_scheme)
            return i;
    }
    return SIGNAL_MODULATION_LIST_LEN-1; // unknown
}

// get index of above map given modulation type as input
unsigned int signal_modulation_map_msn_to_index(noise_scheme _ms)
{
    if(_ms == LIQUID_NOISE_UNKNOWN) return SIGNAL_MODULATION_LIST_LEN-1;//unknown
    for (uint8_t i=0U; i<SIGNAL_MODULATION_LIST_LEN; i++) {
        if (_ms == signal_modulation_list[i].n_scheme)
            return i;
    }
    return SIGNAL_MODULATION_LIST_LEN-1; // unknown
}

