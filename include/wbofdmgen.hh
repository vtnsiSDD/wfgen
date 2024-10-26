// wide-band OFDM signal generation
#ifndef __WBOFDMGEN_HH__
#define __WBOFDMGEN_HH__

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
#include <complex>
#else
#include <complex.h>
#endif
#include <fftw3.h>
#include "liquid.h"

#include <omp.h>
#define OMP_THREADS (16)

#ifdef __cplusplus
// inhereted classes
class wbofdmgen
{
  public:
    wbofdmgen(unsigned int _nfft=4800,
              unsigned int _cplen=20,
              unsigned int _ncarrier=3840,
              unsigned int _ms=LIQUID_MODEM_QPSK);
    ~wbofdmgen();

    // get expected output buffer length
    unsigned int get_buf_len(unsigned int symbols) const {
        return (nfft+cplen)*symbols; }

    //
    void generate(std::complex<float> * _buf, unsigned int symbols);

  protected:
    unsigned int nfft;      // FFT size
    unsigned int cplen;     // cyclic prefix length
    unsigned int ncar;
    unsigned int ms;
    float *      gain;      // subcarrier gains


    // repeat per thread
    std::complex<float> * buf_time[OMP_THREADS];    // shape: (nfft,)
    std::complex<float> * buf_freq[OMP_THREADS];    // shape: (nfft,)
    fftwf_plan            fft[OMP_THREADS];         //
    modemcf               modem[OMP_THREADS];
};
#else

typedef struct wbofdmgen_s{

} wbofdmgen;

#endif


#endif /* __WBOFDMGEN_HH__ */

