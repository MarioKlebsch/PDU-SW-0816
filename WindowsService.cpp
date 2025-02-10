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


#include "WindowsService.h"
#include "case_insensitive.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <system_error>
#include <thread>
#include <Windows.h>



using namespace std::string_literals;

namespace
{
	enum service_state
	{
		stopped          = SERVICE_STOPPED,
		start_pending    = SERVICE_START_PENDING,
		stop_pending     = SERVICE_STOP_PENDING,
		running          = SERVICE_RUNNING,
		continue_pending = SERVICE_CONTINUE_PENDING,
		pause_pending    = SERVICE_PAUSE_PENDING,
		paused           = SERVICE_PAUSED,
	};

	static const char* to_string(service_state status)
	{
		switch (status)
		{
		case stopped:          return "stopped";
		case start_pending:    return "start pending";
		case stop_pending:     return "stop pending";
		case running:          return "running";
		case continue_pending: return "continue pending";
		case pause_pending:    return "pause pending";
		case paused:           return "paused";
		default:                       return "???";
		}
	}

	inline std::ostream& operator<<(std::ostream& s, service_state status) { return s << to_string(status); }

	static const char* start_type_to_string(DWORD start_type)
	{
		switch (start_type)
		{
		case SERVICE_BOOT_START:       return "boot start";
		case SERVICE_SYSTEM_START:     return "system start";
		case SERVICE_AUTO_START:       return "auto start";
		case SERVICE_DEMAND_START:     return "start on demand";
		case SERVICE_DISABLED:         return "disabled";
		default:                       return "???";
		}
	}

	struct service_closer
	{
		void operator()(SC_HANDLE h) { CloseServiceHandle(h); }
	};

	using service_h = std::unique_ptr<std::remove_reference<decltype(*std::declval<SC_HANDLE>())>::type, service_closer >;

	class manager
	{
		service_h handle;
	public:
		class service
		{
			friend class manager;
			service_h handle;

			explicit service(SC_HANDLE handle) :handle{ std::move(handle) } {}
		public:
			service() = default;
			service(service&&) = default;
			service& operator=(service&&) = default;

			explicit operator bool() const { return handle.get(); }
			operator SC_HANDLE() const { return handle.get(); }

			service_state get_status(std::error_code& ec) const
			{
				SERVICE_STATUS_PROCESS ret;
				DWORD dwBytesNeeded;

				if (!QueryServiceStatusEx(
					*this,                    // handle to service 
					SC_STATUS_PROCESS_INFO,   // information level
					(LPBYTE)&ret,             // address of structure
					sizeof(ret),              // size of structure
					&dwBytesNeeded))          // size needed if buffer is too small
				{
					ec = { int(GetLastError()), std::system_category() };
					return {};
				}
				ec = {};
				return service_state(ret.dwCurrentState);
			}

			QUERY_SERVICE_CONFIGA get_config(std::vector<char>& buffer, std::error_code& ec) const
			{
				DWORD bytes_needed;
				if (!QueryServiceConfigA(*this, nullptr, 0, &bytes_needed) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				{
					ec = { int(GetLastError()), std::system_category() };
					return {};
				}
				buffer.resize(bytes_needed);
				if (!QueryServiceConfigA(*this, (LPQUERY_SERVICE_CONFIGA)buffer.data(), (DWORD)buffer.size(), &bytes_needed))
				{
					ec = { int(GetLastError()), std::system_category() };
					return {};
				}

				ec = {};
				return reinterpret_cast<QUERY_SERVICE_CONFIGA&>(buffer[0]);
			}


		};

		manager(DWORD access, std::error_code& ec) :
			handle{ OpenSCManager(NULL, NULL, access) }
		{
			if (!handle)
				ec = { int(GetLastError()), std::system_category() };
			else
				ec = {};
		}

		manager(std::error_code& ec) : manager(SC_MANAGER_ALL_ACCESS, ec)
		{
		}
		explicit operator bool() const { return handle.get(); }
		operator SC_HANDLE() const { return handle.get(); }

