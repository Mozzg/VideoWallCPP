#include "FieldsManager.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#include <iostream>

#include "Log.h"
#include "INIFile.h"

int main(int argc, char* argv[])
{
	setlocale(LC_ALL, "");                                
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
	std::locale::global(std::locale(""));

	try {
		//Initialize log system
		vw::Log::Init();
		LOG_WARN("============================================");
		LOG_WARN("Program started");

		//initialize config system
		LOG_INFO("Reading config file");
		if (argc < 1)
		{
			LOG_CRITICAL("Number of arguments is less then 1, exiting");
			return 1;
		}
		fs::path configPath = argv[0];
		configPath.replace_extension(".ini");

		INIFile settings(configPath.string(), argc, argv);
		LOG_INFO("After creating ini file variable");

		if (settings.GetLoadStatus() == false)
			LOG_CRITICAL("Settings file load failed");
		else
			LOG_DEBUG("Settings file load sucsess");

		//apply settings
		LOG_INFO("Applying settings");
		vw::Log::SetLogLevel((spdlog::level::level_enum)settings.GetUInt(S_LOGLEVEL));
		if (settings.GetBool(S_DISPLAYCONSOLE) == false)
		{
			LOG_DEBUG("Display console is off in settings, hiding console");
			ShowWindow(GetConsoleWindow(), SW_HIDE);
		}

		//logging parameters
		{
			LOG_DEBUG("Settings description:");
			std::stringstream settingsStream;
			settings.LogConfigOptions(settingsStream);
			LOG_DEBUG("\n"+settingsStream.str());

			LOG_DEBUG("LogLevel={}", settings.GetUInt(S_LOGLEVEL));
			LOG_DEBUG("TargetFPS={}", settings.GetUInt(S_TARGETFPS));
			LOG_DEBUG("DisplayConsole={}", settings.GetBool(S_DISPLAYCONSOLE));
			LOG_DEBUG("CustomWindow={}", settings.GetBool(S_CUSTOMENABLED));
			LOG_DEBUG("TabloNumber={}", settings.GetUInt(S_TABLONUMBER));
			LOG_DEBUG("Comport={}", settings.GetString(S_COMPORT));
			LOG_DEBUG("Fallback={}", settings.GetString(S_FALLBACK));
			LOG_DEBUG("Fallback timeout={}", settings.GetUInt(S_FALLBACKTIMEOUT));
			LOG_DEBUG("Font={}", settings.GetString(S_DEFAULTFONT));
		}

		//initializing Comunication manager
		FieldsManager manager(settings.GetString(S_COMPORT), 19200, &settings);

		//initialize OpenGL system
		LOG_INFO("Initialize OpenGL");
		unsigned int wndW = settings.GetUInt(S_CUSTOMWIDTH);
		unsigned int wndH = settings.GetUInt(S_CUSTOMHEIGHT);
		sf::RenderWindow window(sf::VideoMode(wndW, wndH), "VideoWall", sf::Style::None);		
		sf::Vector2i pos = { 0, 0 };
		if (settings.GetBool(S_CUSTOMENABLED) == true)
		{
			pos.x = settings.GetUInt(S_CUSTOMLEFT);
			pos.y = settings.GetUInt(S_CUSTOMTOP);
		}
		window.setPosition(pos);		
		window.setMouseCursorVisible(false);
		window.requestFocus();
		sf::WindowHandle hndl = window.getSystemHandle();

		//initialize FPS counter
		LOG_INFO("Initialize fps counter");
		bool displayFPS = settings.GetBool(S_DISPLAYFPS);
		uint32_t fpsCount = 0;
		double fpsTimeElapsed = 0;
		uint32_t fpsDrawCalls = 0;
		sf::Text fpsCounterText;
		sf::Font fpsFont;
		if (fpsFont.loadFromFile(settings.GetString(S_DEFAULTFONT)) == true)
		{
			fpsCounterText.setFont(fpsFont);
			fpsCounterText.setCharacterSize(30);
			fpsCounterText.setFillColor(sf::Color::White);
			fpsCounterText.setString(L"000");
			sf::FloatRect fpsPos = fpsCounterText.getGlobalBounds();
			fpsCounterText.setPosition(window.getSize().x - fpsPos.width - 10, 0);
			LOG_DEBUG("Font initialization for fps counter complete");
		}
		else
		{
			displayFPS = false;
			LOG_ERROR("ERROR!!! Font initialization for fps counter failed");
		}		

		//initializing FPS limit
		if (settings.GetUInt(S_TARGETFPS) != 0)
		{			
			if (settings.GetUInt(S_TARGETFPS) == 60)
			{
				window.setVerticalSyncEnabled(true);
				window.setFramerateLimit(0);
			}
			else
			{
				window.setVerticalSyncEnabled(false);
				window.setFramerateLimit(settings.GetUInt(S_TARGETFPS));
			}
			LOG_DEBUG("Applied FPS limit to window");
		}	

		//initialize main clock
		//sf::Clock clock;
		LARGE_INTEGER clockFreq;	
		LARGE_INTEGER clockMainNow;
		LARGE_INTEGER clockMainPrev;
		LARGE_INTEGER clockMainElapsed;

		QueryPerformanceFrequency(&clockFreq);
		QueryPerformanceCounter(&clockMainNow);
		//clockMainNow.QuadPart = clockMainNow.QuadPart + 9223371934370257118;
		clockMainPrev.QuadPart = clockMainNow.QuadPart;
		clockMainElapsed.QuadPart = 0;
		double clockReverseFreq = 1000000.0 / clockFreq.QuadPart;

		//sf::Uint64 timeNow = static_cast<sf::Uint64>(clockMain.QuadPart * 1000000 * clockReverseFreq);
		//sf::Uint64 timePrev = timeNow;
		//sf::Int64 timeElapsed = 0;
		sf::Uint64 elapsedMicro = 0;

		//initialize timer to set window to foreground
		//sf::Time foregroundElapsed;
		//const sf::Time FOREGROUND_TIMEOUT = sf::seconds(60);	
		sf::Int64 foregroundElapsed = 0;  //microseconds
		const sf::Int64 FOREGROUND_TIMEOUT = 60 * 1000000;  //microseconds

		//initialize timer to force screen update
		//sf::Time forceRedrawElapsed;
		//const sf::Time FORCEREDRAW_TIMEOUT = sf::seconds(1);
		sf::Int64 forceRedrawElapsed = 0;  //microseconds
		const sf::Int64 FORCEREDRAW_TIMEOUT = 1 * 1000000;  //microseconds

		//initialize timer to reset all timers
		//sf::Time forceResetTimersElapsed;
		//const sf::Time FORCERESETTIMERS_TIMEOUT = sf::seconds(1800);
		sf::Int64 forceResetTimersElapsed = 0;
		const sf::Int64 FORCERESETTIMERS_TIMEOUT = 1800 * 1000000;  //microseconds

#ifdef CUSTOM_DEBUGBUILD
		//manager.ExecuteExternalCommand("%040102500501004%10$1F$00$60$t3$f1FFFF00$h1FF0000$TF$HF$u3F");
		//manager.ExecuteExternalCommand("%040503501501704%10$17$00$60$t3$f1FFFFFF$h1003F00$TF$HFTest string проверка");
		manager.ExecuteExternalCommand("%040102501711904%10$17$00$60$t3$f1FFFFFF$h10000AF$TF$HFTest string проверка со всеми остановками, кроме: Малиновка");
		
		//manager.ExecuteExternalCommand("%040202500250354%10$17$00$60$t2$f1FFFFFF$h1003F00$TF$HFTest string проверка");

		/*
		manager.ExecuteExternalCommand("%040012560880964%10$17$00$60$t2$f1FF0000$h1000000$TF$HFКомпания ВИДОР");
		manager.ExecuteExternalCommand("%040010250010094%10$16$00$60$t3$f1FF8000$h1000000$TF$HF6401");
		manager.ExecuteExternalCommand("%040012560110194%10$17$00$60$t3$f1FF8000$h1000000$TF$HFсо всеми остановками");
		manager.ExecuteExternalCommand("%040271530010094%10$17$00$60$t3$f1FF8000$h1000000$TF$HFМОСКВА(ЯРОСЛАВСКИЙ ВОКЗАЛ)");
		manager.ExecuteExternalCommand("%041561870010094%10$17$00$60$t3$f1FF8000$h1000000$TF$HF05:05");
		manager.ExecuteExternalCommand("%042132240010094%10$17$00$60$t3$f1FF8000$h1000000$TF$HF1");
		manager.ExecuteExternalCommand("%040010250210294%10$16$00$60$t3$f100FFFF$h1000000$TF$HF6907");
		manager.ExecuteExternalCommand("%040012560310394%10$17$00$60$t3$f100FFFF$h1000000$TF$HFсо всеми остановками кроме: ТАЙНИНСКАЯ, ПЕРЛОВСКАЯ, ЛОСЬ, ЯУЗА, МАЛЕНКОВСКАЯ, МОСКВА-3");
		manager.ExecuteExternalCommand("%040271530210294%10$17$00$60$t3$f100FFFF$h1000000$TF$HFМОСКВА (ЯРОСЛАВСКИЙ ВОКЗАЛ)");
		manager.ExecuteExternalCommand("%041561870210294%10$17$00$60$t3$f100FFFF$h1000000$TF$HF05:15");
		manager.ExecuteExternalCommand("%040010250410494%10$16$00$60$t3$f1FF8000$h1000000$TF$HF6203");
		manager.ExecuteExternalCommand("%040012560510594%10$17$00$60$t3$f1FF8000$h1000000$TF$HFсо всеми остановками");
		manager.ExecuteExternalCommand("%040271530410494%10$17$00$60$t3$f1FF8000$h1000000$TF$HFМОСКВА (ЯРОСЛАВСКИЙ ВОКЗАЛ)");
		manager.ExecuteExternalCommand("%041561870410494%10$17$00$60$t3$f1FF8000$h1000000$TF$HF05:21");
		manager.ExecuteExternalCommand("%040010250610694%10$16$00$60$t3$f1FFFFFF$h1000000$TF$HF6301");
		manager.ExecuteExternalCommand("%040012560710794%10$17$00$60$t3$f1FFFFFF$h1000000$TF$HFс остановками: МЫТИЩИ, ЛОСИНООСТРОВСКАЯ, РОСТОКИНО");
		manager.ExecuteExternalCommand("%040271530610694%10$17$00$60$t3$f1FFFFFF$h1000000$TF$HFМОСКВА (ЯРОСЛАВСКИЙ ВОКЗАЛ)");
		manager.ExecuteExternalCommand("%041561870610694%10$17$00$60$t3$f1FFFFFF$h1000000$TF$HF05:37");
		*/

		//manager.ExecuteExternalCommand("%040011800030034%10$19$00$60$t3$f1FFFFFF$h1FF0000$TF$HF");
		//manager.ExecuteExternalCommand("%04207256001017u%1u$t2$19$f1FFFFFF$h1000000$TF$HF$u40");
		//manager.ExecuteExternalCommand("%040010290210304%10$17$00$60$t3$f1FFFFFF$h1FF0000$TF$HF№");
		//manager.ExecuteExternalCommand("%42001256059059");
		//manager.ExecuteExternalCommand("%45002004002004100FF00F");
		manager.ExecuteExternalCommand("%04222256061072u%1u$t3$18$f1FF8000$h1000000$TF$HF$u40%040012560880884%10$10$00$60$t3$f1FFFFFF$h1408080$TF$HF%040301570210304%10$17$00$60$t3$f1FFFFFF$h1FF0000$TF$HFСтанция назначения%041581910210304%10$17$00$60$t3$f1FFFFFF$h1FF0000$TF$HFОтпр");

		//manager.ExecuteExternalCommand("%30113025");

		
#endif // CUSTOM_DEBUGBUILD	

		//bool forceLogAfterSkip = false;
		//bool forceLogDraw = false;

		LOG_INFO("Entering main cycle");
		bool running = true;
		while (running)
		{
			bool forceLog = false;

			sf::Event event;
			while (window.pollEvent(event))
			{
				if (event.type == sf::Event::Closed)
				{
					running = false;
				}
				if (event.type == sf::Event::KeyPressed)
				{
					if (event.key.code == sf::Keyboard::Escape)
					{
						running = false;
					}
					/*if (event.key.code == sf::Keyboard::F1)
					{
						forceLog = true;
						forceLogAfterSkip = true;
					}
					if (event.key.code == sf::Keyboard::F8)
					{
						forceLog = true;
						forceLogAfterSkip = true;
						forceLogDraw = true;
					}*/
				}
			}	

			/*if (forceLog)
			{
				LOG_DEBUG("Force log is on, cycle start");
				if (forceLogDraw) LOG_DEBUG("Force draw is on");

				//LOG_DEBUG("Timers before QuerryCounter: timeNow={0}, timePrev={1}, timeElapsed={2}, elapsedMicro={3}, foregroundElapsed={4}, forceRedrawElapsed={5}, forceResetTimersElapsed={6}, fpsTimeElapsed={7}, clockMain={8}, clockFreq={9}, clockReverseFreq={10}", timeNow, timePrev, timeElapsed, elapsedMicro, foregroundElapsed, forceRedrawElapsed, forceResetTimersElapsed, fpsTimeElapsed, clockMain.QuadPart, clockFreq.QuadPart, clockReverseFreq);
				LOG_DEBUG("Timers before QuerryCounter: clockMainNow={0}, clockMainPrev={1}, clockMainElapsed={2}, elapsedMicro={3}, foregroundElapsed={4}, forceRedrawElapsed={5}, forceResetTimersElapsed={6}, fpsTimeElapsed={7}, clockFreq={8}, clockReverseFreq={9}", clockMainNow.QuadPart, clockMainPrev.QuadPart, clockMainElapsed.QuadPart, elapsedMicro, foregroundElapsed, forceRedrawElapsed, forceResetTimersElapsed, fpsTimeElapsed, clockFreq.QuadPart, clockReverseFreq);
			}*/

			//sf::Time elapsed = clock.restart();
			QueryPerformanceCounter(&clockMainNow);
			//clockMainNow.QuadPart = clockMainNow.QuadPart + 9223371934370257118;
			//timePrev = timeNow;
			//timeNow = static_cast<sf::Int64>(clockMain.QuadPart * 1000000 * clockReverseFreq);
			//timeElapsed = timeNow - timePrev;  //in microseconds			
			if (clockMainNow.QuadPart < clockMainPrev.QuadPart)
			{
				clockMainElapsed.QuadPart = 0;
				LOG_DEBUG("WARNING!!! clockMainElapsed is wrong, clockMainNow={0}, clockMainPrev={1}", clockMainNow.QuadPart, clockMainPrev.QuadPart);
			}
			else
			{
				clockMainElapsed.QuadPart = clockMainNow.QuadPart - clockMainPrev.QuadPart;
			}
			clockMainPrev.QuadPart = clockMainNow.QuadPart;

			//sf::Int64 elapsedMicro = elapsed.asMicroseconds();
			elapsedMicro = static_cast<sf::Uint64>(clockMainElapsed.QuadPart * clockReverseFreq);

			//move timers
			foregroundElapsed += elapsedMicro;
			forceRedrawElapsed += elapsedMicro;
			forceResetTimersElapsed += elapsedMicro;
			//fpsTimeElapsed += elapsed.asSeconds();
			fpsTimeElapsed += elapsedMicro / 1000000.0;

			/*if (forceLog)
			{
				//LOG_DEBUG("Timers after QuerryCounter: timeNow={0}, timePrev={1}, timeElapsed={2}, elapsedMicro={3}, foregroundElapsed={4}, forceRedrawElapsed={5}, forceResetTimersElapsed={6}, fpsTimeElapsed={7}", timeNow, timePrev, timeElapsed, elapsedMicro, foregroundElapsed, forceRedrawElapsed, forceResetTimersElapsed, fpsTimeElapsed);
				LOG_DEBUG("Timers after QuerryCounter: clockMainNow={0}, clockMainPrev={1}, clockMainElapsed={2}, elapsedMicro={3}, foregroundElapsed={4}, forceRedrawElapsed={5}, forceResetTimersElapsed={6}, fpsTimeElapsed={7}, clockFreq={8}, clockReverseFreq={9}", clockMainNow.QuadPart, clockMainPrev.QuadPart, clockMainElapsed.QuadPart, elapsedMicro, foregroundElapsed, forceRedrawElapsed, forceResetTimersElapsed, fpsTimeElapsed, clockFreq.QuadPart, clockReverseFreq);
				LOG_DEBUG("Timeouts: FOREGROUND_TIMEOUT={0}, FORCEREDRAW_TIMEOUT={1}, FORCERESETTIMERS_TIMEOUT={2}", FOREGROUND_TIMEOUT, FORCEREDRAW_TIMEOUT, FORCERESETTIMERS_TIMEOUT);
			}*/

			//reset all timers			
			if (forceResetTimersElapsed > FORCERESETTIMERS_TIMEOUT)
			{
				//LOG_DEBUG("Force timer reset timer enter, elapsedMicroseconds={0}, clockElapsedMicroseconds={1}", elapsedMicro, clock.getElapsedTime().asMicroseconds());
				foregroundElapsed = 0;
				forceRedrawElapsed = 0;
				forceResetTimersElapsed = 0;
				fpsTimeElapsed = 0;
				manager.ResetTimers();
			}

			//failed after 11 days and 4 hours
			bool updated = manager.UpdateFields(elapsedMicro, forceLog);
			//if (forceLog) LOG_DEBUG("Before skip condition, updated={0}, forceRedrawElapsed={1}, FORCEREDRAW_TIMEOUT={2}", updated, forceRedrawElapsed, FORCEREDRAW_TIMEOUT);
			//if (forceLogDraw == false) {
				if ((updated == false) && (forceRedrawElapsed < FORCEREDRAW_TIMEOUT))					
				{
					if (forceLog) LOG_DEBUG("Got into skip condition");
					std::this_thread::yield();
					continue;
				}
			//}

			//if (forceLogAfterSkip) LOG_DEBUG("Got after skip, timeNow={0}, timePrev={1}, timeElapsed={2}, elapsedMicro={3}, foregroundElapsed={4}, forceRedrawElapsed={5}, forceResetTimersElapsed={6}, fpsTimeElapsed={7}", timeNow, timePrev, timeElapsed, elapsedMicro, foregroundElapsed, forceRedrawElapsed, forceResetTimersElapsed, fpsTimeElapsed);
			//if (forceLogAfterSkip) LOG_DEBUG("Got after skip, clockMainNow={0}, clockMainPrev={1}, clockMainElapsed={2}, elapsedMicro={3}, foregroundElapsed={4}, forceRedrawElapsed={5}, forceResetTimersElapsed={6}, fpsTimeElapsed={7}, clockFreq={8}, clockReverseFreq={9}", clockMainNow.QuadPart, clockMainPrev.QuadPart, clockMainElapsed.QuadPart, elapsedMicro, foregroundElapsed, forceRedrawElapsed, forceResetTimersElapsed, fpsTimeElapsed, clockFreq.QuadPart, clockReverseFreq);

			//restart redraw timer
			if (forceRedrawElapsed >= FORCEREDRAW_TIMEOUT) forceRedrawElapsed -= FORCEREDRAW_TIMEOUT;

			//move to foreground timer			
			if (foregroundElapsed > FOREGROUND_TIMEOUT)
			{
				LOG_DEBUG("Set to foreground timer enter");
				foregroundElapsed -= FOREGROUND_TIMEOUT;
				//window.requestFocus();
				SetWindowPos(hndl, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
				SetForegroundWindow(hndl);
				SetActiveWindow(hndl);
				RedrawWindow(hndl, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
				LOG_DEBUG("Set to foreground timer exit");
			}

			//if (forceLogAfterSkip) LOG_DEBUG("Before drawing");

			window.clear();	
			manager.DrawFields(window);
			fpsDrawCalls++;

			//fpscounter
			if (displayFPS)
			{							
				if (fpsTimeElapsed > 1)
				{
					fpsTimeElapsed -= 1;
					fpsCounterText.setString(std::to_wstring(fpsDrawCalls));					
					fpsDrawCalls = 0;
				}
				window.draw(fpsCounterText);
			}

			window.display();	

			//if (forceLogAfterSkip) LOG_DEBUG("After drawing");

			//forceLogAfterSkip = false;
			//forceLogDraw = false;
		}
		LOG_INFO("Main cicle exit");
		//show console to know when program ends
		ShowWindow(GetConsoleWindow(), SW_SHOW);
		window.close();
	}
	catch (std::exception& e)
	{
		LOG_CRITICAL("Exception in main with message: {0}", e.what());
		return 1;
	}
	catch (...)
	{
		LOG_CRITICAL("Exception of unknown type in main, exiting");
		return 1;
	}
    
	//system("pause");

	LOG_WARN("Program exiting normally");
	return 0;
}
