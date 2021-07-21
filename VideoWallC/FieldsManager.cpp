#include <memory>
#include "FieldsManager.h"
#include "Log.h"

FieldsManager::FieldsManager(const std::string& devname, unsigned int baud_rate, INIFile* settingsObject) :
	m_running(true),
	m_readBufferSize(0),
	m_fieldsArray(),
	m_displayFallback(true),
	m_fallbackTexture(),
	m_fallbackSprite(),
	m_internalClock(),
	m_textRunningLastUpdateMicro(0),
	m_textRunningCurrentMicro(0),
	m_textRunningSpeed(30),
	m_textRunningUpdateEveryMicro((int)round(1000000 / m_textRunningSpeed))	
{
	LOG_TRACE("Field manager constructor enter");
	m_pSettings = settingsObject;
	std::string nameTemp = "\\\\.\\" + devname;
	LOG_DEBUG("Fields manager device name = {}", nameTemp);
	m_serial.open(nameTemp, baud_rate,
		boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::even),
		boost::asio::serial_port_base::character_size(8),
		boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none),
		boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));

	//using namespace std::placeholders;
	//m_serial.setCallback(std::bind(&FieldsManager::recvCallback, this, _1, _2));
	//https://stackoverflow.com/questions/34094496/using-c-class-method-as-a-function-callback

	LOG_INFO("Field manager serial opened");

	LOG_TRACE("Initializing clocks");
	m_lastUpdated = m_internalClock.now() - std::chrono::seconds(m_pSettings->GetUInt(S_FALLBACKTIMEOUT)) * 2;

	LOG_TRACE("Creating sprite and texture for fallback");
	size_t wndX = m_pSettings->GetUInt(S_CUSTOMWIDTH);
	size_t wndY = m_pSettings->GetUInt(S_CUSTOMHEIGHT);
	if (m_fallbackTexture.loadFromFile(m_pSettings->GetString(S_FALLBACK)) == false)
	{
		LOG_ERROR("Failed to load fallback image, replacing with blue background");		

		sf::Image tempImage;
		tempImage.create(wndX, wndY, sf::Color::Blue);
		m_fallbackTexture.create(wndX, wndY);
		m_fallbackTexture.update(tempImage);
	}	
	m_fallbackSprite.setTexture(m_fallbackTexture);
	m_fallbackSprite.setPosition(0, 0);
	m_fallbackSprite.setScale(wndX / m_fallbackSprite.getLocalBounds().width, wndY / m_fallbackSprite.getLocalBounds().height);
	//spr.setScale(window.getSize().x / spr.getLocalBounds().width, window.getSize().y / spr.getLocalBounds().height);

	//std::thread t(boost::bind(&boost::asio::io_service::run, &io));
	std::thread t(&FieldsManager::workThreadFunction, this);
	workThread.swap(t);
	LOG_INFO("Launched manager thread");
	LOG_TRACE("Field manager constructor exit");
}

FieldsManager::~FieldsManager()
{
	LOG_TRACE("Manager destructor enter");
	//m_serial.clearCallback();
	m_serial.close();
	m_running.store(false);
	workThread.join();

	std::lock_guard<std::recursive_mutex> mut(m_fieldArrayMutex);
	std::for_each(m_fieldsArray.begin(), m_fieldsArray.end(), std::default_delete<LDPField>());
	m_fieldsArray.clear();
	LOG_TRACE("Manager destructor exit");
}

void FieldsManager::DrawFields(sf::RenderWindow & wnd)
{
	if (m_displayFallback == true)
	{
		wnd.draw(m_fallbackSprite);
	}
	else
	{
		std::lock_guard<std::recursive_mutex> mut(m_fieldArrayMutex);
		for (size_t i = 0; i < m_fieldsArray.size(); i++)
		{
			m_fieldsArray[i]->draw(wnd);
		}
	}
}

//bool FieldsManager::UpdateFields(sf::Time elapsed)
bool FieldsManager::UpdateFields(sf::Int64 elapsed, bool forceLog)
{
	if (forceLog) LOG_TRACE("UpdateFields enter, elapsed Parameter={0}, m_textRunningCurrentMicro={1}, m_textRunningUpdateEveryMicro={2}, m_textRunningLastUpdateMicro={3}", elapsed, m_textRunningCurrentMicro, m_textRunningUpdateEveryMicro, m_textRunningLastUpdateMicro);
	std::lock_guard<std::recursive_mutex> mut(m_fieldArrayMutex);
	bool returnValue = false;
	m_textRunningCurrentMicro += elapsed;
	if (forceLog) LOG_TRACE("New m_textRunningCurrentMicro={0}", m_textRunningCurrentMicro);
	bool needUpdate = (m_textRunningCurrentMicro >= m_textRunningLastUpdateMicro + m_textRunningUpdateEveryMicro);
	if (forceLog) LOG_TRACE("needUpdate={0}, FieldArraySize={1}, returnValue={2}", needUpdate, m_fieldsArray.size(), returnValue);
	for (size_t i = 0; i < m_fieldsArray.size(); i++)
	{		
		if (m_fieldsArray[i]->update(elapsed, needUpdate) == true)
		{
			if (forceLog) LOG_TRACE("Field #{0} updated=true", i);
			returnValue = true;
		}
		else
		{
			if (forceLog) LOG_TRACE("Field #{0} updated=false", i);
		}
	}

	if (needUpdate) m_textRunningLastUpdateMicro += m_textRunningUpdateEveryMicro;
	//if (returnValue == true) m_textRunningLastUpdateMicro = m_textRunningCurrentMicro;
	if (forceLog) LOG_TRACE("Before exit, elapsed Parameter={0}, m_textRunningCurrentMicro={1}, m_textRunningUpdateEveryMicro={2}, m_textRunningLastUpdateMicro={3}, returnValue={4}", elapsed, m_textRunningCurrentMicro, m_textRunningUpdateEveryMicro, m_textRunningLastUpdateMicro, returnValue);
	return returnValue;
}

