#include "LDPField.h"
#include "Log.h"
#include <sstream>
#include <iomanip>
#include <iostream>

LDPField::LDPField() :
m_bounds				(0, 0, 100, 10),
m_textColor				(255, 255, 255, 255),
m_bgColor				(0, 0, 0, 255),
m_textString			(),
m_textSize				(30),
m_textStyle				(sf::Text::Style::Regular),
m_textRunningSpeed		(60),
m_textRunningPosition	(0.0),
//m_textRunningLastUpdateMicro(0),
//m_textRunningCurrentMicro(0),
//m_textRunningUpdateEveryMicro((int)round(1000000 / m_textRunningSpeed)),
m_fontFileName			(),
m_fieldType				(DisplayType::LeftAlign),
m_formatString			("%d.%m.%Y %I:%M:%S"),
m_textObject			(),
m_fieldTexture			(),
m_drawSprite			(),
m_textFont				(),
m_TextWidth				(0),
m_textRunning			(false),
m_usingGoodFont			(false),
m_metricsNeedUpdate		(true),
m_elapsedSeconds		(0),
m_elapsedCount			(0)
{
	LOG_DEBUG("LDPField OnCreate enter");
}

LDPField::LDPField(sf::FloatRect rect, sf::String text) :
m_bounds				(rect),
m_textColor				(255, 255, 255, 255),
m_bgColor				(0, 0, 0, 255),
m_textString			(text),
m_textSize				(30),
m_textStyle				(sf::Text::Style::Regular),
m_textRunningSpeed		(60),
m_textRunningPosition	(0.0),
//m_textRunningLastUpdateMicro(0),
//m_textRunningCurrentMicro(0),
//m_textRunningUpdateEveryMicro((int)round(1000000 / m_textRunningSpeed)),
m_fontFileName			(),
m_fieldType				(DisplayType::LeftAlign),
m_formatString			("%d.%m.%Y %I:%M:%S"),
m_textObject			(),
m_fieldTexture			(),
m_drawSprite			(),
m_textFont				(),
m_TextWidth				(0),
m_textRunning			(false),
m_usingGoodFont			(false),
m_metricsNeedUpdate		(true),
m_elapsedSeconds		(0),
m_elapsedCount			(0)
{
	LOG_DEBUG("LDPField OnCreate enter");	
}

LDPField::LDPField(const LDPField & copy) :
m_bounds(copy.m_bounds),
m_textColor(copy.m_textColor),
m_bgColor(copy.m_bgColor),
m_textString(copy.m_textString),
m_textSize(copy.m_textSize),
m_textStyle(copy.m_textStyle),
m_textRunningSpeed(copy.m_textRunningSpeed),
m_textRunningPosition(copy.m_textRunningPosition),
//m_textRunningLastUpdateMicro(copy.m_textRunningLastUpdateMicro),
//m_textRunningCurrentMicro(copy.m_textRunningCurrentMicro),
//m_textRunningUpdateEveryMicro((int)round(1000000 / m_textRunningSpeed)),
m_fontFileName(copy.m_fontFileName),
m_fieldType(copy.m_fieldType),
m_formatString(copy.m_formatString),
m_textObject(copy.m_textObject),
m_fieldTexture(),
m_drawSprite(copy.m_drawSprite),
m_textFont(copy.m_textFont),
m_TextWidth(copy.m_TextWidth),
m_textRunning(copy.m_textRunning),
m_usingGoodFont(copy.m_usingGoodFont),
m_metricsNeedUpdate(true),
m_elapsedSeconds(copy.m_elapsedSeconds),
m_elapsedCount(0)
{
	LOG_DEBUG("LDPField OnCopy enter");
	EnsureMetricsUpdate();	
}

LDPField::~LDPField()
{
	LOG_DEBUG("LDPField OnDestroy enter");
}

void LDPField::draw(sf::RenderWindow & window)
{
	EnsureMetricsUpdate();

	if (m_usingGoodFont == false) return;

	//m_fieldTexture.clear(m_bgColor);
	//m_fieldTexture.draw(m_textObject);
	//m_fieldTexture.display();
	
	window.draw(m_drawSprite);
}

