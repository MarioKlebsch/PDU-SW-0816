/*
   power_switch: control Argus PDU SW-0816
   Copyright (C) 2025  Mario Klebsch, DG1AM
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PDU_TYPES_H_
#define PDU_TYPES_H_

#include <set>

enum channel { ch1=0, ch2, ch3, ch4, ch5, ch6, ch7, ch8};

enum op_t { on=0, off=1};

struct szene
{
    std::set<channel> on;
    std::set<channel> off;
};

#endif /* PDU_TYPES_H_ */
