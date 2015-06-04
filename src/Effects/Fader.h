/*
    Fader.h

    Copyright 2009, Alan Calvert

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

#ifndef FADER_H
#define FADER_H

// Pseudo logarithmic volume control,
//  ack http://www.maazl.de/project/pm123/index.html#logvolum_1.0
class Fader
{
    public:
        Fader(double maxvol);
        ~Fader() { };
        float Level(unsigned char controlvalue)
            { return (controlvalue <= 127 ) ? scaler[controlvalue] : 1.0f; };
    private:
        const double scalefactor; // default internal scaling factor 3.16227766,
                                  // should not exceed sqrt(10) (+10dB)
        float scaler[128];
        bool primed;
};

#endif
