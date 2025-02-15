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

const std::string addr{"<ip addr or hostname>"};
const std::string port{"80"};
const std::string user{"<user>"};
const std::string password{"<password>"};

#define PROXY_BIND_PORT 8192
#define PROXY_BIND_ADDR "localhost"

// define channel names
static const std::map<std::string_view, channel, case_insensitive> map_channel_name_to_index{
    {"ch1", ch1},
    {"ch2", ch2},
    {"ch3", ch3},
    {"ch4", ch4},
    {"ch5", ch5},
    {"ch6", ch6},
    {"ch7", ch7},
    {"ch8", ch8},

// example with RGB lights, make sure to remove the lines ch1-ch3 above.
//    {"red",   ch1},
//    {"green", ch2},
//    {"blue",  ch3},
};

// define scene names
static const std::map<std::string_view, scene, case_insensitive> scenes {
    {"scene0", { /*off*/{ch1, ch2}, /*on*/{} }},
    {"scene1", { /*off*/{ch2},      /*on*/{ch1} }},
    {"scene2", { /*off*/{ch1},      /*on*/{ch2} }},

// example for scenes based RGB lights
//    {"black",    { /*off*/{ch1, ch2, ch3},  /*on*/{             } }},
//    {"red",      { /*off*/{     ch2, ch3},  /*on*/{ch1          } }},
//    {"green",    { /*off*/{ch1,      ch3},  /*on*/{     ch2     } }},
//    {"blue",     { /*off*/{ch1, ch2     },  /*on*/{          ch3} }},
//    {"cyan",     { /*off*/{ch1          },  /*on*/{     ch2, ch3} }},
//    {"mangenta", { /*off*/{     ch2     },  /*on*/{ch1,      ch3}} }},
//    {"yellow",   { /*off*/{          ch3},  /*on*/{ch1, ch2     } }},
//    {"white",    { /*off*/{             },  /*on*/{ch1, ch2, ch3} }},
};