bool LDPField::update(sf::Int64 elapsedMicroseconds, bool needUpdateRunning)
{
	bool returnValue = false;
	if (m_metricsNeedUpdate == true) returnValue = true;
	EnsureMetricsUpdate();

	/*if (m_textRunning == true)
	{
		double offset = elapsedMicroseconds * (m_textRunningSpeed / 1000. / 1000.);
		//sf::Vector2f pos = m_textObject.getPosition();
		double pos = m_textRunningPosition;
		//std::cout << elapsedMicroseconds << '\n';

		pos += offset;
		//pos += 0.5;
		if (std::abs(pos) > m_TextWidth) pos -= m_TextWidth;
		m_textRunningPosition = pos;
		//pos.x = round(pos.x);

		sf::IntRect rect = m_drawSprite.getTextureRect();
		rect.left = (int)round(m_textRunningPosition);
		m_drawSprite.setTextureRect(rect);
		//m_textObject.setPosition(pos);
	}*/
	//m_textRunningCurrentMicro += elapsedMicroseconds;

	if (m_textRunning == true)
	{		
		//if (m_textRunningCurrentMicro >= (m_textRunningLastUpdateMicro + m_textRunningUpdateEveryMicro))
		//if (current >= (last + m_textRunningUpdateEveryMicro))
		if (needUpdateRunning == true)
		{
			//m_textRunningLastUpdateMicro += m_textRunningUpdateEveryMicro;			
			m_textRunningPosition += 1;
			if (std::abs(m_textRunningPosition) > m_TextWidth) m_textRunningPosition -= m_TextWidth;			
			sf::IntRect rect = m_drawSprite.getTextureRect();
			rect.left = (int)round(m_textRunningPosition);
			m_drawSprite.setTextureRect(rect);
			returnValue = true;
			//LOG_TRACE("TextRunningPosition={}", m_textRunningPosition);
		}		
	}

	m_elapsedSeconds += elapsedMicroseconds / 1000.0 / 1000.0;
	if (m_elapsedSeconds > 0.5)
	{
		m_elapsedSeconds -= 0.5;

		if (m_fieldType == LDPField::DisplayType::DateTime)
		{
			SYSTEMTIME local;
			GetLocalTime(&local);
			m_elapsedCount = (int)(((local.wSecond * 1000) + local.wMilliseconds) / 500);

			UpdateDateTime();
			m_metricsNeedUpdate = true;			
		}
	}

	return returnValue;
}

const sf::FloatRect LDPField::getBounds()
{
	return m_bounds;
}

void LDPField::setBounds(sf::FloatRect& bounds)
{
	if (bounds.left == m_bounds.left &&
		bounds.width == m_bounds.width &&
		bounds.top == m_bounds.top &&
		bounds.height == m_bounds.height) return;

	m_bounds = bounds;
	m_metricsNeedUpdate = true;	
}

const sf::Color LDPField::getTextColor() const
{
	return m_textColor;
}

void LDPField::setTextColor(sf::Color color)
{
	if (color != m_textColor)
	{
		m_textColor = color;
		m_metricsNeedUpdate = true;
	}
}

const sf::Color LDPField::getBGColor() const
{
	return m_bgColor;
}

void LDPField::setBGColor(sf::Color bgColor)
{
	if (bgColor != m_bgColor)
	{
		m_bgColor = bgColor;
		m_metricsNeedUpdate = true;
	}
}

const sf::String& LDPField::getTextString() const
{
	return m_textString;
}

void LDPField::setTextString(std::string text)
{
	if (text != m_textString)
	{
		m_textString = text;
		m_metricsNeedUpdate = true;
	}
}

uint32_t LDPField::getTextSize() const
{
	return m_textSize;
}

void LDPField::setTextSize(uint32_t size)
{
	if (size != m_textSize)
	{
		m_textSize = size;
		m_metricsNeedUpdate = true;
	}
}

const sf::Text::Style LDPField::getTextStyle() const
{
	return m_textStyle;
}

void LDPField::setTextStyle(sf::Text::Style style)
{
	if (style != m_textStyle)
	{
		m_textStyle = style;
		m_metricsNeedUpdate = true;
	}
}

float LDPField::getTextSpeed() const
{
	return m_textRunningSpeed;
}

void LDPField::setTextSpeed(float speed)
{
	if (speed != m_textRunningSpeed)
	{
		m_textRunningSpeed = speed;
		//CalculateRunningMicroseconds();
	}
}

const sf::Font* LDPField::getFont() const
{
	return &m_textFont;
}

bool LDPField::setFont(std::string fontName)
{
	if (m_fontFileName.compare(fontName) != 0)
	{
		bool b = m_textFont.loadFromFile(fontName);
		if (b == true)
		{
			m_metricsNeedUpdate = true;
			m_usingGoodFont = true;
			m_fontFileName = fontName;
		}
		else
		{
			m_metricsNeedUpdate = false;
			m_usingGoodFont = false;
		}
		return b;
	}
	else
	return true;	
}

const LDPField::DisplayType LDPField::getFieldType() const
{
	return m_fieldType;
}

void LDPField::setDisplayType(LDPField::DisplayType type)
{
	if (type != m_fieldType)
	{
		m_fieldType = type;
		m_metricsNeedUpdate = true;
	}
}

const std::string LDPField::getFormatString() const
{
	return m_formatString;
}

void LDPField::setFormatString(std::string format)
{
	if (format != m_formatString)
	{
		m_formatString = format;
		m_metricsNeedUpdate = true;
	}
}

bool LDPField::getMetricsNeedUpdate() const
{
	return m_metricsNeedUpdate;
}

void LDPField::EnsureMetricsUpdate()
{
	if (m_metricsNeedUpdate)
	{
		CalculateMetrics();
	}
}

