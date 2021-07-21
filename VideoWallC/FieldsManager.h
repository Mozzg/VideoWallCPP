#pragma once
#include <thread>
#include <mutex>

//#include "AsyncSerial.h"
#include "BufferedAsyncSerial.h"
#include "LDPField.h"
#include "INIFile.h"

class FieldsManager
{
public:
	FieldsManager(const std::string& devname, unsigned int baud_rate, INIFile* settingsObject);
	~FieldsManager();

	void DrawFields(sf::RenderWindow& wnd);
	//bool UpdateFields(sf::Time elapsed);
	bool UpdateFields(sf::Int64 elapsed, bool forceLog);
	void ResetTimers();

	void ExecuteExternalCommand(std::string command);

	std::thread workThread;
	std::atomic_bool m_running;
private:
	void MoveDataFromSerial();
	bool CheckBufferForPackets(size_t& start, size_t& finish);	

	void SplitPacketToCommands(std::string packet);
	int ExecuteCommands();
	int ParseAndExecuteCommand(std::string command);
	void InsertCommand(std::string command);

	bool CheckPacketCRC(std::string packet);
	size_t CheckFieldIntersects(sf::FloatRect& rect);
	sf::Color GetColorByIndex(uint32_t index);
	uint32_t GetTransparencyByIndex(uint32_t index);
	uint32_t HexToInt(const std::string& str);
	std::string GetFormatstringByAttribute(const std::string& attribute);

	int ParseAndExecuteTextField(std::string command);
	int ParseAndExecuteRectangle(std::string command);
	void DeleteAllFields();
	void DeleteAndFallback();
	void ExecuteTimeChange(std::string command);

	void CheckAndUpdateFallback();

	void workThreadFunction();
		
	BufferedAsyncSerial m_serial;
	std::string m_readQueueBuffer;
	size_t m_readBufferSize;

	std::vector<std::string> m_commandBuffer;
	std::vector<LDPField*> m_fieldsArray;
	std::recursive_mutex m_fieldArrayMutex;	

	sf::Int64 m_textRunningLastUpdateMicro;
	sf::Int64 m_textRunningCurrentMicro;
	const float m_textRunningSpeed;
	const sf::Int64 m_textRunningUpdateEveryMicro;

	//fallback members
	bool m_displayFallback;
	sf::Texture m_fallbackTexture;
	sf::Sprite m_fallbackSprite;
	std::chrono::steady_clock m_internalClock;
	std::chrono::steady_clock::time_point m_lastUpdated;

	INIFile* m_pSettings;
};

