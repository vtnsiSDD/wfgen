
#include <iostream>
#include "wbofdmgen.hh"

wbofdmgen::wbofdmgen(unsigned int _nfft,
                     unsigned int _cplen,
                     unsigned int _ncarrier,
                     unsigned int _ms) :
    nfft(_nfft), cplen(_cplen), ncar(_ncarrier), ms(_ms), gain(new float[nfft])
{
    // // TODO: enable cyclic prefix
    // cplen = 0;

    // allocate memory for transform(s): repeat per thread
    for (auto i=0U; i<OMP_THREADS; i++) {
        buf_freq[i] = (std::complex<float>*) fftwf_malloc(sizeof(fftwf_complex)*nfft);
        buf_time[i] = (std::complex<float>*) fftwf_malloc(sizeof(fftwf_complex)*(nfft));
        fft[i] = fftwf_plan_dft_1d(nfft,(fftwf_complex*)buf_freq[i],(fftwf_complex*)buf_time[i],
                                     FFTW_BACKWARD, FFTW_ESTIMATE);
        modem[i] = modemcf_create(static_cast<modulation_scheme>(ms));
    }

    // compute subcarrier gains
    unsigned int i0 = (ncar/2);
    unsigned int i1 = (unsigned int)(((-((int)ncar)/2)) + (int)nfft);
    auto      scale = 0.0f;
    gain[0] = ncar % 2 ? 0.f : 1.f;
    for (auto i=1U; i<nfft; i++) {
        gain[i] = (i > i0 && i < i1) ? 0.0f : 1.0f;
        scale += gain[i]*gain[i];
    }

    // adjust scale
    scale = 1.0f / sqrtf(scale);
    for (auto i=0U; i<nfft; i++)
        gain[i] *= scale;
}

wbofdmgen::~wbofdmgen()
{
    // repeat per thread
    for (auto i=0U; i<OMP_THREADS; i++) {
        fftwf_free(buf_freq[i]);
        fftwf_free(buf_time[i]);
        fftwf_destroy_plan(fft[i]);
        modemcf_destroy(modem[i]);
    }
    delete [] gain;
}

void wbofdmgen::generate(std::complex<float> * _buf, unsigned int symbols)
{
    unsigned int i;
    omp_set_num_threads(OMP_THREADS);
#pragma omp parallel for private(i) schedule(static)
    for (i=0U; i<symbols; i++)
    {
        int id = omp_get_thread_num();
        //printf("omp_thread_id: %d\n", id);

        // fill buffer with pseudo-random data symbols
        // float phi = randf()*2*M_PI;
        // float dphi = 1e-2f*randnf();
        unsigned int sym;
        for (auto j=0U; j<nfft; j++) {
            // float theta = phi + j*j*dphi;
            // buf_freq[id][j] = gain[j] * std::polar(1.0f, theta);
            sym = modemcf_gen_rand_sym(modem[id]);
            modemcf_modulate(modem[id],sym,&buf_freq[id][j]);

            buf_freq[id][j] = gain[j] * buf_freq[id][j];
        }

        // run transform to get time-domain samples
        fftwf_execute(fft[id]);

        // copy to output buffer
        // TODO: copy cyclic prefix as well
        memmove(_buf + (nfft+cplen)*i,
                buf_time[id], nfft*sizeof(std::complex<float>));
        memmove(_buf + (nfft+cplen)*i + nfft,
                buf_time[id], cplen*sizeof(std::complex<float>));
    }
}

