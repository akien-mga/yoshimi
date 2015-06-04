/*
    Analog Filter.h - Several analog filters (lowpass, highpass...)

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

#ifndef ANALOG_FILTER_H
#define ANALOG_FILTER_H

#include "globals.h"
#include "DSP/Filter_.h"

class AnalogFilter : public Filter_
{
    public:
        AnalogFilter(unsigned char Ftype, float Ffreq, float Fq,
                     unsigned char Fstages);
        ~AnalogFilter();
        void filterout(float *smp);
        void setfreq(float frequency);
        void setfreq_and_q(float frequency, float q_);
        void setq(float q_);

        void settype(int type_);
        void setgain(float dBgain);
        void setstages(int stages_);
        void cleanup();

        float H(float freq); // Obtains the response for a given frequency

    private:
        struct fstage {
            float c1, c2;
        } x[MAX_FILTER_STAGES + 1],
          y[MAX_FILTER_STAGES + 1],
          oldx[MAX_FILTER_STAGES + 1],
          oldy[MAX_FILTER_STAGES + 1];

        void singlefilterout(float *smp, fstage &x, fstage &y, float *c,
                             float *d);
        void computefiltercoefs();
        int type;   // The type of the filter (LPF1,HPF1,LPF2,HPF2...)
        int stages; // how many times the filter is applied (0->1,1->2,etc.)
        float freq; // Frequency given in Hz
        float q;    // Q factor (resonance or Q factor)
        float gain; // the gain of the filter (if are shelf/peak) filters

        int order; // the order of the filter (number of poles)

        float c[3], d[3]; // coefficients

        float oldc[3], oldd[3]; // old coefficients(used only if some filter paremeters changes very fast, and it needs interpolation)

        float xd[3], yd[3]; // used if the filter is applied more times
        int needsinterpolation, firsttime;
        int abovenq;    // this is 1 if the frequency is above the nyquist
        int oldabovenq; // if the last time was above nyquist (used to see if it needs interpolation)

        unsigned int samplerate;
        int buffersize;
};

#endif