void FieldsManager::ResetTimers()
{
	LOG_TRACE("ResetTimers enter");
	m_textRunningCurrentMicro = 0;
	m_textRunningLastUpdateMicro = 0;
}

void FieldsManager::ExecuteExternalCommand(std::string command)
{
	std::lock_guard<std::recursive_mutex> mut(m_fieldArrayMutex);
	m_commandBuffer.push_back(command);
	//m_readQueueBuffer.append(command);
}

bool FieldsManager::CheckBufferForPackets(size_t & start, size_t & finish)
{
	LOG_TRACE("Entered CheckBufferForPackets with buffer={}", m_readQueueBuffer);
	const unsigned char startCode = 2;
	const unsigned char endCode = 3;

	if (m_readQueueBuffer.size() == 0) return false;

	size_t first = m_readQueueBuffer.find_first_of(startCode, 0);
	while (first != std::string::npos)
	{
		//if what we found is not 0, then there is garbage in fron of string, deleting
		if (first != 0) 
		{
			m_readQueueBuffer.erase(0, first);
			first = m_readQueueBuffer.find_first_of(startCode, 0);
			LOG_DEBUG("Deleted trash in front of buffer, resulting buffer={}", m_readQueueBuffer);
			continue;
		}
		//here we found start at 0, looking for end
		size_t end = m_readQueueBuffer.find_first_of(endCode, 0);

		if (end == std::string::npos) return false;

		//checking if this packet is for us
		//checking minimal length
		int len = end - first + 1;
		if (len < 8)
		{
			LOG_DEBUG("Packet is too small, deleting");
			m_readQueueBuffer.erase(0, end + 1);
			first = m_readQueueBuffer.find_first_of(startCode, 0);
			LOG_DEBUG("resulting buffer={}", m_readQueueBuffer);
			continue;
		}
		//checking adress
		std::string t = m_readQueueBuffer.substr(1, 2);
		int addr;
		std::istringstream(t) >> std::hex >> addr;
		if (addr != m_pSettings->GetUInt(S_TABLONUMBER))
		{
			LOG_DEBUG("Packet is not for us, deleting");
			m_readQueueBuffer.erase(0, end + 1);
			first = m_readQueueBuffer.find_first_of(startCode, 0);
			LOG_DEBUG("resulting buffer={}", m_readQueueBuffer);
			continue;
		}
		//check checksum
		//todo

		start = first;
		finish = end;
		return true;
	}

	if (m_readQueueBuffer.size() != 0) m_readQueueBuffer.clear();
	
	return false;
}

bool FieldsManager::CheckPacketCRC(std::string packet)
{
	LOG_TRACE("CheckPacketCRC enter");
	unsigned char CRC = 0;

	for (size_t i = 1; i < packet.size() - 3; i++)
	{
		CRC ^= packet[i];
	}
	CRC ^= 0xFF;
	
	std::string t = packet.substr(packet.size() - 3, 2);
	unsigned char packetCRC = HexToInt(t);	

	if (CRC == packetCRC) return true;

	LOG_ERROR("Pakcet CRC does not match, packetCRC={0}, calculated CRC={1}", packetCRC, CRC);
	return false;
}

size_t FieldsManager::CheckFieldIntersects(sf::FloatRect& rect)
{
	LOG_TRACE("CheckFieldIntersects enter with rect={0},{1},{2},{3}", rect.left, rect.left + rect.width, rect.top, rect.top + rect.height);

	if (rect.left + rect.width - 1 > m_pSettings->GetUInt(S_CUSTOMWIDTH) ||
		rect.top + rect.height - 1 > m_pSettings->GetUInt(S_CUSTOMHEIGHT)) return UINT_MAX - 2;

	for (size_t i = 0; i < m_fieldsArray.size(); i++)
	{
		if (rect.intersects(m_fieldsArray[i]->getBounds()) == true)
		{
			if (m_fieldsArray[i]->getBounds() == rect)
			{
				LOG_DEBUG("Field fully intersects with another field");
				return i;
			}
			LOG_DEBUG("Field intersects with another field");
			return UINT_MAX;
		}
	}
	LOG_DEBUG("Field doesn't intersects, good to create");
	return UINT_MAX - 1;
}

