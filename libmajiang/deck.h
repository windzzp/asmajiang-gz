#ifndef _DECK_H_
#define _DECK_H_

#include <vector>

#include "card.h"
#include "hole_cards.h"

using namespace std;

class Deck
{
public:
    Deck();
    void init(int feng, int gui, int horse, int duizi, int gh_card = Card::ZhongV);
	void fill();
	void empty();
	int size() const;
    int permit_get() const;
	
	bool push(Card card);
	bool pop(Card &card);
	bool shuffle(int seed);
	
	void get_hole_cards(HoleCards &holecards);
	void get_hole_cards(HoleCards &holecards, int card_type);
    void get_next_card(HoleCards &holecards, int card_value = 0);
    int  see_next_card();
	void change_hole_cards(HoleCards &holecards, int pos);
	void debug();

    // 翻鬼，flag为1时，会将鬼牌从剩余的牌中去掉
    void turn_ghost(Card& card, Card& ghost, int flag);

public:
	vector<Card> cards;
    vector<Card> horse_cards;
    int get_count;

private:
    void delete_cards(std::vector<Card> &tmp);

private:
    int has_feng;
    int has_ghost;
    int horse_num;
    int hu_duizi;
    int ghost_card;
};

#endif /* _DECK_H_ */
