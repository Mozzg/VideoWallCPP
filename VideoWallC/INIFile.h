#pragma once
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#define S_LOGLEVEL "Main.LogLevel"
#define S_DISPLAYCONSOLE "Main.DisplayConsole"
#define S_TARGETFPS "Main.TargetFPS"
#define S_CUSTOMENABLED "Main.CustomWindowEnabled"
#define S_CUSTOMLEFT "Main.CustomWindowLeft"
#define S_CUSTOMTOP "Main.CustomWindowTop"
#define S_CUSTOMWIDTH "Main.CustomWindowWidth"
#define S_CUSTOMHEIGHT "Main.CustomWindowHeight"
#define S_TABLONUMBER "Main.TabloNumber"
#define S_COMPORT "Main.ComPort"
#define S_FALLBACK "Main.FallBackImage"
#define S_FALLBACKTIMEOUT "Main.FallBackTimeout" 
#define S_DISPLAYFPS "Main.DisplayFPS"
#define S_DEFAULTFONT "Fonts.Default"

class INIFile
{
public:
	INIFile(std::string fileName, int argc, char* argv[]);

	void LoadConfigFile(std::string fileName);
	void LogConfigOptions(std::stringstream& stream);

	bool GetLoadStatus();

	unsigned int GetUInt(std::string name);
	std::string GetString(std::string name);
	bool GetBool(std::string name);
private:
	void Init(int argc, char* argv[]);

	po::variables_map m_vm;
	po::options_description m_configOptions;
	bool m_loadSucsessful;
};
