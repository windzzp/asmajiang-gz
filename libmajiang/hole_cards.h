#ifndef _HOLE_CARDS_H_
#define _HOLE_CARDS_H_

#include <vector>
#include <list>
#include <set>
#include <queue>
#include <algorithm>

#include "card.h"

using namespace std;
class HoleCards;

struct CardTypePriority
{
public:
    CardTypePriority(int type, int priority)
    {
        _type = type;
        _priority = priority;
    }

    friend bool operator < (CardTypePriority n1, CardTypePriority n2)  
    {  
        return n1._priority > n2._priority;  //>"为从小到大排列
    }

    int type() { return _type; }

private:
    int _type;
    int _priority;
};

class HoleCards
{
public:
	HoleCards();

    void init(int duizi, int ghost, int gh_card = Card::ZhongV, int feng = 1);

	void add_card(Card c);

	void sort();

	void analysis();

    void analysis(Card card);

    void analysis_card_type(int value = 0);

	int compare(HoleCards &hc);

	void clear()
	{
		cards.clear();
        oldcards.clear();
		card_type = 0;
        discard_cards.clear();
        obsorb_cards.clear();
        hole_cards.clear();
        hu_cards.clear();
        ting_cards.clear();
        last_card = 0;
        eat_cards.clear();
        has_four_ghost = 0;
        chi_count = 0;
        peng_count = 0;
        gang_count = 0;
        men_qing_flag = -1;
        pure_suit_flag = -1;
        quan_qiu_ren_flag = -1;
        ke_count = 0;
        qi_dui_flag = -1;
        an_gang_count = 0;
        ming_gang_count = 0;
        ke_record.clear();
        peng_peng_flag = -1;
        hun_suit_flag = -1;
		gang_flags.clear();
        ming_ke_record.clear();
        lack_suit = -1;
        lack_suit_cards.clear();
        hu_gang_cards.clear();
        hu_cards_record.clear();
	}

	void copy_cards(std::vector<Card> &v);
	void copy_cards(std::vector<int> &v);

    void handler_chu(int value);
    void handler_chi(int value, int pattern[3], int flag);
    void handler_peng(int value, int flag);
    void handler_gang(int value, int gang_flag, int flag);
    int  handler_ting(int value);

    bool permit_ting();
    bool permit_gang();
    bool permit_gang(int value);
    bool permit_peng(int value);
    bool permit_chi(int value);
    bool permit_hu(int value);
    bool permit_hu();
    bool has_card(int value, int flag = 0);
    static void register_type_handler(int type, int priority);
    static void clear_type_handler();

    bool handler_card_type_call(vector<Card>& acards);
	
	void get_default_change_cards(vector<Card> &v);
    void get_default_lack_suit(int &suit);
    void change_cards(vector<Card>& v1, vector<Card>& v2);
    void set_lack_suit(int suit);
    int has_lack_suit_card();
	
	int size() const;
	void debug();
    void set_shi_san_yao(bool value);
    void set_si_ghost(bool value);
    void set_ghost(int value);
    void erase_discard_card(int value);
    void erase_obsorb_card(int value);
    void erase_card(int value);
    void record_hu_card(int value);

public:
	std::vector<Card> cards;
    std::vector<Card> oldcards;

    std::list<Card> discard_cards;
    std::vector<std::vector<Card> > obsorb_cards; // 吃碰杠的牌
    std::vector<std::vector<Card> > hole_cards; // 手上的牌组合
    std::vector<Card> hu_cards;

    std::vector<Card> eat_cards;  // 保存可以吃的牌的组合，3个一组
    std::vector<Card> gang_cards; // 明杠
    int gang_flag;   // 0 是吃杠，1是碰杠，2是3+1，3是手上四个形成暗杠
    std::vector<int> gang_flags; // 杠标记

	int card_type;
    Card last_card;
    std::map<int, vector<Card> > ting_cards;

    int hu_pair;
    int has_ghost;
    int has_four_ghost;

    int peng_count; //统计碰的次数
    int gang_count; //统计杠的次数
    int an_gang_count; //统计暗杠
    int ming_gang_count; //统计明杠
    int chi_count; //统计吃的次数