		service open(const std::string& name, DWORD access, std::error_code& ec)
		{
			service s{ OpenServiceA(*this, name.c_str(), access) };
			if (!s)
			{
				ec = { int(GetLastError()), std::system_category() };
				return {};
			}
			ec = {};
			return s;
		}
	};


}

#define SERVICE_MAIN_COMMAND "service_main"

namespace windows
{
	void* instance{ nullptr };

	class service::impl
	{
		service& inst;

	public:
		impl(service& inst) : inst{ inst } {}

		inline std::string name() const         { return inst.name(); }
		inline std::string display_name() const { return inst.display_name(); }
		inline std::string path() const         { return inst.path(); }
		inline std::string arguments() const    { return inst.arguments() + " " + SERVICE_MAIN_COMMAND; }

		int install()
		{
			const auto cmd = "\""s + path() + "\" " + arguments();

			std::error_code ec;
			manager m{ ec };
			if (ec)
			{
				std::cerr << "OpenSCManager() failed: " << ec.message() << "\n";
				return -1;
			}

			SC_HANDLE h = CreateServiceA(
				m,                           // SCM database 
				name().c_str(),              // name of service 
				display_name().c_str(),      // service name to display 
				SERVICE_ALL_ACCESS,          // desired access 
				SERVICE_WIN32_OWN_PROCESS,   // service type 
				SERVICE_AUTO_START,          // start type 
				SERVICE_ERROR_NORMAL,        // error control type 
				cmd.c_str(),                 // path to service's binary 
				NULL,                        // no load ordering group 
				NULL,                        // no tag identifier 
				NULL,                        // no dependencies 
				NULL,                        // LocalSystem account 
				NULL);                       // no password 
			if (!h)
			{
				ec = { int(GetLastError()), std::system_category() };
				std::cerr << "CreateService() failed: " << ec.message() << "\n";
				return -1;
			}
			return 0;
		}

		int uninstall()
		{
			std::error_code ec;
			manager m{ ec };
			if (ec)
			{
				std::cerr << "OpenSCManager() failed: " << ec.message() << "\n";
				return -1;
			}

			auto s = m.open(name(), DELETE | SERVICE_QUERY_STATUS, ec);
			if (ec)
				return -1;

			auto state = s.get_status(ec);
			if (ec)
			{
				std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
				return -1;
			}
			if (state != stopped)
			{
				auto ret = stop();
				if (ret)
					return ret;
			}

			if (!DeleteService(s))
			{
				std::error_code ec{ int(GetLastError()), std::system_category() };
				std::cerr << "DeleteService(" << name() << ") failed: " << ec.message() << "\n";
				return -1;
			}
			return 0;
		}

