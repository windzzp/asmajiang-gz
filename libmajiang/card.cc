#include "card.h"

static string face_symbols[] =
{
 "1条", "2条", "3条", "4条", "5条", "6条", "7条", "8条", "9条",
 "1万", "2万", "3万", "4万", "5万", "6万", "7万", "8万", "9万",
 "1筒", "2筒", "3筒", "4筒", "5筒", "6筒", "7筒", "8筒", "9筒",
 "东", "南", "西", "北", "中", "发", "白"
};


Card::Card()
{
	face = value = suit = 0;
}

Card::Card(int val)
{
	value = val;
	face = value & 0x0F;
    suit = (value & 0xF0) >> 4;
}

void Card::set_value(int val)
{
    value = val;
	face = value & 0x0F;
    suit = (value & 0xF0) >> 4;
}

string Card::get_card()
{
	string card;

    card = face_symbols[suit * 9 + face - 1];

	return card;
}