    // 标记，如果为-1没处理过，0为false，1为true
    int men_qing_flag; //门清标志
    int pure_suit_flag; //清一色标志
    int hun_suit_flag; // 混一色标记
    int quan_qiu_ren_flag; //全求人标记
    int qi_dui_flag; //7对标志
    int peng_peng_flag; // 碰碰胡标记

    int ke_count;   //统计碰的刻字数，杠也算
    set<int> ming_ke_record; // 记录明刻
    set<int> ke_record; // 记录刻字（杠也算是刻)
	bool enable_shi_san_yao; // 是否允许十三幺
    bool enable_si_ghost;    // 是否允许四个鬼牌直接胡
	int lack_suit;
    vector<Card> lack_suit_cards;

    vector<Card> hu_cards_record; // 胡过的牌的记录
    std::set<Card> hu_gang_cards; // 胡过后哪些能杠

    static std::priority_queue<CardTypePriority> card_types;
    static std::priority_queue<CardTypePriority> card_types_back;

private:
    int ghost_card; //定义癞子牌
    int has_feng; //是否有风牌

private:
    bool is_shunzi(Card &card1, Card &card2, Card &card3);
    bool is_same(Card card1, Card card2);
    bool is_same(Card card1, Card card2, Card card3);
    bool is_complete(vector<Card>& acards, int d1, int d2, int flag = 0);
    bool is_complete(vector<Card>& acards, std::vector<std::vector<Card> >* mhole_cards = NULL);

    bool is_duidui_hu(vector<Card>& acards);    // 是不是7小对
    bool is_shi_san_yao(vector<Card>& acards);  // 十三幺

    bool is_haohua_qi_xiao_dui(vector<Card>& acards); //豪华七小对
    bool is_shuang_haohua_qi_xiao_dui(vector<Card>& acards); //双豪华七小对
    bool is_san_haohua_qi_xiao_dui(vector<Card>& acards); //三豪华七小对
    bool is_peng_peng_hu(vector<Card>& acards); // 是不是碰碰胡
    bool is_xiao_san_yuan(vector<Card>& acards); // 小三元
    bool is_da_san_yuan(vector<Card>& acards); // 大三元
    bool is_xiao_si_xi(vector<Card>& acards); // 小四喜
    bool is_da_si_xi(vector<Card>& acards); // 大四喜
    bool is_yao_jiu(vector<Card>& acards); // 幺九
    bool is_hun_yao_jiu(vector<Card>& acards); // 混幺九
    bool is_qing_yao_jiu(vector<Card>& acards); // 清幺九
    bool is_jiu_lian_bao_deng(vector<Card>& acards); // 九莲宝灯
    bool is_hun_peng(vector<Card>& acards); // 混碰
    bool is_qing_peng(vector<Card>& acards); // 清碰
    bool is_zi_yi_se(vector<Card>& acards); //字一色
    bool is_quan_qiu_ren(vector<Card>& acards); // 全求人
    bool is_qing_yi_se(vector<Card>& acards); // 清一色
    bool is_hun_yi_se(vector<Card>& acards); // 混一色
    bool is_ping_hu(vector<Card>& acards); //平胡
    bool is_ji_hu(vector<Card>& acards); // 鸡胡
    bool is_shi_ba_luo_han(vector<Card>& acards); //十八罗汉

    bool is_dai_yao_jiu(vector<Card>& acards);// 带幺九
    bool is_qing_qi_dui(vector<Card>& acards);// 清7对
    bool is_long_qi_dui(vector<Card>& acards);// 龙7对
    bool is_qing_long_qi_dui(vector<Card>& acards); // 清龙7对
    bool is_jin_gou_diao(vector<Card>& acards);   // 金钩钓
    bool is_jiang_jin_gou_diao(vector<Card>& acards); // 将金钩钓
    bool is_qing_jin_gou_diao(vector<Card>& acards); // 清金钩钓

    bool normal_cards_analysis(vector<Card> &newcards, int value, Card& card);
    bool ghost_cards_analysis(vector<Card> &newcards, int value, Card& card);
    bool hu_cards_analysis(vector<Card> &newcards);
    bool is_qing_shi_ba_luo_han(vector<Card>& acards);
    bool is_qing_dai_yao_jiu(vector<Card>& acards);
};

#endif /* _HOLE_CARDS_H_ */
