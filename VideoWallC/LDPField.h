#pragma once
#include <SFML/Graphics.hpp>

class LDPField
{
public:	
	enum DisplayType
	{
		LeftAlign		= 0,
		RightAlign		= 1 << 0,
		CenterAlign		= 1 << 1,
		OptionalLeft	= 1 << 2,
		Running			= 1 << 3,
		DateTime		= 1 << 4
	};

	LDPField();
	LDPField(sf::FloatRect rect, sf::String text);
	LDPField(const LDPField& copy);
	~LDPField();

	void draw(sf::RenderWindow &window);
	bool update(sf::Int64 elapsedMicroseconds, bool needUpdateRunning);

	const sf::FloatRect getBounds();
	void setBounds(sf::FloatRect& bounds);

	const sf::Color getTextColor() const;
	void setTextColor(sf::Color color);

	const sf::Color getBGColor() const;
	void setBGColor(sf::Color bgColor);

	const sf::String& getTextString() const;
	void setTextString(std::string text);

	uint32_t getTextSize() const;
	void setTextSize(uint32_t size);

	const sf::Text::Style getTextStyle() const;
	void setTextStyle(sf::Text::Style style);

	float getTextSpeed() const;
	void setTextSpeed(float speed);

	const sf::Font* getFont() const;
	bool setFont(std::string fontName);

	const LDPField::DisplayType getFieldType() const;
	void setDisplayType(LDPField::DisplayType type);

	const std::string getFormatString() const;
	void setFormatString(std::string format);

	bool getMetricsNeedUpdate() const;

private:
	sf::FloatRect		m_bounds;
	sf::Color			m_textColor;
	sf::Color			m_bgColor;
	sf::String			m_textString;
	uint32_t			m_textSize;	
	sf::Text::Style		m_textStyle;
	float				m_textRunningSpeed;
	double				m_textRunningPosition;
	//sf::Int64			m_textRunningLastUpdateMicro;
	//sf::Int64			m_textRunningCurrentMicro;
	//sf::Int64			m_textRunningUpdateEveryMicro;
	std::string			m_fontFileName;
	DisplayType			m_fieldType;
	std::string			m_formatString;

	sf::Text			m_textObject;
	sf::RenderTexture	m_fieldTexture;
	sf::Sprite			m_drawSprite;
	sf::Font			m_textFont;
	float				m_TextWidth;
	bool				m_textRunning;
	bool				m_usingGoodFont;
	bool				m_metricsNeedUpdate;
	double				m_elapsedSeconds;
	size_t				m_elapsedCount;

	void EnsureMetricsUpdate();
	void CalculateMetrics();
	void UpdateDateTime();

	//void CalculateRunningMicroseconds();
};