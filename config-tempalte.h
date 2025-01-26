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

// NOTE: copy this template file to config.h and change the copy.

#include <map>
#include <string>
#include "pdu_types.h"
#include "case_insensitive.h"

constexpr std::string addr{"<ip addr or hostname>"};
constexpr std::string user{"<user>"};
constexpr std::string password{"<password>"};

// define channel names
static const std::map<const char*, channel, case_insensitive> map_channel_name_to_index{
    {"ch1", ch1},
    {"ch2", ch2},
    {"ch3", ch3},
    {"ch4", ch4},
    {"ch5", ch5},
    {"ch6", ch6},
    {"ch7", ch7},
    {"ch8", ch8},
};

// define szene names
static const std::map<const char*, szene, case_insensitive> szenes {
    {"szene0", { /*on*/{}, /*off*/{ch1, ch2} }},
    {"szene1", { /*on*/{ch1}, /*off*/{ch2} }},
    {"szene2", { /*on*/{ch2}, /*off*/{ch1} }},
};

