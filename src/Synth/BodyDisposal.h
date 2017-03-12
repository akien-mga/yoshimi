/*
    BodyDisposal.h

    Copyright 2011, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BODYDISPOSAL_H
#define BODYDISPOSAL_H

#include <list>
#include <cstddef>
#include <atomic>

using namespace std;

#include "Synth/Carcass.h"

class BodyDisposal
{
    public:
        BodyDisposal() { inFront = true; corpsePile[0].clear(); corpsePile[1].clear(); }
        ~BodyDisposal() {}
        void addBody(Carcass *body);
        void disposeBodies(void);

    private:
        std::atomic<bool> inFront;
        list<Carcass*> corpsePile[2];
};

#endif