void FieldsManager::SplitPacketToCommands(std::string packet)
{
	LOG_TRACE("SplitPacketToCommands enter");
	//checking size just in case
	size_t pSize = packet.size();
	if (pSize < 8) return;

	//checking packet for crc
	if (CheckPacketCRC(packet) == false)
	{
		LOG_CRITICAL("Packet CRC does not match, ignoring packet");
		return;
	}

	//stripping packet from start and end
	packet.erase(pSize - 3);  //deleting 3 characters at end (CRC and endChar)
	packet.erase(0, 5);  //deleting 5 character ar start (startChar, address, packet length)
	LOG_DEBUG("Packet after stripping ends={}", packet);

	//checking size again
	if (packet.size() == 0) return;

	const unsigned char commandStart = '%';
	size_t pos = packet.find_first_of(commandStart);
	while (pos != std::string::npos)
	{
		size_t pos2 = packet.find_first_of(commandStart, pos + 1);
		if (pos2 == std::string::npos)
		{
			m_commandBuffer.push_back(packet.substr(pos));
			packet.erase(pos);
		}
		else
		{
			m_commandBuffer.push_back(packet.substr(pos, pos2 - pos));
			packet.erase(pos, pos2 - pos);
		}
		pos = packet.find_first_of(commandStart);
	}
}

//return values:
//0 - sucsess
//1 - fields intersection
//2 - unknown command
int FieldsManager::ExecuteCommands()
{
	LOG_TRACE("ExecuteCommands enter");

	int retValue = 0;
	//concat text fields together
	size_t commandIndex = 0;
	while (commandIndex < (m_commandBuffer.size() - 1))
	{
		if (m_commandBuffer[commandIndex].find("%0") == 0 &&
			m_commandBuffer[commandIndex + 1].find("%1") == 0) 
		{
			m_commandBuffer[commandIndex].append(m_commandBuffer[commandIndex + 1]);
			m_commandBuffer.erase(m_commandBuffer.begin() + commandIndex + 1);
		}
		commandIndex++;
	}

	LOG_DEBUG("Command buffer log after concat:");
	for (size_t i = 0; i < m_commandBuffer.size(); i++)
	{
		LOG_DEBUG("Element #{0}={1}", i, m_commandBuffer[i]);
	}

	//execute commands
	commandIndex = 0;
	std::lock_guard<std::recursive_mutex> mut(m_fieldArrayMutex);
	//m_fieldArrayMutex.lock();
	while (commandIndex < m_commandBuffer.size())
	{
		std::string command = m_commandBuffer[commandIndex];
		LOG_DEBUG("Executing command #{0}={1}", commandIndex, command);
		int ret = ParseAndExecuteCommand(command);
		//if ( == true)
		{
			LOG_DEBUG("Erasing command #{0}", commandIndex);
			m_commandBuffer.erase(m_commandBuffer.begin() + commandIndex);
			LOG_DEBUG("Updating last update timer");
			if (command != "%35")  m_lastUpdated = m_internalClock.now();
			if (ret > retValue) retValue = ret;
			continue;
		}
		//commandIndex++;
	}

	return retValue;
}

//return values:
//0 - sucsess
//1 - fields intersection
//2 - unknown command
int FieldsManager::ParseAndExecuteCommand(std::string command)
{
	LOG_TRACE("ParseAndExecuteCommand enter with command={}", command);

	if (command[0] != '%') return 2;   //if % is not at the start - skip
	LOG_DEBUG("Command length={}", command.length());
	if (command.length() < 3) return 2;  //if command is too short - skip

	unsigned char majorMode = command[1];
	unsigned char minorMode = command[2];

	switch (majorMode)
	{
	case '0': return ParseAndExecuteTextField(command);
	case '1': return 0;  //there should be no text declaration fields
	case '2': 
		switch (minorMode)
		{
		case '3': DeleteAllFields(); return 0;  //%23 deleting all fields
		default: return 2; break;
		}
	case '3':
		switch (minorMode)
		{
		case '0': ExecuteTimeChange(command); return 0;   //%30  time sync
		case '5': DeleteAndFallback(); return 0;  //%35 display default fallback
		case '9': return 0; break;  //%39 system reset ?
		default:return 2; break;
		}
	case '4':
		switch (minorMode)
		{
		case '0':  //%40 white horizontal line
		case '1':  //%41 white vertical line
		case '2':  //%42 white rectangle
		case '3':  //%43 colored horizontal line
		case '4':  //%44 colored vertical line
		case '5': return ParseAndExecuteRectangle(command); //%45 colored rectangle
		default:return 2; break;
		}
	case '7':
		switch (minorMode)
		{
		case 'c': break; //%7c datetime sync
		case 'h': break; //%7h define standard BG color
		case 'v': break; //%7v define standard foreground color (text, rects)
		default:return 2; break;
		}
	default: return 2; break;
	}

	return 0;
}

