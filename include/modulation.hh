// modulation mapping between sponsor and liquid-dsp
#ifndef __MODULATIONS_HH__
#define __MODULATIONS_HH__

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liquid.h"
#include "fskmodems.hh"
#include "afmodem.hh"
#include "noisemodem.hh"

struct signal_modulation_s {
    const char * name_label;
    const char * family_label;
    modulation_scheme scheme;
    fsk_scheme        f_scheme;
    analog_scheme     a_scheme;
    noise_scheme      n_scheme;
};

#define SIGNAL_MODULATION_LIST_LEN (80)

// list of modulation/mapping
extern const struct signal_modulation_s signal_modulation_list[SIGNAL_MODULATION_LIST_LEN];
#ifdef __cplusplus
// get index of above map given label name as input
unsigned int signal_modulation_map_label_to_index(std::string _name_label);
#endif
// get index of above map given modulation type as input
unsigned int signal_modulation_map_ms_to_index(modulation_scheme _ms);

// get index of above map given modulation type as input
unsigned int signal_modulation_map_msf_to_index(fsk_scheme _ms);

// get index of above map given modulation type as input
unsigned int signal_modulation_map_msa_to_index(analog_scheme _ms);

// get index of above map given modulation type as input
unsigned int signal_modulation_map_msn_to_index(noise_scheme _ms);

#endif /* __MODULATIONS_HH__ */

