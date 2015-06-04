/*
    PADnoteParameters.cpp - Parameters for PADnote (PADsynth)

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cmath>

#include "Misc/Util.h"
#include "Misc/Master.h"
#include "Params/PADnoteParameters.h"

PADnoteParameters::PADnoteParameters(FFTwrapper *fft_) : Presets()
{
    setpresettype("Ppadsyth");

    fft=fft_;

    resonance = new Resonance();
    oscilgen = new OscilGen(fft_, resonance);
    oscilgen->ADvsPAD = true;

    FreqEnvelope = new EnvelopeParams(0, 0);
    FreqEnvelope->ASRinit(64, 50, 64, 60);
    FreqLfo = new LFOParams(70, 0, 64, 0, 0, 0, 0, 0);

    AmpEnvelope = new EnvelopeParams(64, 1);
    AmpEnvelope->ADSRinit_dB(0, 40, 127, 25);
    AmpLfo = new LFOParams(80, 0, 64, 0, 0, 0, 0, 1);

    GlobalFilter = new FilterParams(2, 94, 40);
    FilterEnvelope = new EnvelopeParams(0, 1);
    FilterEnvelope->ADSRinit_filter(64, 40, 64, 70, 60, 64);
    FilterLfo = new LFOParams(80, 0, 64, 0, 0, 0, 0, 2);

    for (int i = 0; i < PAD_MAX_SAMPLES; ++i)
        sample[i].smp = NULL;
    newsample.smp = NULL;
    defaults();
}

PADnoteParameters::~PADnoteParameters()
{
    deletesamples();
    delete(oscilgen);
    delete(resonance);
    delete(FreqEnvelope);
    delete(FreqLfo);
    delete(AmpEnvelope);
    delete(AmpLfo);
    delete(GlobalFilter);
    delete(FilterEnvelope);
    delete(FilterLfo);

}

void PADnoteParameters::defaults()
{
    Pmode = 0;
    Php.base.type = 0;
    Php.base.par1 = 80;
    Php.freqmult = 0;
    Php.modulator.par1 = 0;
    Php.modulator.freq = 30;
    Php.width = 127;
    Php.amp.type = 0;
    Php.amp.mode = 0;
    Php.amp.par1 = 80;
    Php.amp.par2 = 64;
    Php.autoscale = true;
    Php.onehalf = 0;

    setPbandwidth(500);
    Pbwscale = 0;

    resonance->defaults();
    oscilgen->defaults();

    Phrpos.type = 0;
    Phrpos.par1 = 64;
    Phrpos.par2 = 64;
    Phrpos.par3 = 0;

    Pquality.samplesize = 3;
    Pquality.basenote = 4;
    Pquality.oct = 3;
    Pquality.smpoct = 2;

    PStereo = 1; // stereo
    // Frequency Global Parameters
    Pfixedfreq = 0;
    PfixedfreqET = 0;
    PDetune = 8192; // zero
    PCoarseDetune = 0;
    PDetuneType = 1;
    FreqEnvelope->defaults();
    FreqLfo->defaults();

    // Amplitude Global Parameters
    PVolume = 90;
    PPanning = 64; // center
    PAmpVelocityScaleFunction = 64;
    AmpEnvelope->defaults();
    AmpLfo->defaults();
    PPunchStrength = 0;
    PPunchTime = 60;
    PPunchStretch = 64;
    PPunchVelocitySensing = 72;

    // Filter Global Parameters
    PFilterVelocityScale = 64;
    PFilterVelocityScaleFunction = 64;
    GlobalFilter->defaults();
    FilterEnvelope->defaults();
    FilterLfo->defaults();
    deletesamples();
}

void PADnoteParameters::deletesample(int n)
{
    if (n < 0 || n >= PAD_MAX_SAMPLES)
        return;
    if (sample[n].smp != NULL)
    {
        delete [] sample[n].smp;
        sample[n].smp = NULL;
    }
    sample[n].size = 0;
    sample[n].basefreq = 440.0;
}

void PADnoteParameters::deletesamples()
{
    for (int i = 0; i < PAD_MAX_SAMPLES; ++i)
        deletesample(i);
}

// Get the harmonic profile (i.e. the frequency distributio of a single harmonic)
float PADnoteParameters::getprofile(float *smp, int size)
{
    for (int i = 0; i < size; ++i)
        smp[i] = 0.0;

    const int supersample = 16;
    float basepar = powf(2.0, ((1.0 - Php.base.par1 / 127.0) * 12.0));
    float freqmult = floorf(powf(2.0, (Php.freqmult / 127.0 * 5.0)) + 0.000001);

    float modfreq = floorf(powf(2.0,(Php.modulator.freq / 127.0 * 5.0)) + 0.000001);
    float modpar1 = powf((Php.modulator.par1 / 127.0), 4.0) * 5.0 / sqrtf(modfreq);
    float amppar1 = powf(2.0, powf((Php.amp.par1 / 127.0), 2.0) * 10.0) - 0.999;
    float amppar2 = (1.0 - Php.amp.par2 / 127.0) * 0.998 + 0.001;
    float width = powf((150.0 / (Php.width + 22.0)), 2.0);

    for (int i = 0; i < size * supersample; ++i)
    {
        bool makezero = false;
        float x = i * 1.0 / (size * (float)supersample);
        float origx = x;
        // do the sizing (width)
        x = (x - 0.5) * width + 0.5;
        if (x < 0.0)
        {
            x = 0.0;
            makezero = true;
        } else {
            if (x >1.0)
            {
                x = 1.0;
                makezero = true;
            }
        }
        // compute the full profile or one half
        switch (Php.onehalf)
        {
        case 1:
            x = x * 0.5 + 0.5;
            break;
        case 2:
            x = x * 0.5;
            break;
        }

        float x_before_freq_mult = x;
        // do the frequency multiplier
        x *= freqmult;
        // do the modulation of the profile
        x += sinf(x_before_freq_mult * 3.1415926 * modfreq) * modpar1;
        x = fmodf(x + 1000.0, 1.0) * 2.0 - 1.0;
        // this is the base function of the profile
        float f;
        switch (Php.base.type)
        {
        case 1:
            f = expf(-(x * x) * basepar);
            if (f < 0.4)
                f = 0.0;
            else
                f = 1.0;
            break;
        case 2:
            f = expf(-(fabsf(x)) * sqrtf(basepar));
            break;
        default:
            f = expf(-(x * x) * basepar);
            break;
        }
        if (makezero)
            f = 0.0;
        float amp = 1.0;
        origx = origx * 2.0 - 1.0;
        // compute the amplitude multiplier
        switch (Php.amp.type)
        {
        case 1:
            amp = expf(-(origx * origx) * 10.0 * amppar1);
            break;
        case 2:
            amp = 0.5 * (1.0 + cosf(3.1415926 * origx * sqrtf(amppar1 * 4.0 + 1.0)));
            break;
        case 3:
            amp = 1.0 / (powf(origx * (amppar1 * 2.0 + 0.8), 14.0) + 1.0);
            break;
        }
        // apply the amplitude multiplier
        float finalsmp = f;
        if (Php.amp.type != 0)
        {
            switch (Php.amp.mode)
            {
            case 0:
                finalsmp = amp * (1.0 - amppar2) + finalsmp * amppar2;
                break;
            case 1:
                finalsmp *= amp * (1.0 - amppar2) + amppar2;
                break;
            case 2:
                finalsmp =
                    finalsmp / (amp + powf(amppar2, 4.0) * 20.0 + 0.0001);
                break;
            case 3:
                finalsmp =
                    amp / (finalsmp + powf(amppar2, 4.0) * 20.0 + 0.0001);
                break;
            }
        }
        smp[i / supersample] += finalsmp / supersample;
    }
    // normalize the profile (make the max. to be equal to 1.0)
    float max = 0.0;
    for (int i = 0; i < size; ++i)
    {
        smp[i] = smp[i];
        if (smp[i] > max)
            max = smp[i];
    }
    if (max < 0.00001)
        max = 1.0;
    for (int i = 0; i < size; ++i)
        smp[i] /= max;

    if (!Php.autoscale)
        return 0.5;
    // compute the estimated perceived bandwidth
    float sum = 0.0;
    int i;
    for (i = 0; i < size / 2 - 2; ++i)
    {
        sum += smp[i] * smp[i] + smp[size - i -1] * smp[size - i - 1];
        if (sum >= 4.0)
            break;
    }
    float result = 1.0 - 2.0 * i / (float)size;
    return result;
}

// Compute the real bandwidth in cents and returns it
// Also, sets the bandwidth parameter
float PADnoteParameters::setPbandwidth(int Pbandwidth)
{
    this->Pbandwidth = Pbandwidth;
    float result = powf(Pbandwidth / 1000.0, 1.1);
    result = powf(10.0, result * 4.0) * 0.25;
    return result;
}

// Get the harmonic(overtone) position
float PADnoteParameters::getNhr(int n)
{
    float result = 1.0;
    float par1 = powf(10.0, -(1.0 - Phrpos.par1 / 255.0) * 3.0);
    float par2 = Phrpos.par2 / 255.0;

    float n0 = n - 1.0;
    float tmp = 0.0;
    int thresh = 0;
    switch (Phrpos.type)
    {
    case 1:
        thresh = (int)(par2 * par2 * 100.0) + 1;
        if (n < thresh)
            result = n;
        else
            result = 1.0 + n0 + (n0 - thresh + 1.0) * par1 * 8.0;
        break;
    case 2:
        thresh = (int)(par2 * par2 * 100.0) + 1;
        if (n < thresh)
            result = n;
        else
            result = 1.0 + n0 - (n0 - thresh + 1.0) * par1 * 0.90;
        break;
    case 3:
        tmp = par1 * 100.0 + 1.0;
        result = powf(n0 / tmp, (1.0 - par2 * 0.8)) * tmp + 1.0;
        break;
    case 4:
        result = n0 * (1.0 - par1) + powf(n0 * 0.1, par2 * 3.0 + 1.0) * par1 * 10.0 + 1.0;
        break;
    case 5:
        result = n0 + sinf(n0 * par2 * par2 * PI * 0.999) * sqrtf(par1) * 2.0 + 1.0;
        break;
    case 6:
        tmp = powf((par2 * 2.0), 2.0) + 0.1;
        result = n0 * powf(1.0 + par1 * powf(n0 * 0.8, tmp), tmp) + 1.0;
        break;
    default:
        result=n;
        break;
    }
    float par3 = Phrpos.par3 / 255.0;
    float iresult = floorf(result + 0.5);
    float dresult = result-iresult;
    result = iresult + (1.0 - par3) * dresult;
    return result;
}

// Generates the long spectrum for Bandwidth mode (only amplitudes are generated;
// phases will be random)
void PADnoteParameters::generatespectrum_bandwidthMode(float *spectrum,
                                                       int size,
                                                       float basefreq,
                                                       float *profile,
                                                       int profilesize,
                                                       float bwadjust)
{
    for (int i = 0; i < size; ++i)
        spectrum[i] = 0.0;

    float harmonics[zynMaster->getOscilsize() / 2];
    memset(harmonics, 0, half_oscilsize * sizeof(float));
    //for (unsigned int u = 0; u < zynMaster->getOscilsize() / 2; ++u)
    //    harmonics[u] = 0.0;
    // get the harmonic structure from the oscillator (I am using the frequency amplitudes, only)
    oscilgen->get(harmonics, basefreq, false);

    // normalize
    float max = 0.0;
    for (int i = 0; i < half_oscilsize; ++i)
        if (harmonics[i] > max)
            max = harmonics[i];
    if (max < 0.000001)
        max = 1;
    for (int i = 0; i < half_oscilsize; ++i)
        harmonics[i] /= max;
    for (int nh = 1; nh < half_oscilsize; ++nh)
    {   //for each harmonic
        float realfreq = getNhr(nh) * basefreq;
        if (realfreq > samplerate * 0.49999)
            break;
        if (realfreq < 20.0)
            break;
        if (harmonics[nh - 1] < 1e-4)
            continue;
        //compute the bandwidth of each harmonic
        float bandwidthcents = setPbandwidth(Pbandwidth);
        float bw = (powf(2.0, bandwidthcents / 1200.0) - 1.0) * basefreq / bwadjust;
        float power = 1.0;
        switch (Pbwscale)
        {
        case 0:
            power = 1.0;
            break;
        case 1:
            power = 0.0;
            break;
        case 2:
            power = 0.25;
            break;
        case 3:
            power = 0.5;
            break;
        case 4:
            power = 0.75;
            break;
        case 5:
            power = 1.5;
            break;
        case 6:
            power = 2.0;
            break;
        case 7:
            power = -0.5;
            break;
        }
        bw = bw * powf(realfreq / basefreq, power);
        int ibw = (int)((bw / (samplerate * 0.5) * size)) + 1;
        float amp = harmonics[nh - 1];
        if (resonance->Penabled)
            amp *= resonance->getfreqresponse(realfreq);
        if (ibw > profilesize)
        {   // if the bandwidth is larger than the profilesize
            float rap = sqrtf((float)profilesize / (float)ibw);
            int cfreq = (int)(realfreq / (samplerate * 0.5) * size) - ibw / 2;
            for (int i = 0; i < ibw; ++i)
            {
                int src = (int)(i * rap * rap);
                int spfreq = i + cfreq;
                if (spfreq < 0)
                    continue;
                if (spfreq >= size)
                    break;
                spectrum[spfreq] += amp * profile[src] * rap;
            }
        }
        else
        {   // if the bandwidth is smaller than the profilesize
            float rap = sqrtf((float)ibw / (float)profilesize);
            float ibasefreq = realfreq / (samplerate * 0.5) * size;
            for (int i = 0; i < profilesize; ++i)
            {
                float idfreq = i / (float)profilesize - 0.5;
                idfreq *= ibw;
                int spfreq = (int)(idfreq + ibasefreq);
                float fspfreq = fmodf(idfreq + ibasefreq, (float)1.0);
                if (spfreq <= 0)
                    continue;
                if (spfreq >= size - 1)
                    break;
                spectrum[spfreq] += amp * profile[i] * rap * (1.0 - fspfreq);
                spectrum[spfreq + 1] += amp * profile[i] * rap * fspfreq;
            }
        }
    }
}

// Generates the long spectrum for non-Bandwidth modes (only amplitudes are generated; phases will be random)
void PADnoteParameters::generatespectrum_otherModes(float *spectrum,
                                                    int size,
                                                    float basefreq,
                                                    float *profile,
                                                    int profilesize,
                                                    float bwadjust)
{
    for (int i = 0; i < size; ++i)
        spectrum[i] = 0.0;

    float harmonics[half_oscilsize];
    memset(harmonics, 0, half_oscilsize * sizeof(float));

    // get the harmonic structure from the oscillator (I am using the frequency
    // amplitudes, only)
    oscilgen->get(harmonics, basefreq, false);

    // normalize
    float max = 0.0;
    for (int i = 0; i < half_oscilsize; ++i)
        if (harmonics[i] > max)
            max = harmonics[i];
    if (max < 0.000001)
        max = 1;
    for (int i = 0; i < half_oscilsize; ++i)
        harmonics[i] /= max;

    for (int nh = 1; nh < half_oscilsize; ++nh)
    {   //for each harmonic
        float realfreq = getNhr(nh) * basefreq;

        ///sa fac aici interpolarea si sa am grija daca frecv descresc

        if (realfreq > samplerate * 0.49999)
            break;
        if (realfreq < 20.0)
            break;
//	if (harmonics[nh-1]<1e-4) continue;

        float amp = harmonics[nh - 1];
        if (resonance->Penabled)
            amp *= resonance->getfreqresponse(realfreq);
        int cfreq = (int)(realfreq / (samplerate * 0.5) * size);
        spectrum[cfreq] = amp + 1e-9;
    }

    if (Pmode != 1)
    {
        int old = 0;
        for (int k = 1; k < size; ++k)
        {
            if ((spectrum[k] > 1e-10) || (k == (size - 1)))
            {
                int delta = k - old;
                float val1 = spectrum[old];
                float val2 = spectrum[k];
                float idelta = 1.0 / delta;
                for (int i = 0; i < delta; ++i)
                {
                    float x = idelta * i;
                    spectrum[old+i] = val1 * (1.0 - x) + val2 * x;
                }
                old = k;
            }
        }
    }
}

// Applies the parameters (i.e. computes all the samples, based on parameters);
void PADnoteParameters::applyparameters(bool lockmutex)
{
    const int samplesize = (((int)1) << (Pquality.samplesize + 14));
    int spectrumsize = samplesize / 2;
    float spectrum[spectrumsize];
    int profilesize = 512;
    float profile[profilesize];

    float bwadjust = getprofile(profile, profilesize);
//    for (int i=0;i<profilesize;i++) profile[i]*=profile[i];
    float basefreq = 65.406 * powf(2.0, Pquality.basenote / 2);
    if (Pquality.basenote %2 == 1)
        basefreq *= 1.5;

    int samplemax = Pquality.oct + 1;
    int smpoct = Pquality.smpoct;
    if (Pquality.smpoct == 5)
        smpoct = 6;
    if (Pquality.smpoct == 6)
        smpoct = 12;
    if (smpoct != 0)
        samplemax *= smpoct;
    else
        samplemax = samplemax / 2 + 1;
    if (samplemax == 0)
        samplemax = 1;

    // prepare a BIG FFT stuff
    FFTwrapper *fft = new FFTwrapper(samplesize);
    FFTFREQS fftfreqs;
    newFFTFREQS(&fftfreqs, samplesize / 2);

    float adj[samplemax]; // this is used to compute frequency relation to the base frequency
    for (int nsample = 0; nsample < samplemax; ++nsample)
        adj[nsample] = (Pquality.oct + 1.0) * (float)nsample / samplemax;
    for (int nsample = 0; nsample < samplemax; ++nsample)
    {
        float tmp = adj[nsample] - adj[samplemax - 1] * 0.5;
        float basefreqadjust = powf(2.0, tmp);

        if (Pmode == 0)
            generatespectrum_bandwidthMode(spectrum, spectrumsize,
                                           basefreq * basefreqadjust, profile,
                                           profilesize, bwadjust);
        else
            generatespectrum_otherModes(spectrum, spectrumsize,
                                        basefreq * basefreqadjust, profile,
                                        profilesize, bwadjust);

        const int extra_samples = 5; // the last samples contains the first
                                     // samples (used for linear/cubic interpolation)
        newsample.smp = new float[samplesize + extra_samples];

        newsample.smp[0] = 0.0;
        for (int i = 1; i < spectrumsize; ++i)
        {   // randomize the phases
            float phase = RND * 6.29;
            fftfreqs.c[i] = spectrum[i] * cosf(phase);
            fftfreqs.s[i] = spectrum[i] * sinf(phase);
        }
        fft->freqs2smps(fftfreqs, newsample.smp);
        // that's all; here is the only ifft for the whole sample; no windows are used ;-)

        // normalize(rms)
        float rms = 0.0;
        for (int i = 0; i < samplesize; ++i)
            rms += newsample.smp[i] * newsample.smp[i];
        rms = sqrtf(rms);
        if (rms < 0.000001)
            rms = 1.0;
        rms *= sqrtf(262144.0 / samplesize);
        for (int i = 0; i < samplesize; ++i)
            newsample.smp[i] *= 1.0 / rms * 50.0;

        // prepare extra samples used by the linear or cubic interpolation
        for (int i = 0; i < extra_samples; ++i)
            newsample.smp[i + samplesize] = newsample.smp[i];

        // replace the current sample with the new computed sample
        if (lockmutex)
            zynMaster->actionLock(lock);
        deletesample(nsample);
        sample[nsample].smp = newsample.smp;
        sample[nsample].size = samplesize;
        sample[nsample].basefreq = basefreq * basefreqadjust;
        if (lockmutex)
            zynMaster->actionLock(unlock);
        newsample.smp = NULL;
    }
    delete(fft);
    deleteFFTFREQS(&fftfreqs);

    // delete the additional samples that might exists and are not useful
    if (lockmutex)
        zynMaster->actionLock(lock);
    for (int i = samplemax; i < PAD_MAX_SAMPLES; ++i)
        deletesample(i);
    if (lockmutex)
        zynMaster->actionLock(unlock);
}

/**
void PADnoteParameters::export2wav(string basefilename)
{
    applyparameters(true);
    basefilename += "_PADsynth_";
    for (int k = 0; k < PAD_MAX_SAMPLES; ++k)
    {
        if (sample[k].smp == NULL)
            continue;
        char tmpstr[20];
        snprintf(tmpstr,20, "_%02d", k + 1);
        string filename = basefilename + string(tmpstr) + ".wav";
        WAVaudiooutput wav;
        if (wav.newfile(filename, zynMaster->getSamplerate(), 1))
        {
            int nsmps = sample[k].size;
            short int *smps = new short int[nsmps];
            for (int i = 0; i < nsmps; ++i)
                smps[i] = (short int)(sample[k].smp[i] * 32767.0);
            wav.write_mono_samples(nsmps, smps);
            wav.close();
        }
    }
}
**/