void FieldsManager::InsertCommand(std::string command)
{
	LOG_DEBUG("InsertCommand enter, inserting command={}", command);
	m_commandBuffer.push_back(command);
}

sf::Color FieldsManager::GetColorByIndex(uint32_t index)
{
	return sf::Color::White;
}

uint32_t FieldsManager::GetTransparencyByIndex(uint32_t index)
{
	const uint32_t transparencyArray[16] = {0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255};
	uint32_t t = index;
	if (t > 15) t = 15;
	return transparencyArray[t];
}

uint32_t FieldsManager::HexToInt(const std::string & str)
{		
	uint32_t value;
	std::istringstream(str) >> std::hex >> value;
	return value;
}

std::string FieldsManager::GetFormatstringByAttribute(const std::string & attribute)
{
	std::string returnValue = "";
	if (attribute.size() < 2) return returnValue;
	//https://en.cppreference.com/w/cpp/io/manip/put_time
	if (attribute == "10") returnValue = "%d.%m.%y";		//$u10
	if (attribute == "30") returnValue = "%H.%M:%S";		//$u30
	if (attribute == "3F") returnValue = "%H%1.%M%1:%S%2:";	//$u3F  custom type
	if (attribute == "40") returnValue = "%H%2:%M";			//$u40

	return returnValue;
}