		int start()
		{
			std::error_code ec;
			manager m{ ec };
			if (ec)
			{
				std::cerr << "OpenSCManager() failed: " << ec.message() << "\n";
				return -1;
			}

			auto s = m.open(name(), SERVICE_START | SERVICE_QUERY_STATUS, ec);
			if (ec == std::error_code{ ERROR_SERVICE_DOES_NOT_EXIST, std::system_category() })
			{
				auto ret = install();
				if (ret)
					return ret;
				s = std::move(m.open(name(), SERVICE_START | SERVICE_QUERY_STATUS, ec));
			}
			if (ec)
			{
				std::cerr << "OpenService(" << name() << ") failed: " << ec.message() << "\n";
				return -1;
			}

			auto state = s.get_status(ec);
			if (ec)
			{
				std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
				return -1;
			}

			if (state != stopped && state != stop_pending)
			{
				std::cerr << "Cannot start the service " << name() << " because it is already running\n";
				return -1;
			}

			auto start = std::chrono::system_clock::now();
			while (state == stop_pending)
			{

				if (std::chrono::system_clock::now() - start > std::chrono::seconds(10))
				{
					std::cerr << "timeout stopping service " << name() << "\n";
					return -1;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				state = s.get_status(ec);
				if (ec)
				{
					std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
					return -1;
				}
			}

			if (!StartService(s, 0, NULL) )
			{
				std::error_code ec{ int(GetLastError()), std::system_category() };
				std::cerr << "StartService(" << name() << ") failed: " << ec.message() << "\n";
				return -1;
			}

			state = s.get_status(ec);
			if (ec)
			{
				std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
				return -1;
			}
			start = std::chrono::system_clock::now();
			while (state == start_pending)
			{

				if (std::chrono::system_clock::now() - start > std::chrono::seconds(10))
				{
					std::cerr << "timeout starting service " << name() << "\n";
					return -1;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				state = s.get_status(ec);
				if (ec)
				{
					std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
					return -1;
				}
			}

			if (state != running)
			{
				std::cerr << "service " << name() << " is not running\n";
				return -1;
			}

			return 0;
		}

		int stop()
		{
			std::error_code ec;
			manager m{ ec };
			if (ec)
			{
				std::cerr << "OpenSCManager() failed: " << ec.message() << "\n";
				return -1;
			}

			auto s = m.open(name(), SERVICE_STOP | SERVICE_QUERY_STATUS, ec);
			if (ec)
			{
				std::cerr << "OpenService(" << name() << ") failed: " << ec.message() << "\n";
				return -1;
			}

			auto start = std::chrono::system_clock::now();
			service_state state;
			for (;;)
			{
				state = s.get_status(ec);
				if (ec)
				{
					std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
					return -1;
				}
				if (state != stop_pending)
					break;

				if (std::chrono::system_clock::now() - start > std::chrono::seconds(10))
				{
					std::cerr << "timeout waiting for termination of " << name() << "service\n";
					break;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			if (state == stopped)
			{
				std::cerr << "Service " << name() << " is already stopped.\n";
				return 0;
			}

			SERVICE_STATUS req;
			if (!ControlService(s, SERVICE_CONTROL_STOP, &req))
			{
				ec = { int(GetLastError()), std::system_category() };
				std::cerr << "ControlService(" << name() << ", SERVICE_CONTROL_STOP) failed: " << ec.message() << "\n";
				return -1;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(15));

			start = std::chrono::system_clock::now();
			for (;;)
			{
				state = s.get_status(ec);
				if (ec)
				{
					std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
					return -1;
				}
				if (state != stop_pending)
					break;
				if (std::chrono::system_clock::now() - start > std::chrono::seconds(10))
				{
					std::cerr << "timeout waiting for termination of " << name() << "service\n";
					return -1;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			if (state != stopped)
			{
				std::cerr << "Service " << name() << " not stopped.\n";
				return -1;
			}

			return 0;
		}

		int status()
		{
			std::error_code ec;
			manager m{ SC_MANAGER_ENUMERATE_SERVICE | SERVICE_QUERY_CONFIG, ec };
			if (ec)
			{
				std::cerr << "OpenSCManager() failed: " << ec.message() << "\n";
				return -1;
			}

			auto s = m.open(name(), SC_MANAGER_ENUMERATE_SERVICE | SERVICE_QUERY_CONFIG, ec);
			if (ec)
			{
				if (ec == std::error_code{ ERROR_SERVICE_DOES_NOT_EXIST, std::system_category() })
				{
					std::cout << "name:             " << name() << "\n";
					std::cout << "status:           " << "not installed\n";
					return 0;
				}
				std::cerr << "OpenService(" << name() << ") failed: " << ec.message() << "\n";
				return -1;
			}
			auto status = s.get_status(ec);
			if (ec)
			{
				std::cerr << "QueryServiceStatusEx() failed: " << ec.message() << "\n";
				return -1;
			}

			std::vector<char> buffer;
			auto config = s.get_config(buffer, ec);
			if (ec)
			{
				std::cerr << "QueryServiceConfig() failed: " << ec.message() << "\n";
				return -1;
			}


			std::cout << "name:             " << name() << "\n";
			std::cout << "display name:     " << config.lpDisplayName << "\n";
			std::cout << "status:           " << status << "\n";
			std::cout << "start type:       " << start_type_to_string(config.dwStartType) << "\n";
			std::cout << "path name:        " << config.lpBinaryPathName << "\n";
			std::cout << "user name:        " << config.lpServiceStartName << "\n";
			if (config.lpLoadOrderGroup && *config.lpLoadOrderGroup)
				std::cout << "load order group: " << config.lpLoadOrderGroup << "\n";
			if (config.lpDependencies && *config.lpDependencies)
				std::cout << "dependencies:     " << config.lpDependencies << "\n";

			return 0;
		}

		SERVICE_STATUS_HANDLE SvcStatusHandle;
		SERVICE_STATUS        SvcStatus;

		void setStatus(service_state status)
		{
			static DWORD dwCheckPoint = 1;

			SvcStatus.dwCurrentState = DWORD(status);
			SvcStatus.dwWin32ExitCode = 0;
			SvcStatus.dwWaitHint = 3000;

			SvcStatus.dwControlsAccepted = status == start_pending ? 0 : SERVICE_ACCEPT_STOP;

			if ((status == running) ||
				(status == stopped))
				SvcStatus.dwCheckPoint = 0;
			else
				SvcStatus.dwCheckPoint = dwCheckPoint++;

			SetServiceStatus(SvcStatusHandle, &SvcStatus);
		}

		static void WINAPI control(DWORD ctrl)
		{
			assert(instance);
			auto This = static_cast<impl*>(instance);
			switch (ctrl)
			{
			case SERVICE_CONTROL_STOP:
				This->inst.stop();
				break;
			}
		}

		int main(int argc, char* argv[])
		{

			SvcStatusHandle = RegisterServiceCtrlHandlerA(name().c_str(), &impl::control);
			if (!SvcStatusHandle)
			{
				std::error_code ec{ int(GetLastError()), std::system_category() };
				std::cerr << "RegisterServiceCtrlHandler(" << name() << ") failed: " << ec.message() << "\n";
				return -1;
			}

			SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
			SvcStatus.dwServiceSpecificExitCode = 0;

			setStatus(start_pending);
			auto ret = inst.init();
			if (ret)
				return ret;
			setStatus(running);

			return inst.mainloop();
		}

		static VOID WINAPI callServiceMain(DWORD dwNumServicesArgs, LPSTR* lpServiceArgVectors)
		{
			assert(instance);
			auto This = static_cast<impl*>(instance);
			This->SvcStatus.dwServiceSpecificExitCode = This->main(dwNumServicesArgs, lpServiceArgVectors);
			This->setStatus(stopped);
		}

		int handle_command(std::string_view cmd, std::string_view command_and_prefix)
		{
			if (iequals(cmd, "status"))
				return status();
			if (iequals(cmd, "install"))
				return install();
			if (iequals(cmd, "uninstall"))
				return uninstall();
			if (iequals(cmd, "start"))
				return start();
			if (iequals(cmd, "stop"))
				return stop();
			if (iequals(cmd, "service_main"))
			{
				assert(!instance);
				instance = this;
				SERVICE_TABLE_ENTRYA DispatchTable[] =
				{
					{ (char*)name().c_str(), (LPSERVICE_MAIN_FUNCTIONA)impl::callServiceMain},
					{ NULL, NULL }
				};

				if (!StartServiceCtrlDispatcherA(DispatchTable))
				{
					std::error_code ec{ int(GetLastError()), std::system_category() };
					std::cerr << "StartServiceCtrlDispatcher() failed: " << ec.message() << "\n";
					return -1;
				}
				return 0;
			}

			if (!cmd.empty())
				std::cerr << "usage:\n";
			std::cerr << "    " << command_and_prefix << " install     : install "   << display_name() << " service\n";
			std::cerr << "    " << command_and_prefix << " uninstall   : uninstall " << display_name() << " service\n";;
			std::cerr << "    " << command_and_prefix << " start       : start "     << display_name() << " service\n";;
			std::cerr << "    " << command_and_prefix << " stop        : stop "      << display_name() << " service\n";;
			std::cerr << "    " << command_and_prefix << " status      : show "      << display_name() << " service status\n";;

			return -1;
		}
	};


	service::service() :
		pimpl{ std::make_unique<impl>(*this) }
	{
	}

	std::string service::path() const
	{
		static char path[MAX_PATH] = "";
		if (!path[0] && !GetModuleFileNameA(NULL, path, MAX_PATH))
		{
			std::error_code ec{ int(GetLastError()), std::system_category() };
			std::cerr << "GetModuleFileName() failed: " << ec.message() << "\n";
			return {};
		}

		return path;
	}

	service::~service() = default;

	int service::handle_command(std::string_view cmd, std::string_view command_and_prefix)
	{
		return pimpl->handle_command(cmd, command_and_prefix);
	}

}