void PADnoteParameters::add2XML(XMLwrapper *xml)
{
    xml->information.PADsynth_used=true;

    xml->addparbool("stereo",PStereo);
    xml->addpar("mode",Pmode);
    xml->addpar("bandwidth",Pbandwidth);
    xml->addpar("bandwidth_scale",Pbwscale);

    xml->beginbranch("HARMONIC_PROFILE");
    xml->addpar("base_type",Php.base.type);
    xml->addpar("base_par1",Php.base.par1);
    xml->addpar("frequency_multiplier",Php.freqmult);
    xml->addpar("modulator_par1",Php.modulator.par1);
    xml->addpar("modulator_frequency",Php.modulator.freq);
    xml->addpar("width",Php.width);
    xml->addpar("amplitude_multiplier_type",Php.amp.type);
    xml->addpar("amplitude_multiplier_mode",Php.amp.mode);
    xml->addpar("amplitude_multiplier_par1",Php.amp.par1);
    xml->addpar("amplitude_multiplier_par2",Php.amp.par2);
    xml->addparbool("autoscale",Php.autoscale);
    xml->addpar("one_half",Php.onehalf);
    xml->endbranch();

    xml->beginbranch("OSCIL");
    oscilgen->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("RESONANCE");
    resonance->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("HARMONIC_POSITION");
    xml->addpar("type",Phrpos.type);
    xml->addpar("parameter1",Phrpos.par1);
    xml->addpar("parameter2",Phrpos.par2);
    xml->addpar("parameter3",Phrpos.par3);
    xml->endbranch();

    xml->beginbranch("SAMPLE_QUALITY");
    xml->addpar("samplesize",Pquality.samplesize);
    xml->addpar("basenote",Pquality.basenote);
    xml->addpar("octaves",Pquality.oct);
    xml->addpar("samples_per_octave",Pquality.smpoct);
    xml->endbranch();

    xml->beginbranch("AMPLITUDE_PARAMETERS");
    xml->addpar("volume",PVolume);
    xml->addpar("panning",PPanning);
    xml->addpar("velocity_sensing",PAmpVelocityScaleFunction);
    xml->addpar("punch_strength",PPunchStrength);
    xml->addpar("punch_time",PPunchTime);
    xml->addpar("punch_stretch",PPunchStretch);
    xml->addpar("punch_velocity_sensing",PPunchVelocitySensing);

    xml->beginbranch("AMPLITUDE_ENVELOPE");
    AmpEnvelope->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("AMPLITUDE_LFO");
    AmpLfo->add2XML(xml);
    xml->endbranch();

    xml->endbranch();

    xml->beginbranch("FREQUENCY_PARAMETERS");
    xml->addpar("fixed_freq",Pfixedfreq);
    xml->addpar("fixed_freq_et",PfixedfreqET);
    xml->addpar("detune",PDetune);
    xml->addpar("coarse_detune",PCoarseDetune);
    xml->addpar("detune_type",PDetuneType);

    xml->beginbranch("FREQUENCY_ENVELOPE");
    FreqEnvelope->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("FREQUENCY_LFO");
    FreqLfo->add2XML(xml);
    xml->endbranch();
    xml->endbranch();

    xml->beginbranch("FILTER_PARAMETERS");
    xml->addpar("velocity_sensing_amplitude",PFilterVelocityScale);
    xml->addpar("velocity_sensing",PFilterVelocityScaleFunction);

    xml->beginbranch("FILTER");
    GlobalFilter->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("FILTER_ENVELOPE");
    FilterEnvelope->add2XML(xml);
    xml->endbranch();

    xml->beginbranch("FILTER_LFO");
    FilterLfo->add2XML(xml);
    xml->endbranch();
    xml->endbranch();
}