//return values:
//0 - sucsess
//1 - fields intersection
//2 - unknown command
int FieldsManager::ParseAndExecuteTextField(std::string command)
{
	LOG_TRACE("ParseAndExecuteTextField enter with command={}", command);

	unsigned char majorMode = command[1];
	unsigned char minorMode = command[2];

	//checking if text definition is here
	size_t textCommandStart = command.find("%10");
	if (textCommandStart == std::string::npos) textCommandStart = command.find("%1u");
	if (textCommandStart == std::string::npos) return 2; 

	if (minorMode == '0')
	{
		LOG_DEBUG("Determined minor mode 0");
	}
	else if (minorMode == '4')
	{
		LOG_DEBUG("Determined minor mode 4");
		//4 koordinates
		//checking length of field definition
		if (textCommandStart != 16) return 2;

		//extracting coordinates to rect
		sf::FloatRect fieldRect;
		fieldRect.left = std::stof(command.substr(3, 3));
		fieldRect.top = std::stof(command.substr(9, 3));
		fieldRect.width = std::stof(command.substr(6, 3)) - fieldRect.left + 1;
		fieldRect.height = std::stof(command.substr(12, 3)) - fieldRect.top + 1;

		size_t intersectResult = CheckFieldIntersects(fieldRect);
		bool needFieldUpdate = false;
		if (intersectResult == UINT_MAX)
		{
			LOG_ERROR("Field intersects another field, ignoring");
			return 1;
		}
		else if (intersectResult == (UINT_MAX - 1))
		{
			LOG_DEBUG("Field doesn't intersect, need to create");
		}
		else if (intersectResult == (UINT_MAX - 2))
		{
			LOG_ERROR("Field is out of bounds, ignoring");
			return 1;
		}
		else
		{
			LOG_DEBUG("Field fully intersects");
			needFieldUpdate = true;
		}

		std::string textCommand = command.substr(textCommandStart);
		textCommand.erase(0, 3);
		uint32_t fontIndex = 2;  //$1
		uint32_t textBlinking = 0;  //$6
		uint32_t textAlligment = 3;  //$t
		sf::Color textColor = sf::Color::White;  //$f
		sf::Color textBGColor = sf::Color::Transparent;  //$h
		uint32_t textAlpha = textColor.a;  //$T
		uint32_t textBGAlpha = 255;  //$H
		std::string datetimeCommand = "";   //$u

		size_t searchIndex = textCommand.find_first_of('$');
		uint32_t par1;
		while (searchIndex != std::string::npos)
		{
			LOG_DEBUG("Text command={}", textCommand);
			if (searchIndex >= textCommand.size()) break;
			unsigned char atribute = textCommand[searchIndex + 1];

			switch (atribute)
			{
			case '1':  //$1...
				fontIndex = HexToInt(textCommand.substr(searchIndex + 2, 1));
				textCommand.erase(searchIndex, 3);
				break;
			case '6':  //$6...
				textBlinking = HexToInt(textCommand.substr(searchIndex + 2, 1));
				textCommand.erase(searchIndex, 3);
				break;
			case 't':  //$t...
				textAlligment = HexToInt(textCommand.substr(searchIndex + 2, 1));
				textCommand.erase(searchIndex, 3);
				break;
			case 'f':  //$f...				
				par1 = std::stoi(textCommand.substr(searchIndex + 2, 1));
				if (par1 == 0)
				{
					textColor = GetColorByIndex(HexToInt(textCommand.substr(searchIndex + 3, 2)));
					textCommand.erase(searchIndex, 5);
				}
				else
				{
					textColor.r = HexToInt(textCommand.substr(searchIndex + 3, 2));
					textColor.g = HexToInt(textCommand.substr(searchIndex + 5, 2));
					textColor.b = HexToInt(textCommand.substr(searchIndex + 7, 2));
					textCommand.erase(searchIndex, 9);
				}
				LOG_DEBUG("Found $f, searchIndex={0}, color={1}.{2}.{3}", searchIndex, textColor.r, textColor.g, textColor.b);
				break;
			case 'h':  //$h...				
				par1 = std::stoi(textCommand.substr(searchIndex + 2, 1));
				if (par1 == 0)
				{
					textBGColor = GetColorByIndex(HexToInt(textCommand.substr(searchIndex + 3, 2)));
					textCommand.erase(searchIndex, 5);
				}
				else
				{
					textBGColor.r = HexToInt(textCommand.substr(searchIndex + 3, 2));
					textBGColor.g = HexToInt(textCommand.substr(searchIndex + 5, 2));
					textBGColor.b = HexToInt(textCommand.substr(searchIndex + 7, 2));
					textCommand.erase(searchIndex, 9);
				}
				LOG_DEBUG("Found $h, searchIndex={0}, color={1}.{2}.{3}", searchIndex, textBGColor.r, textBGColor.g, textBGColor.b);
				break;
			case 'T':  //$T...
				textAlpha = GetTransparencyByIndex(HexToInt(textCommand.substr(searchIndex + 2, 1)));
				textCommand.erase(searchIndex, 3);
				break;
			case 'H':  //$H...
				textBGAlpha = GetTransparencyByIndex(HexToInt(textCommand.substr(searchIndex + 2, 1)));
				textCommand.erase(searchIndex, 3);
				break;
			case 'u':
				datetimeCommand = textCommand.substr(searchIndex + 2, 2);
				textCommand.erase(searchIndex, 4);
				break;
			default:
				textCommand.erase(searchIndex, 3);
				break;
			}
			searchIndex = textCommand.find_first_of('$');
		}

		textColor.a = textAlpha;
		if (textBGColor.r != 0 || textBGColor.g != 0 || textBGColor.b != 0) textBGColor.a = textBGAlpha;

		if (needFieldUpdate == false)
		{
			//no field intersection, creating field
			LOG_DEBUG("Creating new field with text={}", textCommand);				
			LDPField* field = new LDPField();
			/*if (m_fieldsArray.size() > 0)
			{
				field->setCurrentMicro(m_fieldsArray[0]->getCurrentMicro());
				field->setLastUpdateMicro(m_fieldsArray[0]->getLastUpdateMicro());
			}*/
			field->setFont(m_pSettings->GetString(S_DEFAULTFONT));
			LOG_DEBUG("Field font={}", m_pSettings->GetString(S_DEFAULTFONT));
			field->setBGColor(textBGColor);
			LOG_DEBUG("Field BGColor={0}.{1}.{2} a={3}", textBGColor.r, textBGColor.g, textBGColor.b, textBGColor.a);
			field->setBounds(fieldRect);
			field->setTextSize(fontIndex + 1);
			LOG_DEBUG("Field fontsize={}", field->getTextSize());
			field->setTextStyle(sf::Text::Style::Regular);
			field->setTextColor(textColor);
			LOG_DEBUG("Field TextColor={0}.{1}.{2} a={3}", textColor.r, textColor.g, textColor.b, textColor.a);
			//std::string strTemp = command.substr(textCommandStart + 3);
			field->setTextString(textCommand);
			LDPField::DisplayType ali = LDPField::DisplayType::OptionalLeft;
			if (textAlligment == 1) ali = LDPField::DisplayType::RightAlign;
			else if (textAlligment == 2) ali = LDPField::DisplayType::CenterAlign;
			if (datetimeCommand != "")
			{
				std::string formatTemp = GetFormatstringByAttribute(datetimeCommand);
				field->setFormatString(formatTemp);
				ali = LDPField::DisplayType::DateTime;
				LOG_DEBUG("Field format string={}", formatTemp);
			}
			field->setDisplayType(ali);
			field->setTextSpeed(m_textRunningSpeed);
			m_fieldsArray.push_back(field);
			LOG_DEBUG("Added new field to field pool");			
		}
		else
		{
			//field fully intersect, need update
			LOG_DEBUG("Changing field with text={}", textCommand);
			//LOG_DEBUG("Field metrics need update before={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
			m_fieldsArray[intersectResult]->setFont(m_pSettings->GetString(S_DEFAULTFONT));
			//LOG_TRACE("Metrics update={}",m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
			LOG_DEBUG("Field font={}", m_pSettings->GetString(S_DEFAULTFONT));
			m_fieldsArray[intersectResult]->setBGColor(textBGColor);
			//LOG_TRACE("Metrics update={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
			LOG_DEBUG("Field BGColor={0}.{1}.{2} a={3}", textBGColor.r, textBGColor.g, textBGColor.b, textBGColor.a);
			m_fieldsArray[intersectResult]->setBounds(fieldRect);
			m_fieldsArray[intersectResult]->setTextSize(fontIndex + 1);
			//LOG_TRACE("Metrics update={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
			LOG_DEBUG("Field fontsize={}", m_fieldsArray[intersectResult]->getTextSize());
			m_fieldsArray[intersectResult]->setTextStyle(sf::Text::Style::Regular);
			m_fieldsArray[intersectResult]->setTextColor(textColor);
			//LOG_TRACE("Metrics update={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
			LOG_DEBUG("Field TextColor={0}.{1}.{2} a={3}", textColor.r, textColor.g, textColor.b, textColor.a);
			m_fieldsArray[intersectResult]->setTextString(textCommand);
			//LOG_TRACE("Metrics update={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
			LDPField::DisplayType ali = LDPField::DisplayType::OptionalLeft;
			if (textAlligment == 1) ali = LDPField::DisplayType::RightAlign;
			else if (textAlligment == 2) ali = LDPField::DisplayType::CenterAlign;
			if (datetimeCommand != "")
			{
				std::string formatTemp = GetFormatstringByAttribute(datetimeCommand);
				m_fieldsArray[intersectResult]->setFormatString(formatTemp);
				ali = LDPField::DisplayType::DateTime;
				//LOG_TRACE("Metrics update={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
				LOG_DEBUG("Field format string={}", formatTemp);
			}
			m_fieldsArray[intersectResult]->setDisplayType(ali);
			m_fieldsArray[intersectResult]->setTextSpeed(m_textRunningSpeed);
			//LOG_TRACE("Metrics update={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
			LOG_DEBUG("Updated field");
			//LOG_DEBUG("Field metrics need update after={}", m_fieldsArray[intersectResult]->getMetricsNeedUpdate());
		}
	}

	return 0;
}

