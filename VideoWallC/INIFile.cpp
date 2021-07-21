#include "INIFile.h"
#include "Log.h"
#include <iostream>
#include <fstream>

INIFile::INIFile(std::string fileName, int argc, char* argv[]):
	m_loadSucsessful(false)
{
	Init(argc, argv);
	LoadConfigFile(fileName);
}

void INIFile::LoadConfigFile(std::string fileName)
{
	const bool ALLOW_UNREGISTERED = true;

	po::options_description options;
	options.add(m_configOptions);

	std::ifstream cfg_file(fileName.c_str());
	if (cfg_file)
	{
		po::store(po::parse_config_file(cfg_file, options, ALLOW_UNREGISTERED), m_vm);
		po::notify(m_vm);

		m_loadSucsessful = true;		
	}
	else LOG_CRITICAL("Config file does not exists, using defaults");
}

void INIFile::LogConfigOptions(std::stringstream & stream)
{
	stream << m_configOptions;
}

bool INIFile::GetLoadStatus()
{
	return m_loadSucsessful;
}

unsigned int INIFile::GetUInt(std::string name)
{
	if (m_vm.count(name))
		return m_vm[name].as<unsigned int>();
	else
		return 0;
}

std::string INIFile::GetString(std::string name)
{
	if (m_vm.count(name))
		return m_vm[name].as<std::string>();
	else
		return 0;
}

bool INIFile::GetBool(std::string name)
{
	if (m_vm.count(name))
		return m_vm[name].as<bool>();
	else
		return 0;
}


void INIFile::Init(int argc, char* argv[])
{
	m_configOptions.add_options()
		(S_LOGLEVEL, po::value<unsigned int>()->default_value(0), "Logging level (0-6). 6 is off")
		(S_DISPLAYCONSOLE, po::value<bool>()->default_value(false), "Enabling displaying the program console")
		(S_TARGETFPS, po::value<unsigned int>()->default_value(200), "Target FPS for program")
		(S_CUSTOMENABLED, po::value<bool>()->default_value(false), "Enabling custom window to create in a custom location")
		(S_CUSTOMLEFT, po::value<unsigned int>()->default_value(100), "Custom window settings. Left coordinate in pixels")
		(S_CUSTOMTOP, po::value<unsigned int>()->default_value(100), "Custom window settings. Top coordinate in pixels")
		(S_CUSTOMWIDTH, po::value<unsigned int>()->default_value(300), "Custom window settings. Width in pixels")
		(S_CUSTOMHEIGHT, po::value<unsigned int>()->default_value(300), "Custom window settings. Width in pixels")
		(S_TABLONUMBER, po::value<unsigned int>()->default_value(5), "Display number in LDP protocol")
		(S_COMPORT, po::value<std::string>()->default_value("COM1"), "Serial port number")
		(S_FALLBACK, po::value<std::string>()->default_value("fallback.jpg"), "Fallback image to use when not active")
		(S_DISPLAYFPS, po::value<bool>()->default_value(true), "Enabling displaying of FPS")
		(S_FALLBACKTIMEOUT, po::value<unsigned int>()->default_value(180), "Fallback timeout after witch change picture to fallback image")

		(S_DEFAULTFONT, po::value<std::string>()->default_value("arial.ttf"), "Default font to use in display form")
		;

	po::store(po::parse_command_line(argc, argv, m_configOptions), m_vm);
	po::notify(m_vm);	
}
