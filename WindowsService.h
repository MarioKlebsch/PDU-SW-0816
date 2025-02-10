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

#ifndef WINDOWS_SERVICE_H_
#define WINDOWS_SERVICE_H_

#include <memory>
#include <string>

namespace windows
{

	class service
	{
		class impl;
		std::unique_ptr<impl> pimpl;

		virtual std::string name() const = 0;
		virtual std::string display_name() const { return name(); }
		virtual std::string path() const;
		virtual std::string arguments() const { return ""; };

		virtual int init() = 0;
		virtual int mainloop() = 0;
		virtual void stop() = 0;

		service(const service&) = delete;
		service& operator=(const service&) = delete;

	protected:
		service();

	public:
		~service();
		int handle_command(std::string_view cmd, std::string_view command_and_prefix);
	};
}
#endif /* WINDOWS_SERVICE_H_ */