//return values:
//0 - sucsess
//1 - fields intersection
//2 - unknown command
int FieldsManager::ParseAndExecuteRectangle(std::string command)
{
	LOG_TRACE("ParseAndExecuteRectangle enter");

	std::string commandType = command.substr(1, 2);

	sf::FloatRect fieldRect;
	sf::Color bgColor = sf::Color::White;
	size_t commandSize = command.size();
	uint32_t par1;

	if (commandType == "40")        //white horizontal line
	{
		if (commandSize != 13) return 2;

		float x1 = std::stof(command.substr(3, 3));
		float x2 = std::stof(command.substr(6, 3));

		fieldRect.left = std::min(x1, x2);
		fieldRect.width = std::max(x1, x2) - fieldRect.left;
		float thickness = std::stof(command.substr(12, 1));
		if (thickness < 1) thickness = 1;
		fieldRect.height = thickness;
		fieldRect.top = std::stof(command.substr(9, 3)) - thickness + 1;
	}
	else if (commandType == "41")   //white vertical line
	{
		if (commandSize != 13) return 2;

		float y1 = std::stof(command.substr(6, 3));
		float y2 = std::stof(command.substr(9, 3));

		fieldRect.top = std::min(y1, y2);
		fieldRect.height = std::max(y1, y2) - fieldRect.top + 1;
		fieldRect.left = std::stof(command.substr(3, 3));
		float thickness = std::stof(command.substr(12, 1));
		if (thickness < 1) thickness = 1;
		fieldRect.width = thickness;
	}
	else if (commandType == "42")   //white rectangle
	{
		if (commandSize != 15) return 2;

		fieldRect.top = std::stof(command.substr(9, 3));
		fieldRect.height = std::stof(command.substr(12, 3)) - fieldRect.top + 1;
		fieldRect.left = std::stof(command.substr(3, 3));
		fieldRect.width = std::stof(command.substr(6, 3)) - fieldRect.left + 1;
	}
	else if (commandType == "43")   //colored horizontal line
	{
		if ((commandSize != 21)&&(commandSize != 17)) return 2;

		float x1 = std::stof(command.substr(3, 3));
		float x2 = std::stof(command.substr(6, 3));

		fieldRect.left = std::min(x1, x2);
		fieldRect.width = std::max(x1, x2) - fieldRect.left;
		float thickness = std::stof(command.substr(12, 1));
		if (thickness < 1) thickness = 1;
		fieldRect.height = thickness;
		fieldRect.top = std::stof(command.substr(9, 3)) - thickness + 1;

		par1 = std::stoi(command.substr(13, 1));
		if (par1 == 0)
		{
			bgColor = GetColorByIndex(HexToInt(command.substr(14, 2)));			
		}
		else if (par1 == 1)
		{
			bgColor.r = HexToInt(command.substr(14, 2));
			bgColor.g = HexToInt(command.substr(16, 2));
			bgColor.b = HexToInt(command.substr(18, 2));
			bgColor.a = GetTransparencyByIndex(HexToInt(command.substr(20, 1)));
		}
	}
	else if (commandType == "44")   //colored vertical line
	{
		if ((commandSize != 21) && (commandSize != 17)) return 2;

		float y1 = std::stof(command.substr(6, 3));
		float y2 = std::stof(command.substr(9, 3));

		fieldRect.top = std::min(y1, y2);
		fieldRect.height = std::max(y1, y2) - fieldRect.top + 1;
		fieldRect.left = std::stof(command.substr(3, 3));
		float thickness = std::stof(command.substr(12, 1));
		if (thickness < 1) thickness = 1;
		fieldRect.width = thickness;

		par1 = std::stoi(command.substr(13, 1));
		if (par1 == 0)
		{
			bgColor = GetColorByIndex(HexToInt(command.substr(14, 2)));
		}
		else if (par1 == 1)
		{
			bgColor.r = HexToInt(command.substr(14, 2));
			bgColor.g = HexToInt(command.substr(16, 2));
			bgColor.b = HexToInt(command.substr(18, 2));
			bgColor.a = GetTransparencyByIndex(HexToInt(command.substr(20, 1)));
		}
	}
	else if (commandType == "45")   //colored rectangle
	{
		if ((commandSize != 19) && (commandSize != 23)) return 2;

		fieldRect.top = std::stof(command.substr(9, 3));
		fieldRect.height = std::stof(command.substr(12, 3)) - fieldRect.top + 1;
		fieldRect.left = std::stof(command.substr(3, 3));
		fieldRect.width = std::stof(command.substr(6, 3)) - fieldRect.left + 1;

		par1 = std::stoi(command.substr(15, 1));
		if (par1 == 0)
		{
			bgColor = GetColorByIndex(HexToInt(command.substr(16, 2)));
		}
		else if (par1 == 1)
		{
			bgColor.r = HexToInt(command.substr(16, 2));
			bgColor.g = HexToInt(command.substr(18, 2));
			bgColor.b = HexToInt(command.substr(20, 2));
			bgColor.a = GetTransparencyByIndex(HexToInt(command.substr(22, 1)));
		}
	}
	else return 2;

	//looking for field intersection
	size_t intersectResult = CheckFieldIntersects(fieldRect);
	if (intersectResult == UINT_MAX)
	{
		LOG_ERROR("Field intersects another field, ignoring");
		return 1;
	}
	else if (intersectResult == (UINT_MAX - 1))
	{
		LOG_DEBUG("Field doesn't intersect, need to create");
	}
	else if (intersectResult == (UINT_MAX - 2))
	{
		LOG_ERROR("Field is out of bounds, ignoring");
		return 1;
	}
	else
	{
		LOG_DEBUG("Field fully intersects, ignoring");
		return 0;
	}

	//no field intersection, creating field
	LOG_DEBUG("Creating new field with bounds=X{0},{1} Y{2},{3}", fieldRect.left, fieldRect.left + fieldRect.width, fieldRect.top, fieldRect.top + fieldRect.height);
	LDPField* field = new LDPField();
	/*if (m_fieldsArray.size() > 0)
	{
		field->setCurrentMicro(m_fieldsArray[0]->getCurrentMicro());
		field->setLastUpdateMicro(m_fieldsArray[0]->getLastUpdateMicro());
	}*/
	field->setFont(m_pSettings->GetString(S_DEFAULTFONT));
	field->setBGColor(bgColor);
	LOG_DEBUG("Field Color={0}.{1}.{2} a={3}", bgColor.r, bgColor.g, bgColor.b, bgColor.a);
	field->setBounds(fieldRect);
	field->setTextSize(5);
	field->setTextStyle(sf::Text::Style::Regular);
	field->setTextColor(sf::Color::White);
	field->setTextString("");
	field->setDisplayType(LDPField::DisplayType::LeftAlign);
	field->setTextSpeed(m_textRunningSpeed);
	m_fieldsArray.push_back(field);
	LOG_DEBUG("Added new field to field pool");

	return 0;
}

