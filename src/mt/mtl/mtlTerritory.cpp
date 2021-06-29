/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: mtlTerritory.cpp 87064 2020-03-30 04:21:12Z jake.jang $
 ***********************************************************************/

#include <mte.h>
#include <mtl.h>
#include <mtlTerritory.h>

/* ���� ������ Territory �̸� */
static const SChar * nlsTerritoryName[TERRITORY_MAX] =
{
    "ALBANIA",
    "ALGERIA",
    "AMERICA",
    "ARGENTINA",
    "AUSTRALIA",
    "AUSTRIA",      /* 5 */
    "AZERBAIJAN",
    "BAHRAIN",
    "BANGLADESH",
    "BELARUS",
    "BELGIUM",      /* 10 */
    "BRAZIL",
    "BULGARIA",
    "CANADA",
    "CATALONIA",
    "CHILE",
    "CHINA",
    "CIS",
    "COLOMBIA",
    "COSTA RICA",
    "CROATIA",      /* 20 */
    "CYPRUS",
    "CZECH REPUBLIC",
    "CZECHOSLOVAKIA",
    "DENMARK",
    "DJIBOUTI",
    "ECUADOR",
    "EGYPT",
    "EL SALVADOR",
    "ESTONIA",
    "FINLAND",      /* 30 */
    "FRANCE",
    "FYR MACEDONIA",
    "GERMANY",
    "GREECE",
    "GUATEMALA",
    "HONG KONG",
    "HUNGARY",
    "ICELAND",
    "INDIA",
    "INDONESIA",    /* 40 */
    "IRAQ",
    "IRELAND",
    "ISRAEL",
    "ITALY",
    "JAPAN",
    "JORDAN",
    "KAZAKHSTAN",
    "KOREA",
    "KUWAIT",
    "LATVIA",       /* 50 */
    "LEBANON",
    "LIBYA",
    "LITHUANIA",
    "LUXEMBOURG",
    "MACEDONIA",
    "MALAYSIA",
    "MAURITANIA",
    "MEXICO",
    "MOROCCO",
    "NEW ZEALAND",  /* 60 */
    "NICARAGUA",
    "NORWAY",
    "OMAN",
    "PANAMA",
    "PERU",
    "PHILIPPINES",
    "POLAND",
    "PORTUGAL",
    "PUERTO RICO",
    "QATAR",        /* 70 */
    "ROMANIA",
    "RUSSIA",
    "SAUDI ARABIA",
    "SERBIA AND MONTENEGRO",
    "SINGAPORE",
    "SLOVAKIA",
    "SLOVENIA",
    "SOMALIA",
    "SOUTH AFRICA",
    "SPAIN",        /* 80 */
    "SUDAN",
    "SWEDEN",
    "SWITZERLAND",
    "SYRIA",
    "TAIWAN",
    "THAILAND",
    "THE NETHERLANDS",
    "TUNISIA",
    "TURKEY",
    "UKRAINE",      /* 90 */
    "UNITED ARAB EMIRATES",
    "UNITED KINGDOM",
    "UZBEKISTAN",
    "VENEZUELA",
    "VIETNAM",
    "YEMEN",
    "YUGOSLAVIA" /* end of oracle in 11g2*/
};

