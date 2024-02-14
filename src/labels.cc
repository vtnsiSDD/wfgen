
#include <iostream>
#include "labels.hh"

labels::labels(std::string _filename,
               std::string _prefix,
               std::string _signal,
               std::string _source) :
    filename(_filename),
    prefix(_prefix),
    signal(_signal),
    source(_source),
    misc("    \"misc\": {\n"),
    count(0), start(-1), stop(-1), freq_lo(-1), freq_hi(-1),
    protocol("unknown"),
    modality("unknown"),
    activity_type("lowprob_anomaly"),
    modulation("unknown"),
    modulation_origin("unknown"),
    modulation_src("nil"),
    device_origin("custom label"),
    eng_bw(0)
{
    json_init();
}

labels::~labels()
{
    if(fid != NULL)
        json_close();
}

void labels::json_init()
{
    fid = fopen(filename.c_str(), "w");
    if (!fid)
        throw std::runtime_error("could not open " + filename + " for writing");
    fclose(fid);
    fid = fopen(filename.c_str(), "a");
    fprintf(fid,"{\n");
    // fprintf(fid,"    \"reports\": [\n");
}


void labels::json_close()
{
    // derive global center frequency and bandwidth
    double fc = 0.5*(freq_hi + freq_lo);
    double bw =      freq_hi - freq_lo;

    if (count > 0){
        fprintf(fid,"        {\n");
        fprintf(fid,"            \"report_type\": \"signal\",\n");
        fprintf(fid,"            \"instance_name\": \"%s\",\n", signal.c_str());
        fprintf(fid,"            \"activity_type\": \"%s\",\n", activity_type.c_str());
        fprintf(fid,"            \"reference_time\": %.6f,\n", (stop-start)/2+stop);
        fprintf(fid,"            \"reference_freq\": %.6f,\n", fc/1000000);
        fprintf(fid,"            \"protocol\": \"%s\",\n",protocol.c_str());
        fprintf(fid,"            \"modulation\": \"%s\",\n", modulation.c_str());
        fprintf(fid,"            \"mod_name_label\": \"%s\",\n", modulation_origin.c_str());
        fprintf(fid,"            \"mod_family_label\": \"%s\",\n", modulation_family.c_str());
        fprintf(fid,"            \"mod_src_name\": \"%s\",\n", modulation_src.c_str());
        fprintf(fid,"            \"modality\": \"%s\",\n", modality.c_str());
        fprintf(fid,"            \"modification\": \"no_modification\",\n");
        fprintf(fid,"            \"freq_lo\": %.6f,\n", fc/1000000 - 0.5*bw/1000000);
        fprintf(fid,"            \"freq_hi\": %.6f,\n", fc/1000000 + 0.5*bw/1000000);
        fprintf(fid,"            \"bw\": %.6f,\n", bw/1000000);
        fprintf(fid,"            \"energy_bw\": %.6f,\n",eng_bw);
        fprintf(fid,"            \"time_start\": %.6f,\n", start);
        fprintf(fid,"            \"time_stop\": %.6f,\n", stop);
        fprintf(fid,"            \"duration\": %.6f,\n", stop-start);
        fprintf(fid,"            \"energy_set\": [\n");
        for (unsigned int i=0; i<count; i++)
            fprintf(fid,"                \"%s%06u\"%s\n", prefix.c_str(), i, i==count-1 ? "" : ",");
        fprintf(fid,"            ],\n");
        fprintf(fid,"            \"rx_center_freq\": {\n");
        fprintf(fid,"                \"rx1\": %.6f\n", fc/1000000);
        fprintf(fid,"            },\n");
        fprintf(fid,"            \"rx_sample_rate\": {\n");
        fprintf(fid,"                \"rx1\": 100\n");
        fprintf(fid,"            },\n");
        fprintf(fid,"            \"rx_input_snr\": {\n");
        fprintf(fid,"                \"rx1\": -100 \n");
        fprintf(fid,"            }\n");
        fprintf(fid,"        },\n");
        fprintf(fid,"        {\n");
        fprintf(fid,"            \"report_type\": \"source\",\n");
        fprintf(fid,"            \"instance_name\": \"%s\",\n", source.c_str());
        fprintf(fid,"            \"signal_set\": [\n");
        fprintf(fid,"                \"%s\"\n", signal.c_str());
        fprintf(fid,"            ],\n");
        fprintf(fid,"            \"device_origin\": \"%s\"\n",device_origin.c_str());
        fprintf(fid,"        }\n");
        if (misc.length() > 15){
            fprintf(fid,"    ],\n");
            fprintf(fid,"%s",misc.c_str());
            fprintf(fid,"    }\n");
        }
        else{
            fprintf(fid,"    ]\n");
        }
        fprintf(fid,"}\n");
    }
    fclose(fid);
    fid = NULL;
    std::cout << ".json log written to " << filename << std::endl;
}

void labels::start_reports()
{
    fprintf(fid,"    \"reports\": [\n");
}

void labels::append(double tic, double dur, double fc, double bw, std::string meta)
{
    // update internal counters
    if (start < 0) {
        start = tic;
    }
    stop = tic + dur;
    if (freq_lo < 0)
        freq_lo = fc - 0.5*bw;
    if (freq_hi < 0)
        freq_hi = fc + 0.5*bw;
    freq_lo = std::min(freq_lo, fc - 0.5*bw);
    freq_hi = std::max(freq_hi, fc + 0.5*bw);

    fprintf(fid,"        {\n");
    fprintf(fid,"            \"report_type\": \"energy\",\n");
    fprintf(fid,"            \"instance_name\": \"%s%.6u\",\n", prefix.c_str(), count);
    fprintf(fid,"            \"freq_lo\": %.6f,\n", (fc-0.5*bw)*1e-6f); // MHz
    fprintf(fid,"            \"freq_hi\": %.6f,\n", (fc+0.5*bw)*1e-6f); // MHz
    fprintf(fid,"            \"bw\":      %.6f,\n", (       bw)*1e-6f); // MHz
    fprintf(fid,"            \"time_start\": %.9f,\n", tic);
    fprintf(fid,"            \"time_stop\":  %.9f,\n", tic + dur);
    fprintf(fid,"            \"duration\":   %.16f,\n", dur);
    fprintf(fid,"            \"modulation\": \"%s\",\n", modulation.c_str());
    fprintf(fid,"            \"meta\": \"%s\"\n", meta.c_str());
    fprintf(fid,"        },\n");

    // update internal counter
    count++;
}


void labels::cache_to_misc(std::string input){
    misc += input;
}
