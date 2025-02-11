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

#ifndef CASE_INSENSITIVS_H_
#define CASE_INSENSITIVS_H_

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
// case-insesitive string compare
bool iequals(std::string_view lhs, std::string_view rhs)
{
    return std::equal(lhs.begin(), lhs.end(),
                      rhs.begin(), rhs.end(),
                      [](char lhs, char rhs) {
                          return std::tolower(lhs) == std::tolower(rhs);
                      });
}

// case insensitive string compare for std::map
struct case_insensitive
{
    bool operator()(std::string_view lhs, std::string_view rhs) const
    {
        auto lhs_it = lhs.begin();
        auto rhs_it = rhs.begin();
        while (lhs_it != lhs.end() && rhs_it != rhs.end())
        {
            const auto ch_lhs = std::tolower(*lhs_it++);
            const auto ch_rhs = std::tolower(*rhs_it++);

            if (ch_lhs < ch_rhs)
                return true;
            else if (ch_lhs > ch_rhs)
                return false;
        }
        return rhs_it != rhs.end();
    }
};
}

#endif /* CASE_INSENSITIVS_H_ */