void PADnoteParameters::getfromXML(XMLwrapper *xml)
{
    PStereo=xml->getparbool("stereo",PStereo);
    Pmode=xml->getpar127("mode",0);
    Pbandwidth=xml->getpar("bandwidth",Pbandwidth,0,1000);
    Pbwscale=xml->getpar127("bandwidth_scale",Pbwscale);

    if (xml->enterbranch("HARMONIC_PROFILE")) {
        Php.base.type=xml->getpar127("base_type",Php.base.type);
        Php.base.par1=xml->getpar127("base_par1",Php.base.par1);
        Php.freqmult=xml->getpar127("frequency_multiplier",Php.freqmult);
        Php.modulator.par1=xml->getpar127("modulator_par1",Php.modulator.par1);
        Php.modulator.freq=xml->getpar127("modulator_frequency",Php.modulator.freq);
        Php.width=xml->getpar127("width",Php.width);
        Php.amp.type=xml->getpar127("amplitude_multiplier_type",Php.amp.type);
        Php.amp.mode=xml->getpar127("amplitude_multiplier_mode",Php.amp.mode);
        Php.amp.par1=xml->getpar127("amplitude_multiplier_par1",Php.amp.par1);
        Php.amp.par2=xml->getpar127("amplitude_multiplier_par2",Php.amp.par2);
        Php.autoscale=xml->getparbool("autoscale",Php.autoscale);
        Php.onehalf=xml->getpar127("one_half",Php.onehalf);
        xml->exitbranch();
    }

    if (xml->enterbranch("OSCIL")) {
        oscilgen->getfromXML(xml);
        xml->exitbranch();
    }

    if (xml->enterbranch("RESONANCE")) {
        resonance->getfromXML(xml);
        xml->exitbranch();
    }

    if (xml->enterbranch("HARMONIC_POSITION")) {
        Phrpos.type=xml->getpar127("type",Phrpos.type);
        Phrpos.par1=xml->getpar("parameter1",Phrpos.par1,0,255);
        Phrpos.par2=xml->getpar("parameter2",Phrpos.par2,0,255);
        Phrpos.par3=xml->getpar("parameter3",Phrpos.par3,0,255);
        xml->exitbranch();
    }

    if (xml->enterbranch("SAMPLE_QUALITY")) {
        Pquality.samplesize=xml->getpar127("samplesize",Pquality.samplesize);
        Pquality.basenote=xml->getpar127("basenote",Pquality.basenote);
        Pquality.oct=xml->getpar127("octaves",Pquality.oct);
        Pquality.smpoct=xml->getpar127("samples_per_octave",Pquality.smpoct);
        xml->exitbranch();
    }

    if (xml->enterbranch("AMPLITUDE_PARAMETERS")) {
        PVolume=xml->getpar127("volume",PVolume);
        PPanning=xml->getpar127("panning",PPanning);
        PAmpVelocityScaleFunction=xml->getpar127("velocity_sensing",PAmpVelocityScaleFunction);
        PPunchStrength=xml->getpar127("punch_strength",PPunchStrength);
        PPunchTime=xml->getpar127("punch_time",PPunchTime);
        PPunchStretch=xml->getpar127("punch_stretch",PPunchStretch);
        PPunchVelocitySensing=xml->getpar127("punch_velocity_sensing",PPunchVelocitySensing);

        xml->enterbranch("AMPLITUDE_ENVELOPE");
        AmpEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("AMPLITUDE_LFO");
        AmpLfo->getfromXML(xml);
        xml->exitbranch();

        xml->exitbranch();
    }

    if (xml->enterbranch("FREQUENCY_PARAMETERS")) {
        Pfixedfreq=xml->getpar127("fixed_freq",Pfixedfreq);
        PfixedfreqET=xml->getpar127("fixed_freq_et",PfixedfreqET);
        PDetune=xml->getpar("detune",PDetune,0,16383);
        PCoarseDetune=xml->getpar("coarse_detune",PCoarseDetune,0,16383);
        PDetuneType=xml->getpar127("detune_type",PDetuneType);

        xml->enterbranch("FREQUENCY_ENVELOPE");
        FreqEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FREQUENCY_LFO");
        FreqLfo->getfromXML(xml);
        xml->exitbranch();
        xml->exitbranch();
    }

    if (xml->enterbranch("FILTER_PARAMETERS")) {
        PFilterVelocityScale=xml->getpar127("velocity_sensing_amplitude",PFilterVelocityScale);
        PFilterVelocityScaleFunction=xml->getpar127("velocity_sensing",PFilterVelocityScaleFunction);

        xml->enterbranch("FILTER");
        GlobalFilter->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FILTER_ENVELOPE");
        FilterEnvelope->getfromXML(xml);
        xml->exitbranch();

        xml->enterbranch("FILTER_LFO");
        FilterLfo->getfromXML(xml);
        xml->exitbranch();
        xml->exitbranch();
    }
}
