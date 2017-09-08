#ifndef _CARD_H_
#define _CARD_H_

#include<string.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdio>
#include <vector>
#include <map>
#include <algorithm>

#include "card_type.h"

using namespace std;

/**
 * 
 * 	
 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
 
 * @author luochuanting
*/



class Card
{
public:

    typedef enum
    {
        One = 1,
		Two,
		Three,
		Four,
		Five,
		Six,
		Seven,
		Eight,
		Nine,
		East = 1,
		South,
		West,
		North,
		Zhong,
		Fa,
		Bai
    }Face;

    typedef enum
    {
        EastV = 49,
        SouthV,
        WestV,
        NorthV,
        ZhongV,
        FaV,
        BaiV
    }FengV;

    typedef enum
    {
        TIAO = 0,
        WAN,
        TONG,
        FENG
    }Suit;

public:
	int face;
	int value;
    int suit;

	Card();
	Card(int val);
	void set_value(int val);

	std::string get_card();

	bool operator <(const Card &c) const
	{
        if (suit == c.suit)
        {
            return (face < c.face);
        }
        else
        {
            return suit < c.suit;
        }
	}

	bool operator >(const Card &c) const
	{
        if (suit == c.suit)
        {
            return (face > c.face);
        }
        else
        {
            return suit > c.suit;
        }
	}

	bool operator ==(const Card &c) const
	{
		return value == c.value;
	}

    bool operator !=(const Card &c) const
    {
        return value != c.value;
    }

	static int compare(const Card &a, const Card &b)
    {
        if (a.suit > b.suit)
        {
            return 1;
        }
        else if (a.suit < b.suit)
        {
            return -1;
        }
        else
        {
            if (a.face > b.face)
            {
                return 1;
            }
            else if (a.face < b.face)
            {
                return -1;
            }
            else
            {
                return 0;
            }
        } 
    }

	static bool lesser_callback(const Card &a, const Card &b)
	{
		if (Card::compare(a, b) == -1)
			return true;
		else
			return false;
	}

	static bool greater_callback(const Card &a, const Card &b)
	{
		if (Card::compare(a, b) == 1)
			return true;
		else
			return false;
	}

	static void sort_by_ascending(std::vector<Card> &v)
	{
		sort(v.begin(), v.end(), Card::lesser_callback);
	}

	static void sort_by_descending(std::vector<Card> &v)
	{
		sort(v.begin(), v.end(), Card::greater_callback);
	}

	static void dump_cards(std::vector<Card> &v, string str = "cards")
	{
		fprintf(stdout, "[%s]: [[ ", str.c_str());
		for (std::vector<Card>::iterator it = v.begin(); it != v.end(); it++)
			fprintf(stdout, "%s ", it->get_card().c_str());

		fprintf(stdout, "]]\n");
	}

	static void dump_cards(std::map<int, Card> &m, string str = "cards")
	{
		fprintf(stdout, "[%s]: [[ ", str.c_str());
		for (std::map<int, Card>::iterator it = m.begin(); it != m.end(); it++)
			fprintf(stdout, "%s ", it->second.get_card().c_str());

		fprintf(stdout, "]]\n");
	}

    static void desc_cards(std::vector<Card> &v, string &str)
    {
        char buff[256] = {0, };
        std::vector<Card>::iterator it = v.begin();
        for (; it != v.end(); it++)
        {
            int len = strlen(buff);
            snprintf(buff + len, 256 - len, "%s ", it->get_card().c_str());
        }

        if (buff[0] != '\0')
        {
            buff[strlen(buff) - 1] = '\0';
        }

        str = buff;
    }
};

#endif /* _CARD_H_ */