/* �� Territory �� �ش��ϴ� ISO Currency Code */
static const SChar * nlsTeritoryCode[TERRITORY_MAX] =
{
    "ALL",  // 0 ALBANIA
    "DZD",  // 1 ALGERIA
    "USD",  // 2 AMERICA
    "ARS",  // 3 ARGENTINA
    "AUD",  // 4 AUSTRALIA
    "EUR",  // 5 AUSTRIA
    "AZN",  // 6 AZERBAIJAN
    "BHD",  // 7 BAHRAIN
    "BDT",  // 8 BANGLADESH
    "BYR",  // 9 BELARUS
    "EUR",  //10 BELGIUM
    "BRL",  //11 BRAZIL
    "BGN",  //12 BULGARIA
    "CAD",  //13 CANADA
    "EUR",  //14 CATALONIA
    "CLP",  //15 CHILE
    "CNY",  //16 CHINA
    "SUR",  //17 CIS
    "COP",  //18 COLOMBIA
    "CRC",  //19 COSTA RICA
    "HRK",  //20 CROATIA
    "EUR",  //21 CYPRUS
    "CZK",  //22 CZECH REPUBLIC
    "CSK",  //23 CZECHOSLOVAKIA
    "DKK",  //24 DENMARK
    "DJF",  //25 DJIBOUTI
    "USD",  //26 ECUADOR
    "EGP",  //27 EGYPT
    "SVC",  //28 EL SALVADOR
    "EUR",  //29 ESTONIA
    "EUR",  //30 FINLAND
    "EUR",  //31 FRANCE
    "MKD",  //32 FYR MACEDONIA
    "EUR",  //33 GERMANY
    "EUR",  //34 GREECE
    "GTQ",  //35 GUATEMALA
    "HKD",  //36 HONG KONG
    "HUF",  //37 HUNGARY
    "ISK",  //38 ICELAND
    "INR",  //39 INDIA
    "IDR",  //40 INDONESIA
    "IQD",  //41 IRAQ
    "EUR",  //42 IRELAND
    "ILS",  //43 ISRAE
    "EUR",  //44 ITALY
    "JPY",  //45 JAPAN
    "JOD",  //46 JORDAN
    "KZT",  //47 KAZAKHSTAN
    "KRW",  //48 KOREA
    "KWD",  //49 KUWAIT
    "LVL",  //50 LATVIA
    "LBP",  //51 LEBANON
    "LYD",  //52 LIBYA
    "LTL",  //53 LITHUANIA
    "EUR",  //54 LUXEMBOURG
    "MKD",  //55 MACEDONIA
    "MYR",  //56 MALAYSIA
    "MRO",  //57 MAURITANIA
    "MXN",  //58 MEXICO
    "MAD",  //59 MOROCCO
    "NZD",  //60 NEW ZEALAND
    "NIO",  //61 NICARAGUA
    "NOK",  //62 NORWAY
    "OMR",  //63 OMAN
    "PAB",  //64 PANAMA
    "PEN",  //65 PERU
    "PHP",  //66 PHILIPPINES
    "PLN",  //67 POLAND
    "EUR",  //68 PORTUGAL
    "USD",  //69 PUERTO RICO
    "QAR",  //70 QATAR
    "RON",  //71 ROMANIA
    "RUB",  //72 RUSSIA
    "SAR",  //73 SAUDI ARABIA
    "CSD",  //74 SERBIA AND MONTENEGRO
    "SGD",  //75 SINGAPORE
    "SKK",  //76 SLOVAKIA
    "SIT",  //77 SLOVENIA
    "SOS",  //78 SOMALIA
    "ZAR",  //79 SOUTH AFRICA
    "EUR",  //80 SPAIN
    "SDG",  //81 SUDAN
    "SEK",  //82 SWEDEN
    "CHF",  //83 SWITZERLAND
    "SYP",  //84 SYRIA
    "TWD",  //85 TAIWAN
    "THB",  //86 THAILAND
    "EUR",  //87 THE NETHERLANDS
    "TND",  //88 TUNISIA
    "TRY",  //89 TURKEY
    "UAH",  //90 UKRAINE
    "AED",  //91 UNITED ARAB EMIRATES
    "GBP",  //92 UNITED KINGDOM
    "UZS",  //93 UZBEKISTAN
    "VEF",  //94 VENEZUELA
    "VND",  //95 VIETNAM
    "YER",  //96 YEMEN
    "YUN"   //97 YUGOSLAVIA  end of oracle in 11g2
};

/* �� Territory �� �ش��ϴ� NLS_NUMERIC_CHARACTERS ��
 * ù ��° �� D( Decimal ) value�̰� �ι�°�� G( Group ) value
 */
