// signal labels
#ifndef __LABELS_HH__
#define __LABELS_HH__

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modulation.hh"

class labels
{
  public:
    labels(std::string _filename,
           std::string _prefix="GT ENG",
           std::string _signal="GT SIG 0000001",
           std::string _source="GT SRC 0000001");
    ~labels();

    /// append energy report to file and update internal counters
    void append(double tic, double dur, double fc, double bw, std::string meta="");

    // open file and write header
    void start_reports();

    // set modulation type based on liquid-dsp enumeration
    void set_modulation(modulation_scheme _ms)
        { modulation.assign(signal_modulation_list[ signal_modulation_map_ms_to_index(_ms) ].name_label);
          modulation_origin.assign(signal_modulation_list[ signal_modulation_map_ms_to_index(_ms) ].name_label);
          modulation_family.assign(signal_modulation_list[ signal_modulation_map_ms_to_index(_ms) ].family_label); }
    void set_modulation(fsk_scheme _ms)
        { modulation.assign(signal_modulation_list[ signal_modulation_map_msf_to_index(_ms) ].name_label);
          modulation_origin.assign(signal_modulation_list[ signal_modulation_map_msf_to_index(_ms) ].name_label);
          modulation_family.assign(signal_modulation_list[ signal_modulation_map_msf_to_index(_ms) ].family_label); }
    void set_modulation(analog_scheme _ms)
        { modulation.assign(signal_modulation_list[ signal_modulation_map_msa_to_index(_ms) ].name_label);
          modulation_origin.assign(signal_modulation_list[ signal_modulation_map_msa_to_index(_ms) ].name_label);
          modulation_family.assign(signal_modulation_list[ signal_modulation_map_msa_to_index(_ms) ].family_label); }
    void set_modulation(noise_scheme _ms)
        { modulation.assign(signal_modulation_list[ signal_modulation_map_msn_to_index(_ms) ].name_label);
          modulation_origin.assign(signal_modulation_list[ signal_modulation_map_msn_to_index(_ms) ].name_label);
          modulation_family.assign(signal_modulation_list[ signal_modulation_map_msn_to_index(_ms) ].family_label); }
    void set_modulation(std::string _modulation)
        { modulation.assign(signal_modulation_list[ signal_modulation_map_label_to_index(_modulation) ].name_label);
          modulation_origin.assign(signal_modulation_list[ signal_modulation_map_label_to_index(_modulation) ].name_label);
          modulation_family.assign(signal_modulation_list[ signal_modulation_map_label_to_index(_modulation) ].family_label); }

    void finalize(){ json_close(); }

    void cache_to_misc(std::string);

  protected:
    std::string  filename;  ///< name of file to write
    std::string  prefix;    ///< prefix for energy reports
    std::string  signal;    ///< signal ID
    std::string  source;    ///< source ID
    std::string  misc;


    unsigned int count;     ///< number of energy reports written
    double       start;     ///< start time
    double       stop;      ///< stop time
    double       freq_lo;   ///< lower edge of frequency band
    double       freq_hi;   ///< lower edge of frequency band

    /// file handle
    FILE * fid;

    // open file and close, then reopen (flagging other processes)
    void json_init();

    // finalize and close file
    void json_close();

  public:
    // energy/signal characteristics
    std::string protocol;
    std::string modality;
    std::string activity_type;
    std::string modulation;
    std::string modulation_origin;
    std::string modulation_family;
    std::string modulation_src;
    std::string device_origin;
    double      eng_bw;    ///< bw of energy burst should be constant
};

/******************************************************************************************/
// Preserving old functionality until I update all scripts
void append_json(FILE * fid, double tic, float dur, float fc, float bw, unsigned int index);
// finalize .json
void finalize_json_verbose(FILE * fid, double start, double stop, float fc, float bw, unsigned int count,
                        std::string activty, std::string protocol, std::string modulation,
                        std::string modality, std::string device);

#endif /* __LABELS_HH__ */