void LDPField::CalculateMetrics()
{
	//checking if the font is good
	if (m_usingGoodFont == false)
	{
		m_metricsNeedUpdate = false;
		return;
	}		
		
	sf::Texture& texture = const_cast<sf::Texture&>(m_textFont.getTexture(m_textSize));
	texture.setSmooth(true);	

	//calculating lower bounds
	sf::Text tempText("Hxj", m_textFont, m_textSize);
	tempText.setStyle(m_textStyle);
	sf::FloatRect tempBounds = tempText.getLocalBounds();
	float t = m_bounds.height - (tempBounds.top + tempBounds.height);
	m_textObject.setOrigin(0, -t);
	
	//updating text object
	m_textObject.setFont(m_textFont);
	m_textObject.setCharacterSize(m_textSize);
	m_textObject.setStyle(m_textStyle);
	if (m_fieldType != DisplayType::DateTime)
	{
		std::wstring wtemp = m_textString;
		m_textObject.setString(wtemp);
	}
	m_textObject.setFillColor(m_textColor);
	m_textObject.setPosition(0, 0);
	tempBounds = m_textObject.getLocalBounds();

	//looking for aligment
	m_textRunning = false;
	switch (m_fieldType)
	{
	default:
	case DisplayType::LeftAlign:		
		break;
	case DisplayType::RightAlign:
		m_textObject.setPosition(m_bounds.width - tempBounds.width - 4, 0);		
		break;
	case DisplayType::CenterAlign:
		m_textObject.setPosition(m_bounds.width / 2 - tempBounds.width / 2, 0);		
		break;
	case DisplayType::OptionalLeft:
		if (tempBounds.width > m_bounds.width) m_textRunning = true;
		break;
	case DisplayType::Running:
		m_textRunning = true;
		break;
	}

	if (m_textRunning == true)
	{	
		std::wstring tempString = m_textString + L"   ";
		m_textObject.setString(tempString);
		tempBounds = m_textObject.getLocalBounds();
		m_TextWidth = tempBounds.width + tempBounds.left;		

		//updating field position and size	
		m_fieldTexture.create((int)tempBounds.width, (int)m_bounds.height);
		m_fieldTexture.clear(m_bgColor);
		m_fieldTexture.draw(m_textObject);
		m_fieldTexture.display();
		sf::Texture& texture = const_cast<sf::Texture&>(m_fieldTexture.getTexture());
		texture.setRepeated(true);

		sf::IntRect texRect;
		texRect.left = (int)round(m_textRunningPosition);
		texRect.top = 0;
		texRect.width = (int)m_bounds.width;
		texRect.height = (int)m_bounds.height;

		m_drawSprite.setTexture(m_fieldTexture.getTexture(), true);
		m_drawSprite.setTextureRect(texRect);
		m_drawSprite.setPosition(m_bounds.left, m_bounds.top);
	}	
	else
	{
		//updating field position and size	
		m_fieldTexture.create((int)m_bounds.width, (int)m_bounds.height);
		m_fieldTexture.clear(m_bgColor);
		m_fieldTexture.draw(m_textObject);
		m_fieldTexture.display();

		m_drawSprite.setTexture(m_fieldTexture.getTexture(), true);
		m_drawSprite.setPosition(m_bounds.left, m_bounds.top);
	}	

	m_metricsNeedUpdate = false;
}

void LDPField::UpdateDateTime()
{
	std::time_t t = std::time(nullptr);
	std::tm tm;
	localtime_s(&tm, &t);
	std::string tempFormat = m_formatString;
	size_t attribute = tempFormat.find("%1");  //change 1 time per second
	while (attribute != std::string::npos)
	{		
		if (m_elapsedCount % 4 > 1) tempFormat.erase(attribute, 2);
		else
		{
			tempFormat.erase(attribute, 3);
			tempFormat.insert(attribute, " ");
		}
		attribute = tempFormat.find("%1");
	}
	attribute = tempFormat.find("%2");  //change 2 times per second
	while (attribute != std::string::npos)
	{		
		if (m_elapsedCount % 2 == 0) tempFormat.erase(attribute, 2);
		else
		{
			tempFormat.erase(attribute, 3);
			tempFormat.insert(attribute, " ");
		}
		attribute = tempFormat.find("%2");
	}

	//LOG_TRACE("Resulting format={}", tempFormat);

	std::stringstream tempStream;
	tempStream << std::put_time(&tm, tempFormat.c_str()) << '\n';
	m_textObject.setString(tempStream.str());

	//m_fieldTexture.clear(m_bgColor);
	//m_fieldTexture.draw(m_textObject);
	//m_fieldTexture.display();
}

//void LDPField::CalculateRunningMicroseconds()
//{
//	m_textRunningLastUpdateMicro = 0;
//	m_textRunningCurrentMicro = 0;
//	m_textRunningUpdateEveryMicro = (int)round(1000000 / m_textRunningSpeed);
//}