static const SChar * nlsNumericChar[TERRITORY_MAX] =
{
    ",.",   // 0 ALBANIA
    ".,",   // 1 ALGERIA
    ".,",   // 2 AMERICA
    ",.",   // 3 ARGENTINA
    ".,",   // 4 AUSTRALIA
    ",.",   // 5 AUSTRIA
    ", ",   // 6 AZERBAIJAN
    ".,",   // 7 BAHRAIN
    ".,",   // 8 BANGLADESH
    ", ",   // 9 BELARUS
    ",.",   //10 BELGIUM
    ",.",   //11 BRAZIL
    ".,",   //12 BULGARIA
    ", ",   //13 CANADA
    ",.",   //14 CATALONIA
    ",.",   //15 CHILE
    ".,",   //16 CHINA
    ", ",   //17 CIS
    ",.",   //18 COLOMBIA
    ",.",   //19 COSTA RICA
    ",.",   //20 CROATIA
    ",.",   //21 CYPRUS
    ",.",   //22 CZECH REPUBLIC
    ",.",   //23 CZECHOSLOVAKIA
    ",.",   //24 DENMARK
    ".,",   //25 DJIBOUTI
    ",.",   //26 ECUADOR
    ".,",   //27 EGYPT
    ".,",   //28 EL SALVADOR
    ", ",   //29 ESTONIA
    ", ",   //30 FINLAND
    ", ",   //31 FRANCE
    ",.",   //32 FYR MACEDONIA
    ",.",   //33 GERMANY
    ",.",   //34 GREECE
    ".,",   //35 GUATEMALA
    ".,",   //36 HONG KONG
    ", ",   //37 HUNGARY
    ",.",   //38 ICELAND
    ".,",   //39 INDIA
    ",.",   //40 INDONESIA
    ".,",   //41 IRAQ
    ".,",   //42 IRELAND
    ".,",   //43 ISRAE
    ",.",   //44 ITALY
    ".,",   //45 JAPAN
    ".,",   //46 JORDAN
    ", ",   //47 KAZAKHSTAN
    ".,",   //48 KOREA
    ".,",   //49 KUWAIT
    ", ",   //50 LATVIA
    ".,",   //51 LEBANON
    ".,",   //52 LIBYA
    ", ",   //53 LITHUANIA
    ",.",   //54 LUXEMBOURG
    ",.",   //55 MACEDONIA
    ".,",   //56 MALAYSIA
    ".,",   //57 MAURITANIA
    ".,",   //58 MEXICO
    ".,",   //59 MOROCCO
    ".,",   //60 NEW ZEALAND
    ".,",   //61 NICARAGUA
    ", ",   //62 NORWAY
    ".,",   //63 OMAN
    ".,",   //64 PANAMA
    ",.",   //65 PERU
    ".,",   //66 PHILIPPINES
    ", ",   //67 POLAND
    ",.",   //68 PORTUGAL
    ".,",   //69 PUERTO RICO
    ".,",   //70 QATAR
    ",.",   //71 ROMANIA
    ", ",   //72 RUSSIA
    ".,",   //73 SAUDI ARABIA
    ",.",   //74 SERBIA AND MONTENEGRO
    ".,",   //75 SINGAPORE
    ",.",   //76 SLOVAKIA
    ",.",   //77 SLOVENIA
    ".,",   //78 SOMALIA
    ".,",   //79 SOUTH AFRICA
    ",.",   //80 SPAIN
    ".,",   //81 SUDAN
    ",.",   //82 SWEDEN
    ".'",   //83 SWITZERLAND
    ".,",   //84 SYRIA
    ".,",   //85 TAIWAN
    ".,",   //86 THAILAND
    ",.",   //87 THE NETHERLANDS
    ".,",   //88 TUNISIA
    ",.",   //89 TURKEY
    ", ",   //90 UKRAINE
    ".,",   //91 UNITED ARAB EMIRATES
    ".,",   //92 UNITED KINGDOM
    ",.",   //93 UZBEKISTAN
    ",.",   //94 VENEZUELA
    ",.",   //95 VIETNAM
    ".,",   //96 YEMEN
    ",.",   //97 YUGOSLAVIA  end of oracle in 11g2
};

typedef struct symbolValue
{
    UChar symbol[11];
} symbolValue;

static symbolValue currencySymbolsTable[IDN_MAX_CHARSET_ID][TERRITORY_MAX];

typedef struct currencySymbol
{
    mtlU16Char code[5];
    UChar      length;
} currencySymbol;

/* UTF 16 ������ Territory �� Currency Symbol ��
 * UTF 16 �� �������� �̷���� ������ ����Ŭ 11g2 ���� ���� �̾Ƽ� �״�� ������.
 */