void FieldsManager::DeleteAllFields()
{
	LOG_TRACE("DeleteAllFields enter");

	std::lock_guard<std::recursive_mutex> mut(m_fieldArrayMutex);	
	LOG_TRACE("Fields to delete={}", m_fieldsArray.size());
	std::for_each(m_fieldsArray.begin(), m_fieldsArray.end(), std::default_delete<LDPField>());
	m_fieldsArray.clear();
}

void FieldsManager::DeleteAndFallback()
{
	LOG_TRACE("DeleteAndFallback enter");
	DeleteAllFields();
	m_lastUpdated = m_internalClock.now() - std::chrono::hours(24);
	m_displayFallback = true;
}

void FieldsManager::ExecuteTimeChange(std::string command)
{
	LOG_TRACE("ExecuteTimeChange enter");

	size_t commandSize = command.size();

	if (commandSize != 9) return;

	float hh = std::stof(command.substr(3, 2));
	float mm = std::stof(command.substr(5, 2));
	float ss = std::stof(command.substr(7, 2));
	
	LOG_DEBUG("Changing time with new time={0}h.{1}m.{2}s", hh, mm, ss);
	
	if((hh < 0) || (hh > 23) || (mm < 0) || (mm > 59) || (ss < 0) || (ss > 59))
	{
		LOG_ERROR("New time is wrong, aborting time change");
		return;
	}

	SYSTEMTIME time;
	GetLocalTime(&time);

	time.wHour = WORD(hh);
	time.wMinute = WORD(mm);
	time.wSecond = WORD(ss);

	if (SetLocalTime(&time) == false) LOG_ERROR("Time change failed");
}

