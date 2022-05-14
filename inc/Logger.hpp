#pragma once
#include <fstream>
#include <string>
#include <ctime>
#include "KeyboardInputLog.hpp"
#include <iomanip>


class Logger
{
	std::ofstream out;
	std::string logsPath;
	std::tm last_log_date;

	KeyboardInputLog current_input;

	//const Logger& operator=(const Logger&) = delete;

	public:

	Logger(): out("", std::ios::out) {}
	Logger(std::string lp) : logsPath(lp), out("", std::ios::out) {}

	template <class Loggable>
	void log(const Loggable& loggable, const std::string& tag = "Info", std::time_t time = std::time(nullptr))
	{
		std::tm tm;
		localtime_s(&tm, &time);
		
		std::locale lSY("");
        out.imbue(lSY);

		if (!out.good() || tm.tm_year != last_log_date.tm_year || tm.tm_yday != last_log_date.tm_yday)
		{
			out.close();
			out.clear();

			std::stringstream ss;
			ss << logsPath << "\\" << std::put_time(&tm, "%F");

			std::filesystem::create_directories(ss.str());

			out.open(ss.str() + "\\" + "activity.log" , std::ios_base::app );
		}

		if ((void*)&loggable != (void*)&current_input && !current_input.empty())
		{
			// should log current input before
			log(current_input, "Input", current_input.time);
			current_input.clear();
		}

        out << "[" << std::put_time(&tm, "%FT%T%z") << "]"
            << "[" << tag << "] " 
			<< loggable
			<< std::endl;

		last_log_date = tm;
	}

	void logKeyboardInputItem(const KeyboardInputItem& item)
	{
		if (current_input.empty())
			current_input.set_time(std::time(nullptr));

		current_input.push(item);

		if (current_input.size() >= 60)
		{
			log(current_input, "Input", current_input.time);
			current_input.clear();
		}
	}
};