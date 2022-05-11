#pragma once

#include <ostream>
#include <string>
#include <ctime>
#include "KeyboardInputLog.hpp"

class Logger
{
	std::ostream& out;

	KeyboardInputLog current_input;

	const Logger& operator=(const Logger&) = delete;

	public:

	Logger(std::ostream& output_stream) : out(output_stream) {}


	template <class Loggable>
	void log(const Loggable& loggable, const std::string& tag = "Info", std::time_t time = std::time(nullptr))
	{
		if ((void*)&loggable != (void*)&current_input && !current_input.empty())
		{
			// should log current input before
			log(current_input, "Input", current_input.time);
			current_input.clear();
		}

        std::tm tm;
		
		localtime_s(&tm, &time);
        std::locale lSY("");
        out.imbue(lSY);

        out << "[" << std::put_time(&tm, "%FT%T%z") << "]"
            << "[" << tag << "] " 
			<< loggable
			<< std::endl;
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