void FieldsManager::CheckAndUpdateFallback()
{
	auto time = m_internalClock.now();
	auto elapsed = time - m_lastUpdated;

	size_t timeout = m_pSettings->GetUInt(S_FALLBACKTIMEOUT);
	if (timeout == 0)
	{
		//check only to transition from fallback to display
		if (m_displayFallback == true && elapsed < std::chrono::hours(24))
		{
			LOG_DEBUG("Was update on internal timer, hiding fallback");
			m_displayFallback = false;
		}
	}
	else
	{
		//check to transition from fallback to display
		if (m_displayFallback == true)
		{
			if (elapsed < std::chrono::seconds(timeout))
			{
				LOG_DEBUG("Was update on internal timer, hiding fallback");
				m_displayFallback = false;
			}
		}
		else  //check to transition from display to fallback
		{
			if (elapsed > std::chrono::seconds(timeout))
			{
				LOG_DEBUG("Was no update on internal timer during timeout, showing fallback");
				//m_displayFallback = true;
				DeleteAndFallback();
			}
		}
	}
}

void FieldsManager::MoveDataFromSerial()
{
	LOG_TRACE("Buffer copy from serial enter");
	size_t needToRead = m_serial.GetBytesToRead();
	std::string t = m_serial.readString();

	m_readQueueBuffer.append(t);
	m_readBufferSize = m_readQueueBuffer.size();
	size_t copied = t.size();
	LOG_INFO("Copyed {} bytes to internal buffer", copied);
	if (copied != needToRead)
	{
		LOG_CRITICAL("Wrong number of bytes copied, need to copy={0}, actually copied={1}", needToRead, copied);
	}
}

void FieldsManager::workThreadFunction()
{
	LOG_DEBUG("Work thread enter");

	//0200FD  good answer
	std::string m_goodAnswer;
	m_goodAnswer += (char)2;
	m_goodAnswer += "0200FD";
	m_goodAnswer += (char)3;
	//0202FF  unknown mode/sub mode answer
	std::string m_unknownModeAnswer;
	m_unknownModeAnswer += (char)2;
	m_unknownModeAnswer += "0202FF";
	m_unknownModeAnswer += (char)3;
	//0203FE  wrong field position answer
	std::string m_fieldPositionAnswer;
	m_fieldPositionAnswer += (char)2;
	m_fieldPositionAnswer += "0203FE";
	m_fieldPositionAnswer += (char)3;

	while (m_running.load())
	{
		try
		{
			if (m_serial.GetBytesToRead() > 0)
			{
				LOG_DEBUG("Ready to read some data from serial object");
				MoveDataFromSerial();
				LOG_DEBUG("Resulting internal buffer={}", m_readQueueBuffer);
			}

			if (m_readQueueBuffer.size() != 0)
			{
				size_t start, finish;
				if (CheckBufferForPackets(start, finish) == true)
				{
					std::string packet = m_readQueueBuffer.substr(start, finish - start + 1);
					m_readQueueBuffer.erase(start, finish - start + 1);
					LOG_DEBUG("Buffer after erasing current packet={}", m_readQueueBuffer);

					SplitPacketToCommands(packet);
				}
			}

			//work with commands
			if (m_commandBuffer.size() != 0)
			{
				LOG_DEBUG("Command buffer is not empty, logging:");
				for (size_t i = 0; i < m_commandBuffer.size(); i++)
				{
					LOG_DEBUG("Element #{0}={1}", i, m_commandBuffer[i]);
				}

				int tmp = ExecuteCommands();
				std::string answer;
				if (tmp == 0) answer = m_goodAnswer;
				else if (tmp == 1) answer = m_fieldPositionAnswer;
				else answer = m_unknownModeAnswer;
				LOG_DEBUG("Returning answer, string={0}", answer);
				m_serial.writeString(answer);
			}

			//check fallback
			CheckAndUpdateFallback();

			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		catch (std::exception& e)
		{
			LOG_CRITICAL("Exception in work thread with message: {0}", e.what());
			break;
		}
		catch (...)
		{
			LOG_CRITICAL("Exception of unknown type in work thread, exiting");
			break;
		}
	}
	LOG_DEBUG("Manager thread exit");
}