static currencySymbol utf16Symbols[TERRITORY_MAX] =
{
    { { { 0x0, 0x4C }, { 0x0, 0x65 }, { 0x0, 0x6B }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  // 0 ALBANIA
    { { { 0x6, 0x2F }, { 0x6, 0x2C }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   // 1 ALGERIA
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    // 2 AMERICA
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    // 3 ARGENTINA
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    // 4 AUSTRALIA
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   // 5 AUSTRIA       EURO
    { { { 0x0, 0x41 }, { 0x0, 0x5A }, { 0x0, 0x4D }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  // 6 AZERBAIJAN
    { { { 0x0, 0x2E }, { 0x6, 0x2F }, { 0x0, 0x2E }, { 0x6, 0x28 }, { 0x0, 0x0 } }, 4 }, // 7 BAHRAIN
    { { { 0x9, 0xF3 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    // 8 BANGLADESH
    { { { 0x04, 0x40 }, { 0x0, 0x2E }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },  // 9 BELARUS
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //10 BELGIUM       EURO
    { { { 0x0, 0x52 }, { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //11 BRAZIL
    { { { 0x04, 0x3B }, { 0x04, 0x32 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 }, //12 BULGARIA
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //13 CANADA
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //14 CATALONIA     EURO
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //15 CHILE
    { { { 0xFF, 0xE5 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //16 CHINA
    { { { 0x04, 0x40 }, { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },  //17 CIS
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //18 COLOMBIA
    { { { 0x0, 0x43 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //19 COSTA RICA
    { { { 0x0, 0x6B }, { 0x0, 0x6E }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //20 CROATIA
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //21 CYPRUS        EURO
    { { { 0x0, 0x4B }, { 0x1, 0xD }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },    //22 CZECH REPUBLIC
    { { { 0x0, 0x4B }, { 0x1, 0xD }, { 0x0, 0x73 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },   //23 CZECHOSLOVAKIA
    { { { 0x0, 0x6B }, { 0x0, 0x72 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //24 DENMARK
    { { { 0x0, 0x46 }, { 0x0, 0x64 }, { 0x0, 0x6A }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //25 DJIBOUTI
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //26 ECUADOR
    { { { 0x6, 0x2C }, { 0x0, 0x2E }, { 0x6, 0x45 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //27 EGYPT
    { { { 0x20, 0xA1 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //28 EL SALVADOR
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //29 ESTONIA
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //30 FINLAND       EURO
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //31 FRANCE        EURO
    { { { 0x04, 0x34 }, { 0x04, 0x35 }, { 0x04, 0x3D }, { 0x0, 0x2E }, { 0x0, 0x0 } },4},//32 FYR MACEDONIA
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //33 GERMANY       EURO
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //34 GREECE        EURO
    { { { 0x0, 0x51 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //35 GUATEMALA
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //36 HONG KONG
    { { { 0x0, 0x46 }, { 0x0, 0x74 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //37 HUNGARY
    { { { 0x0, 0x6B }, { 0x0, 0x72 }, { 0x0, 0x2E }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //38 ICELAND
    { { { 0x0, 0x52 }, { 0x0, 0x73 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //39 INDIA
    { { { 0x0, 0x52 }, { 0x0, 0x70 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //40 INDONESIA
    { { { 0x6, 0x39 }, { 0x0, 0x2E }, { 0x6, 0x2F }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //41 IRAQ
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //42 IRELAND       EURO
    { { { 0x20, 0xAA }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //43 ISRAEL
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //44 ITALY         EURO
    { { { 0x00, 0xA5 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //45 JAPAN
    { { { 0x0, 0x4A }, { 0x0, 0x4F }, { 0x0, 0x44 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //46 JORDAN
    { { { 0x0, 0x4B }, { 0x0, 0x5A }, { 0x0, 0x54 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //47 KAZAKHSTAN
    { { { 0xFF, 0xE6 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //48 KOREA
    { { { 0x6, 0x2F }, { 0x0, 0x2E }, { 0x6, 0x43 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //49 KUWAIT
    { { { 0x0, 0x4C }, { 0x0, 0x73 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //50 LATVIA
    { { { 0x0, 0xA3 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //51 LEBANON
    { { { 0x0, 0x4C }, { 0x0, 0x44 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //52 LIBYA
    { { { 0x0, 0x4C }, { 0x0, 0x74 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //53 LITHUANIA
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //54 LUXEMBOURG    EURO
    { { { 0x0, 0x64 }, { 0x0, 0x65 }, { 0x0, 0x6E }, { 0x0, 0x2E }, { 0x0, 0x0 } }, 4 }, //55 MACEDONIA
    { { { 0x0, 0x52 }, { 0x0, 0x4D }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //56 MALAYSIA
    { { { 0x0, 0x55 }, { 0x0, 0x4D }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //57 MAURITANIA
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //58 MEXICO
    { { { 0x6, 0x2F }, { 0x0, 0x2E }, { 0x6, 0x45 }, { 0x0, 0x2E }, { 0x0, 0x0 } }, 4 }, //59 MOROCCO
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //60 NEW ZEALAND
    { { { 0x0, 0x43 }, { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //61 NICARAGUA
    { { { 0x0, 0x6B }, { 0x0, 0x72 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //62 NORWAY
    { { { 0x6, 0x31 }, { 0x0, 0x2E }, { 0x6, 0x39 }, { 0x0, 0x2E }, { 0x0, 0x0 } }, 4 }, //63 OMAN
    { { { 0x0, 0x42 }, { 0x0, 0x2F }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //64 PANAMA
    { { { 0x0, 0x53 }, { 0x0, 0x2F }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //65 PERU
    { { { 0x20, 0xB1 }, { 0x0, 0x0 }, { 0x0, 0x00 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //66 PHILIPPINES
    { { { 0x0, 0x7A }, { 0x01, 0x42 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },  //67 POLAND
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //68 PORTUGAL      EURO
    { { { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //69 PUERTO RICO
    { { { 0x6, 0x31 }, { 0x0, 0x2E }, { 0x6, 0x42 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //70 QATAR
    { { { 0x0, 0x4C }, { 0x0, 0x45 }, { 0x0, 0x49 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //71 ROMANIA
    { { { 0x04, 0x40 }, { 0x0, 0x2E }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },  //72 RUSSIA
    { { { 0x6, 0x31 }, { 0x0, 0x2E }, { 0x6, 0x33 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //73 SAUDI ARABIA
    { { { 0x0, 0x64 }, { 0x0, 0x69 }, { 0x0, 0x6E }, { 0x0, 0x2E }, { 0x0, 0x0 } }, 4 }, //74 SERBIA AND MONTENEGRO
    { { { 0x0, 0x53 }, { 0x0, 0x24 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //75 SINGAPORE
    { { { 0x0, 0x53 }, { 0x0, 0x6B }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //76 SLOVAKIA
    { { { 0x0, 0x53 }, { 0x0, 0x49 }, { 0x0, 0x54 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //77 SLOVENIA
    { { { 0x0, 0x53 }, { 0x0, 0x6F }, { 0x0, 0x2E }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //78 SOMALIA
    { { { 0x0, 0x52 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //79 SOUTH AFRICA
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //80 SPAIN         EURO
    { { { 0x0, 0xA3 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //81 SUDAN
    { { { 0x0, 0x4B }, { 0x0, 0x72 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //82 SWEDEN
    { { { 0x0, 0x53 }, { 0x0, 0x46 }, { 0x0, 0x72 }, { 0x0, 0x2E }, { 0x0, 0x0 } }, 4 }, //83 SWITZERLAND
    { { { 0x0, 0xA3 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //84 SYRIA
    { { { 0x0, 0x4E }, { 0x0, 0x54 }, { 0x0, 0x2E }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //85 TAIWAN
    { { { 0x0E, 0x3F }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //86 THAILAND
    { { { 0x20, 0xAC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //87 THE NETHERLANDS EURO
    { { { 0x6, 0x2F }, { 0x0, 0x2E }, { 0x6, 0x2A }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //88 TUNISIA
    { { { 0x0, 0x53 }, { 0x0, 0x4C }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //89 TURKEY
    { { { 0x04, 0x33 }, { 0x04, 0x40 }, { 0x4, 0x41 }, { 0x0, 0x2E }, { 0x0, 0x0 } }, 4},//90 UKRAINE
    { { { 0x6, 0x2F }, { 0x0, 0x2E }, { 0x6, 0x27 }, { 0x6, 0x55 }, { 0x0, 0x0 } }, 4 }, //91 UNITED ARAB EMIRATES
    { { { 0x0, 0xA3 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },    //92 UNITED KINGDOM
    { { { 0x0, 0x54 }, { 0x0, 0x5A }, { 0x0, 0x53 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 3 },  //93 UZBEKISTAN
    { { { 0x0, 0x42 }, { 0x0, 0x73 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 2 },   //94 VENEZUELA
    { { { 0x20, 0xAB }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //95 VIETNAM
    { { { 0xFD, 0xFC }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 }, { 0x0, 0x0 } }, 1 },   //96 YEMEN
    { { { 0x0, 0x64 }, { 0x0, 0x69 }, { 0x0, 0x6E }, { 0x0, 0x2E }, { 0x0, 0x0 } }, 4 }  //97 YUGOSLAVIA
};

/**
 * getNlsTerritoryName
 *
 *  �Է� ���� enum���� �ش��ϴ� territory�̸��� ��ȯ�Ѵ�.
 */
const SChar * mtlTerritory::getNlsTerritoryName( SInt aNlsTerritory )
{
    if ( ( aNlsTerritory < 0 ) || ( aNlsTerritory >= TERRITORY_MAX ) )
    {
        aNlsTerritory = MTL_TERRITORY_DEFAULT;
    }
    else
    {
        /* Nothing to do */
    }

    return nlsTerritoryName[aNlsTerritory];
}

/**
 * setNlsTerritoryName
 *
 *  �Է� ���� enum ���� �ش��ϴ� terrtiroy �� �̸��� ã�� ���ۿ� �����Ѵ�.
 */
IDE_RC mtlTerritory::setNlsTerritoryName( SInt    aNlsTerritory,
                                          SChar * aBuffer )
{
    const SChar * sName = NULL;

    if ( ( aNlsTerritory < 0 ) || ( aNlsTerritory >= TERRITORY_MAX ) )
    {
        aNlsTerritory = MTL_TERRITORY_DEFAULT;
    }
    else
    {
        /* Nothing to do */
    }

    sName = nlsTerritoryName[aNlsTerritory];
    idlOS::snprintf( aBuffer, MTL_TERRITORY_NAME_LEN + 1, "%s", sName );

    return IDE_SUCCESS;
}

/**
 * searchNlsTerritory
 *
 *  ���ڿ��� �Է����� �޾� �ش��ϴ� Territory�� �ִٸ� �̿� �ش��ϴ� enum����
 *  ��ȯ�Ѵ�. ���� ã�� ���Ѵٸ� -1�� ��ȯ�Ѵ�.
 */
IDE_RC mtlTerritory::searchNlsTerritory( SChar  * aValue,
                                         SInt   * aReturn )
{
    idBool sFind       = ID_FALSE;
    SInt   i           = 0;
    SInt   sValueSize  = 0;
    SInt   len         = 0;

    sValueSize = idlOS::strlen( aValue );

    for ( i = 0; i < TERRITORY_MAX; i ++ )
    {
        len = idlOS::strlen(nlsTerritoryName[i]);

        if ( idlOS::strCaselessMatch( nlsTerritoryName[i], len,
                                      aValue, sValueSize ) == 0 )
        {
            sFind = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sFind == ID_TRUE )
    {
        *aReturn = i;
    }
    else
    {
        *aReturn = -1;
    }

    return IDE_SUCCESS;
}

/**
 * setNlsISOCurrencyCode
 *
 *  ISO Currency�� Session�̳� System Property ���� Territory �̸����� �����ְ�
 *  ���� TO_CHAR���� 'C'�� ���� ����Ҷ��� ISO�̸��� ���ȴ�.
 *  �� �Լ��� ���� Currency Code ���ۿ� �����Ѵ�.
 */
IDE_RC mtlTerritory::setNlsISOCurrencyCode( SInt aNlsISOCurrency,
                                            SChar * aBuffer )
{
    const SChar * sCode = NULL;

    if ( ( aNlsISOCurrency < 0 ) || ( aNlsISOCurrency >= TERRITORY_MAX ) )
    {
        aNlsISOCurrency = MTL_TERRITORY_DEFAULT;
    }
    else
    {
        /* Nothing to do */
    }

    sCode = nlsTeritoryCode[aNlsISOCurrency];

    idlOS::snprintf( aBuffer, MTL_TERRITORY_ISO_LEN + 1, "%s", sCode );

    return IDE_SUCCESS;
}

/**
 * searchISOCurrency
 *
 *  ISO Currency�� Session�̳� System Property ���� Territory �̸����� �����ְ�
 *  ���� TO_CHAR���� 'C'�� ���� ����Ҷ��� ISO�̸��� ���ȴ�.
 *  �� �Լ��� ����ڿ��� �����ٶ� Territory�� ���� TerritoryName�� ���ۿ� �����ؼ�
 *  ������ �� ȣ��ȴ�.
 */
IDE_RC mtlTerritory::setNlsISOCurrencyName( SInt    aNlsISOCurrency,
                                            SChar * aBuffer )
{
    IDE_TEST( setNlsTerritoryName( aNlsISOCurrency, aBuffer )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * searchISOCurrency
 *
 *  �Է¹��� Territory �̸��� �����ϴ� ���� ���´�.
 */
IDE_RC mtlTerritory::searchISOCurrency( SChar * aValue,
                                        SInt  * aReturn )
{
    IDE_TEST( searchNlsTerritory( aValue, aReturn )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * setNlsCurrency
 *
 * ���ڷ� �־��� NLS_TERRITORY�� ���� currencySymbolTable�� ���� ������ ����
 * ���ۿ� �����Ѵ�.
 */
IDE_RC mtlTerritory::setNlsCurrency( SInt aNlsTerritory, SChar * aBuffer )
{
    SInt             sTerritory  = -1;
    SChar          * sSymbol     = NULL;
    mtlModule      * sModule     = NULL;
    idnCharSetList   sDestCharSet;

    if ( ( aNlsTerritory < 0 ) || ( aNlsTerritory >= TERRITORY_MAX ) )
    {
        sTerritory = MTL_TERRITORY_DEFAULT;
    }
    else
    {
        sTerritory = aNlsTerritory;
    }

    sModule = (mtlModule *)mtl::mDBCharSet;
    sDestCharSet = mtl::getIdnCharSet( sModule );
    sSymbol = (SChar *)currencySymbolsTable[sDestCharSet][sTerritory].symbol;

    idlOS::snprintf( aBuffer, MTL_TERRITORY_CURRENCY_LEN + 1, "%s", sSymbol );

    return IDE_SUCCESS;
}

/**
 * setNlsNumericChar
 *
 * ���ڷ� �־��� NLS_TERRITORY�� ���� �̸� ���ǵ� Table�� ���� �̸� ������ ���� �־��ش�.
 */
IDE_RC mtlTerritory::setNlsNumericChar( SInt aNlsTerritory, SChar * aBuffer )
{
    if ( ( aNlsTerritory < 0 ) || ( aNlsTerritory >= TERRITORY_MAX ) )
    {
        aNlsTerritory = MTL_TERRITORY_DEFAULT;
    }
    else
    {
        /* Nothing to do */
    }

    idlOS::snprintf( aBuffer,
                     MTL_TERRITORY_NUMERIC_CHAR_LEN + 1,
                     "%s",
                     nlsNumericChar[aNlsTerritory] );

    return IDE_SUCCESS;
}

/**
 * validNlsNumericChar
 *
 * nls_numeric_characters ���� �����Ҷ� �ùٸ� �������� �˻��Ѵ�.
 *
 * ù ��° ���� D ( Decimal Symbol ) ���̵ǰ� �� ��° ���� G ( Group Symbol )
 *  ���� �ȴ�.
 *
 * '+', '-', '<', '>' ���� �� �� ���� ���� ���� �� �� ����. ���� ASCII���ڸ� �����ϴ�.
 */
IDE_RC mtlTerritory::validNlsNumericChar( SChar * aNumChar, idBool * aIsValid )
{
    if ( aNumChar == NULL )
    {
        *aIsValid = ID_FALSE;
    }
    else
    {
        if ( idlOS::strlen( aNumChar ) < 2 )
        {
            *aIsValid = ID_FALSE;
        }
        else
        {
            if ( ( *aNumChar == '+' ) || ( *aNumChar == '-' ) ||
                 ( *aNumChar == '<' ) || ( *aNumChar == '>' ) ||
                 ( *( aNumChar + 1 ) == '+' ) ||
                 ( *( aNumChar + 1 ) == '-' ) ||
                 ( *( aNumChar + 1 ) == '<' ) ||
                 ( *( aNumChar + 1 ) == '>' ) ||
                 ( *aNumChar == *( aNumChar + 1 ) ) )
            {
                *aIsValid = ID_FALSE;
            }
            else
            {
                *aIsValid = ID_TRUE;
            }
        }
    }
    
    return IDE_SUCCESS;
}

IDE_RC mtlTerritory::checkNlsNumericChar( SChar * aNumChar )
{
    idBool  sIsValid;

    IDE_TEST( validNlsNumericChar( aNumChar, &sIsValid )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsValid == ID_FALSE, ERR_INVALID_CHARACTER );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_CHARACTER )
    {
        IDE_TEST( ideSetErrorCode( mtERR_ABORT_INVALID_CHARACTER ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * validCurrencySymbol
 *
 * nls_currency ���� ������ �� �ùٸ� �������� üũ�Ѵ�.
 *
 * nls_currency�� ù ��° ���� '+', '-', '<', '>' �� �� �� ����.
 */
IDE_RC mtlTerritory::validCurrencySymbol( SChar * aSymbol, idBool * aIsValid )
{
    if ( aSymbol == NULL )
    {
        *aIsValid = ID_FALSE;
    }
    else
    {
        if ( idlOS::strlen( aSymbol ) < 1 )
        {
            *aIsValid = ID_FALSE;
        }
        else
        {
            if ( ( *aSymbol == '+' ) || ( *aSymbol == '-' ) ||
                 ( *aSymbol == '<' ) || ( *aSymbol == '>' ) )
            {
                *aIsValid = ID_FALSE;
            }
            else
            {
                *aIsValid = ID_TRUE;
            }
        }
    }

    return IDE_SUCCESS;
}

IDE_RC mtlTerritory::checkCurrencySymbol( SChar * aSymbol )
{
    idBool  sIsValid;

    IDE_TEST( validCurrencySymbol( aSymbol, &sIsValid )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsValid == ID_FALSE, ERR_INVALID_CHARACTER );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_CHARACTER )
    {
        IDE_TEST( ideSetErrorCode( mtERR_ABORT_INVALID_CHARACTER ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * createCurrencySymbolTable
 *
 *  �̹� ���ǵ� UTF16 ������ �ڵ� ���� �� ĳ���� �� ���� Convert���� Table�� �����Ѵ�.
 *
 *  UTF16 ������ Convert�� ���� ĳ���� �¿� ���Ե��� �ʴ� ���� ��찡 �ִ�.
 *
 *  �̷� ��� ���� �� ĳ���ͼ¿��� ����� Symbol�� ã�Ƽ� �������ش�.
 */
IDE_RC mtlTerritory::createCurrencySymbolTable( void )
{
    SInt             sTerritory  = 0;
    SInt             sCharSet    = 0;
    currencySymbol * sSymbol     = NULL;
    SInt             sDestRemain = 0;
    UChar          * sBuffer     = NULL;
    UChar          * sSymbolBuff = NULL;
    UInt             i           = 0;
    idnCharSetList   sSrcCharSet;
    idnCharSetList   sDestCharSet;
    mtlU16Char       sCode;

    for ( sCharSet = 0; sCharSet < IDN_MAX_CHARSET_ID; sCharSet++ )
    {
        for ( sTerritory = 0; sTerritory < TERRITORY_MAX; sTerritory++ )
        {
            sSymbol     = &utf16Symbols[sTerritory];
            sDestRemain = MTL_TERRITORY_CURRENCY_LEN;
            sSrcCharSet  = IDN_UTF16_ID;
            sDestCharSet = (idnCharSetList)sCharSet;

            sSymbolBuff = currencySymbolsTable[sCharSet][sTerritory].symbol;
            sBuffer = sSymbolBuff;
            if ( sDestCharSet != IDN_UTF16_ID )
            {
                for ( i = 0; i < sSymbol->length; i ++ )
                {
                    IDE_TEST( convertCharSet( sSrcCharSet,
                                              sDestCharSet,
                                              (void *)&sSymbol->code[i],
                                              2,
                                              sBuffer,
                                              &sDestRemain,
                                              -1 )
                              != IDE_SUCCESS );
                    sBuffer = sSymbolBuff + ( MTL_TERRITORY_CURRENCY_LEN - sDestRemain );
                }
                *sBuffer = 0;
            }
            else
            {
                for ( i = 0; i < sSymbol->length; i++ )
                {
                    UTF16BE_TO_WC( sBuffer, &sSymbol->code[i] );
                    sBuffer += 2;
                }
                *( sSymbolBuff + sSymbol->length * 2 ) = 0;
            }
        }
    }

    /* Big5 �� ǥ�� */
    sCode.value1 = 0xA2;
    sCode.value2 = 0x44;
    sBuffer = currencySymbolsTable[IDN_BIG5_ID][TERRITORY_CHINA].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    sBuffer = currencySymbolsTable[IDN_BIG5_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* GB2312 �� ǥ�� */
    sCode.value1 = 0xA3;
    sCode.value2 = 0xA4;
    sBuffer = currencySymbolsTable[IDN_GB231280_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* GB2312 �Ŀ�� ǥ�� */
    sCode.value1 = 0xA1;
    sCode.value2 = 0xEA;
    sBuffer = currencySymbolsTable[IDN_GB231280_ID][TERRITORY_UNITED_KINGDOM].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* SHIFTJIS �� ǥ�� */
    sCode.value1 = 0x81;
    sCode.value2 = 0x8F;
    sBuffer = currencySymbolsTable[IDN_SHIFTJIS_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* EUCJP �� ǥ�� */
    sCode.value1 = 0xA1;
    sCode.value2 = 0xEF;
    sBuffer = currencySymbolsTable[IDN_EUCJP_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* KSC5601 �� ǥ�� */
    sCode.value1 = 0xA1;
    sCode.value2 = 0xCD;
    sBuffer = currencySymbolsTable[IDN_KSC5601_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* KSC5601 �Ŀ�� ǥ�� */
    sCode.value1 = 0xA1;
    sCode.value2 = 0xCC;
    sBuffer = currencySymbolsTable[IDN_KSC5601_ID][TERRITORY_UNITED_KINGDOM].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* MS949 �� ǥ�� */
    sCode.value1 = 0xA1;
    sCode.value2 = 0xCD;
    sBuffer = currencySymbolsTable[IDN_MS949_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* MS949 �Ŀ�� ǥ�� */
    sCode.value1 = 0xA1;
    sCode.value2 = 0xCC;
    sBuffer = currencySymbolsTable[IDN_MS949_ID][TERRITORY_UNITED_KINGDOM].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰�
     *
     * MS936 �� ǥ��
     *
     * MS936 �� GB2312 �� ���ۼ����� Ư�� �ڵ������� ������ �����ϰ� �ִ�. �� ��
     * ȣ�� �Ŀ�� ��ȣ�� ������ ���Ѵ�. ����, ���� �ڵ� ���� �̿��Ѵ�.
     */
    sCode.value1 = 0xA3;
    sCode.value2 = 0xA4;
    sBuffer = currencySymbolsTable[IDN_MS936_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰�
     *
     * MS936 �Ŀ�� ǥ��
     *
     * ����� LEBANON, SUDAN, SYRIA �� �Ŀ�� ��ȣ�� ��ȭ�� ���������, ��������
     * �ʾҴ�.
     */
    sCode.value1 = 0xA1;
    sCode.value2 = 0xEA;
    sBuffer = currencySymbolsTable[IDN_MS936_ID][TERRITORY_UNITED_KINGDOM].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    /****************************************************************
     * 
     * PROJ-2590 [��ɼ�] CP932 database character set ����
     *
     * MS932 �� ǥ�� : �Ϻ� ��ȭ
     * UTF16 0xA5(��) -> CP932 ����. 0x818F�� �ٲ��ش�.
     *
     ***************************************************************/
    sCode.value1 = 0x81;
    sCode.value2 = 0x8F;
    sBuffer = currencySymbolsTable[IDN_MS932_ID][TERRITORY_JAPAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;
 
    /****************************************************************
     * 
     * PROJ-2590 [��ɼ�] CP932 database character set ����
     *
     * MS932 �Ŀ�� ǥ�� : ���ٳ�, ����, �ø���, ���� ��ȭ
     * UTF16 0xA3(��) -> CP932 ����. 0x8192�� �ٲ��ش�.
     *
     ***************************************************************/
    sCode.value1 = 0x81;
    sCode.value2 = 0x92;
    sBuffer = currencySymbolsTable[IDN_MS932_ID][TERRITORY_LEBANON].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    sBuffer = currencySymbolsTable[IDN_MS932_ID][TERRITORY_SUDAN].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    sBuffer = currencySymbolsTable[IDN_MS932_ID][TERRITORY_SYRIA].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    sBuffer = currencySymbolsTable[IDN_MS932_ID][TERRITORY_UNITED_KINGDOM].symbol;
    idlOS::memcpy( sBuffer, &sCode, 2 );
    *( sBuffer + 2 ) = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

// PROJ-2727
IDE_RC mtlTerritory::searchNlsISOTerritoryName( SChar  * aValue,
                                                SChar  * aBuffer )
{
    SInt   i           = 0;
    SInt   sValueSize  = 0;
    SInt   len         = 0;

    sValueSize = idlOS::strlen( aValue );

    for ( i = 0; i < TERRITORY_MAX; i ++ )
    {
        len = idlOS::strlen(nlsTeritoryCode[i]);

        if ( idlOS::strCaselessMatch( nlsTeritoryCode[i], len,
                                      aValue, sValueSize ) == 0 )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }
    
    IDE_TEST( mtlTerritory::setNlsTerritoryName( i, aBuffer )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
