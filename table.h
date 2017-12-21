#ifndef _TABLE_H_
#define _TABLE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ev++.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <math.h>
#include <json/json.h>

#include <map>
#include <set>

#include "deck.h"
#include "hole_cards.h"
#include "jpacket.h"
#include "proto.h"
#include "replay.h"

class Player;
class Client;

#define MAX_JIABEI  8
enum JiType 
{
    YAO_BAI_JI = 0,          // 摇摆鸡
    BEN_JI = 1,              // 本鸡
    WU_GU_JI = 2,            // 乌骨鸡
    CHONG_FENG_WU_GU_JI = 3, // 冲锋乌骨鸡
    YAO_JI = 4,              // 幺鸡
    CHONG_FEN_JI = 5,        // 冲锋鸡
    ZE_REN_JI = 6,           // 责任鸡
    JIN_JI = 7,              // 金鸡
    BAO_JI = 8,              // 包鸡
};

struct ji_data 
{
    JiType type;
    int value;
};

enum FenType
{
    MING_GANG_TYPE = 0, // 杠分
    PENG_GANG_TYPE =1, 
    AN_GANG_TYPE = 2, 

    JIAO_PAI_TYPE = 3 ,     // 叫牌分，黄庄查叫时计算

    PAO_HU_TYPE = 4,        // 炮胡
    GANG_HU_TYPE = 5,       // 杠胡
    ZI_MO_TYPE = 6,         // 自摸

    YAO_BAI_JI_TYPE = 7,          // 摇摆鸡
    BEN_JI_TYPE = 8,              // 本鸡
    WU_GU_JI_TYPE = 9,            // 乌骨鸡
    CHONG_FENG_WU_GU_JI_TYPE = 10, // 冲锋乌骨鸡
    YAO_JI_TYPE = 11,              // 幺鸡
    CHONG_FENG_JI_TYPE = 12,        // 冲锋鸡
    ZE_REN_JI_TYPE = 13,           // 责任鸡
    JIN_JI_TYPE = 14,              // 金鸡
    BAO_JI_TYPE = 15,              // 包鸡
    LIAN_ZHUANG_TYPE = 16,         //连庄 
};

typedef struct
{
	int enable; // 0 : enable 1 : disable
    int seatid; // 
	int occupied; // seat
	int ready; // to play
	int betting; // betting
	
	int uid; // user id
	int	role; // 0 : Player 1 : Dealer
	HoleCards hole_cards;
	int card_type;
	int status; // 0 no ting, 1 ting
    int action;
	int bet;
	int huanpais;
    int ting;
    int hu;
    int jiabei;
    int robot_flag;
    int timeout_count;
    int dismiss;
    int gang_count[4]; // 0 是吃杠，1是碰杠，2是3+1，3是手上四个形成暗杠
    int fang_gang_count;
    int gang_hu_flag; // 抢杠胡
    int pao_hu_flag;
    int horse_count;
    int get_next_card_cnt; //玩家拿牌的次数

    std::map<int, int> peng_record; // 碰牌记录 seat id => count
    std::map<int, int> peng_card_record; // 碰牌记录 card => seatid
    std::vector<int> guo_hu_cards;    // 记录这一轮过胡不胡的牌
    std::vector<int> guo_peng_cards;  // 记录这一轮过碰不碰的牌
    int forbid_hu;                 // 给别人碰3次，禁胡
    int forbided_seatid;           // 给谁禁胡的人座位号

    vector<int> obsorb_seats;     // 吃、碰、杠的座位号
    vector<int> gang_seats;  // 杠的人位置
    vector<int> lian_gang_cards; // 连杠牌的记录 

    double jingdu;
    double weidu;

    int handler_flag; // 是否处理上次打出的牌的标志
    int last_actions[2];  // 记录该玩家最后两步动作
    int need_card;
    int ahead_start;
    vector<Card> horse_cards;
    vector<Card> zhong_horse_cards;
    vector<int> zhong_horse;
    list<int> actions;
    vector<ji_data> ji_pai;
    int has_jin_ji;//翻到金鸡则为1

    int total_zimo;
    int total_fang_pao;
    int total_jie_pao;
    int total_an_gang;
    int total_ming_gang;
    int total_zhong_horse;
    int total_zhong_ji; //这个只计幺鸡、乌骨鸡和翻出来的鸡牌
    int total_chong_feng_ji;
    int total_ze_ren_ji;
    int is_bao_ting; //是否为报听
    int lian_zhuang_cnt;//连续作庄
    int jiao_pai;
    int has_chong_feng_ji; //是否打出冲锋鸡
    int has_wu_gu_ji; //是否打出乌骨鸡
    int has_ze_ren_ji; //是否打出责任鸡
    int has_wu_gu_ze_ren_ji; //是否打出责任鸡
    int has_yao_ji; 

    int jiao_pai_max_score; //叫牌的分值
    int jiao_pai_max_type; //叫牌的类型
    int has_chong_feng_wu_gu_ji;
    string hu_pai_lei_xing; //记录胡牌玩家的字符串

    std::map<int, std::map<int, int> > score_from_players_detail; //每个玩家，每得得分的去向

	
    Player *player; 
	int already_get_red;
	int city_red_falg;
    int score;  // 基础分(含特殊牌型翻倍)
    vector<int> set_hole_cards;	    
    void clear(void)
    {
    	// enable = 0; // 手否激活
        // seatid = 0; // 座位id
		occupied = 0; // 是否被坐下

		ready = 0; // 是否准备（如果没有准备为0，准备了为1）
		betting = 0; // 是否在玩（如果弃牌即为0，玩牌中为1）

		uid = 0; // 用户id
		role = 0; // 是否是庄家
		hole_cards.clear(); // 保存三张底牌
		card_type = 0;
		status = 0; // 0 no ting, 1 ting
		bet = 0; // 个人投注
		huanpais = 3;
        action = 0;
        ting = 0;
        player = NULL;
        jiabei = 1;
        hu = 0;
        robot_flag = 0;
        timeout_count = 0;
        dismiss = 0;
        total_zimo = 0;
        total_fang_pao = 0;
        total_jie_pao = 0;
        total_an_gang = 0;
        total_ming_gang = 0;
        total_zhong_horse = 0;
        total_zhong_ji = 0;
        total_chong_feng_ji = 0;
        total_ze_ren_ji = 0;
        for (int i = 0; i < 4; i++)
        {
            gang_count[i] = 0;
            gang_hu_flag = 0;
        }

        fang_gang_count = 0;
        horse_count = 0;
        peng_record.clear();
        peng_card_record.clear();
        guo_hu_cards.clear();
        gang_seats.clear();
        zhong_horse_cards.clear();
        forbid_hu = 0;
        pao_hu_flag = 0;
        jingdu = 0.0f;
        weidu = 0.0f;
        handler_flag = 0;
        forbided_seatid = -1;
        obsorb_seats.clear();
        last_actions[0] = -1;
        last_actions[1] = -1;
        score = 0;
        need_card = 0;
        ahead_start = -1;
        horse_cards.clear();
        zhong_horse.clear();
        ji_pai.clear();
        lian_gang_cards.clear();
        guo_peng_cards.clear();
        actions.clear();

        total_zimo = 0;
        total_fang_pao = 0;
        total_jie_pao = 0;
        total_an_gang = 0;
        total_ming_gang = 0;
		already_get_red = 0;
		city_red_falg = 0;
        total_zhong_horse = 0;

        is_bao_ting = 0;
        has_chong_feng_ji = 0;
        has_wu_gu_ji = 0; 
        has_yao_ji = 0; 
        has_ze_ren_ji = 0; 
        has_wu_gu_ze_ren_ji = 0; 
        has_jin_ji = 0;
        jiao_pai_max_score = 0;
        jiao_pai_max_type = 0; 
        has_chong_feng_wu_gu_ji = 0;
        score_from_players_detail.clear();
        hu_pai_lei_xing = "";
        get_next_card_cnt = 0;
        set_hole_cards.clear();
    }

    void reset(void)
    {
		ready = 0;
		betting = 0;
		role = 0;
		hole_cards.clear();
        zhong_horse_cards.clear();
		card_type = 0;
		status = 0;
		bet = 0;
        action = 0;
		huanpais = 3;
        ting = 0;
        jiabei = 1;
        hu = 0;
        robot_flag = 0;
        timeout_count = 0;
        horse_count = 0;
        get_next_card_cnt = 0;

        for (int i = 0; i < 4; i++)
        {
            gang_count[i] = 0;
            gang_hu_flag = 0;
        }

        fang_gang_count = 0;
        peng_record.clear();
        peng_card_record.clear();
        guo_hu_cards.clear();
        gang_seats.clear();
        forbid_hu = 0;
        pao_hu_flag = 0;
        handler_flag = 0;
        forbided_seatid = -1;
        obsorb_seats.clear();
        last_actions[0] = -1;
        last_actions[1] = -1;
        score = 0;
        need_card = 0;
        ahead_start = -1;
        horse_cards.clear();
        zhong_horse.clear();
        ji_pai.clear();
        lian_gang_cards.clear();
        guo_peng_cards.clear();
        actions.clear();
        is_bao_ting = 0;
        has_chong_feng_ji = 0;
        has_wu_gu_ji = 0; 
        has_ze_ren_ji = 0; 
        has_wu_gu_ze_ren_ji = 0;
        jiao_pai_max_score = 0;
        jiao_pai_max_type = 0;
        has_chong_feng_wu_gu_ji = 0;
        has_jin_ji = 0;
        has_yao_ji = 0; 
        score_from_players_detail.clear();
        hu_pai_lei_xing = "";
        set_hole_cards.clear();
    }
} Seat;

class Table
{
public:
    int							tid;
    int                         ttid; //房间代号
    int             			vid;
	int							zid;
	int							type; // 闷
	float 						fee;
    int             			min_money;
	int							max_money;
	int							base_money;
	int							base_ratio;
	int							min_round;
	int							max_round;

	std::map<int, Player*>		players;
    int             			cur_players;
	int							ready_players;
	int 						betting_players;
	
	int							seat_max;
	Seat                        seats[4];

	Deck 						deck;	
	//
	State						state; // game state
	int							dealer;
	int							start_seat;
	int							cur_seat;
	int							cur_action;
	int							cur_bet;

	int							total_bet;
	int							win_bet;

	int 						vip[5];

	int 						ts;
    int                         round_ts;
    int                         fang_8_tong;
    ev_timer                    preready_timer;
    ev_tstamp                   preready_timer_stamp;	

	// int							cur_player;
    ev_timer                    ready_timer;
    ev_tstamp                   ready_timer_stamp;

    ev_timer                    start_timer;
    ev_tstamp                   start_timer_stamp;
	
    ev_timer                    bet_timer;
    ev_tstamp                   bet_timer_stamp;

    ev_timer                    compare_timer;
    ev_tstamp                   compare_timer_stamp;

    
    //ev_timer                    single_ready_timer;
    //ev_tstamp                   single_ready_timer_stamp;
    int                         cur_flow_mode;

    int                         actions[NOTICE_SIZE];
    int                         last_action;
    Card                        last_card;
    Card                        last_gang_card;
    int                         win_seatid;

    int                         owner_uid;
    int owner_sex;
	std::string 				owner_name;
	std::string 				owner_remote_ip;
	
    int                 		origin_owner_uid;
    bool                        transfer_flag;

    std::set<int>               dismiss;
    ev_timer                    dismiss_timer;
    ev_tstamp                   dismiss_timer_stamp;

    int                         dismiss_flag;
    int                         dismiss_uid;

    int                         has_feng;        // 是否有风牌
    int                         has_ghost;       // 是否有鬼牌
    int                         horse_num;       // 买马数量
    int                         hu_pair;         // 可否胡对子
    int                         round_count;    // 统计这一轮第多少局
    int                         max_play_board; // 这一轮玩多少局
    int                         fang_pao;   // 是否允许放炮胡
    int                         dead_double;

    int                         hu_seat;  // 记录胡牌的玩家座位号
    int                         hu_card;
    int                         get_card_seat; // 记录摸牌的玩家座位号
    int                         chu_seat;     // 记录出牌的玩家位置
    int                         gang_hu_seat; // 记录被抢杠胡的玩家位置
    int                         pao_hu_seat;  // 记录放炮的玩家位置

    int                         pao_hu_count; //统计放炮胡的人数，多于一个人则，一炮多响应
    int                         gang_hu_count; //统计抢杠胡的人数，多于一个则

    std::map<int, int>          forbid_hu_record; //禁止胡的玩家记录 seatid => forbid seatid
    std::vector<int>            zhong_horse;
    vector<ji_data>             ji_pai;

    int                         tian_hu_flag; //天胡
    int                         di_hu_flag; //地胡
    int                         hai_di_hu_flag; //海底胡
    int                         gang_shang_hua_flag; //杠上花
    int                         gang_shang_hua_seat; //明杠杠上花，被杠的玩家位置
    int                         gang_shang_pao; //杠上炮
    int                         quan_qiu_ren_pao; //全求人炮
    int                         hai_di_pao; //海底炮
    int                         lian_gang_flag; // 连杠杠上花标记
    int                         lian_gang_pao_flag; // 连杠杠上炮标记

    int                         chi_count;   // 统计吃
    int                         peng_count;  // 统计碰
    int                         gang_count;  // 统计杠

    int                         wait_handle_action; //等待决定处理的action
    int                         wait_handler_seat;  //等待决定处理的seat
	
	ev_timer                    ahead_start_timer;
	ev_tstamp                   ahead_start_timer_stamp;

	int                         ahead_start_flag;
    int                         ahead_start_uid;
    int							max_ready_players;
    int 						set_card_flag ; //是否是需要配牌的标记
    int                         game_end_flag;
    int                         create_rmb;

    int                         forbid_same_ip;
    int                         forbid_same_place;

    ev_timer 					subs_timer;
    ev_tstamp 					subs_timer_stamp;
	
	ev_timer ji_card_timer;
    ev_tstamp ji_card_stamp;
	ev_timer 					club_timer;
    ev_tstamp 					club_timer_stamp;
	int                         substitute;
    Replay                      replay;

    int                         ben_ji;
    int                         wu_gu_ji;
    int                         bao_ji;
    int                         is_re_pao; //是否为热炮
    int                         is_qiang_gang; //是否为抢杠
    int                         is_huang_zhuang; //是否为黄庄
    int                         jiao_pai_cnt; //黄庄时，叫牌的人数
    vector<Card>                horse_cards;
    int                         has_chong_feng_ji;
    int                         has_chong_feng_wu_gu_ji;
    int                         has_yao_ji;
    int                         has_wu_gu_ji; //是否打出乌骨鸡
    int                         has_ze_ren_ji; //是否有责任鸡
    int                         has_wu_gu_ze_ren_ji; //是否有责任鸡
    int                         horse_count;
    std::map<int, int>          score_from_players_total; //每个玩家的最终得分。
    std::map<int, std::map<int, int> > score_from_players_item_total; //每个玩家的每项羸得分。
    std::map<int, std::map<int, int> > score_from_players_item_count; //每个玩家的某种类型牌分次数。

    std::map<int, std::map<int, int> > score_to_players_item_total; //每个玩家的每项输出得分。
    std::map<int, std::map<int, int> > score_to_players_item_count; //每个玩家的某种类型牌分次数。
    int fang_ben_ji;
    int fang_fang_ji;
    int fang_yao_bai_ji1;
    int fang_yao_bai_ji2;
	
	int							cost_select_flag;		//付费选择标记
	std::vector<int>            redpackes;
    int 						red_type;

    int already_update_account_bet;
    int sha_bao;
    int fang_card;
    Json::Value                 config_of_replay;  //配置
	int 						red_open_flag;
	int 						red_open_count;
	int 						red_open_seat;
	int                         create_aa_rmb;
	int                         auto_flag;//判定是都是俱乐部自动开开房
    int                         clubid;//俱乐部id是多少
    int                         rmb_cost;//付费模式
    int                         create_from_club;
    string                      ruler;

	int                         game_end_count_flag;
    int                         need_return_diamo;
    //控制多人可操作成员的变量
    std::vector<int>      handler_hu_seats; //可进行胡操作的玩家
    std::vector<int>      handler_chi_seats;  
    std::vector<int>      handler_peng_seats;
    std::vector<int>      handler_gang_seats;
    std::vector<int>      handler_seats;  //需要进行操作的玩家。
    std::map<int, int>    handler_flags; //某个玩家是否处理了动作，0表示没有处理。
public:
    Table();
    virtual ~Table();
	int init(int my_tid, int my_vid, int my_zid, int my_type, float my_fee, int my_min_money,
				int my_max_money, int my_base_money, int my_base_ratio, int my_min_round, int my_max_round);

    void init_table_type(int set_type, int set_has_ghost, int set_has_feng, int set_hu_pair, int set_horse_num,
                         int set_max_borad, int set_fang_pao = 0, int set_dead_double = 1, int set_forbid_same_ip = 0,
                         int set_forbid_same_place = 0, int set_substitute = 0, int set_cost_select_flag = 1, int set_ben_ji = 0, int set_wu_gu_ji = 0, int set_bao_ji = 0,
						int set_auto_flag = 0, int set_clubid = 0, int set_create_from_club = 0);
	int set_card_ratio(int my_is_stack, int my_card_ratio_danpai, int my_card_ratio_duizi, int my_card_ratio_shunzi,
			int my_card_ratio_jinhua, int my_card_ratio_shunjin, int my_card_ratio_baozi);
	int get_card_type(int salt);
	int broadcast(Player *player, const std::string &packet);
	int unicast(Player *player, const std::string &packet);
	int random(int start, int end);
	int random(int start, int end, int seed);
	void reset();
	void vector_to_json_array(std::vector<Card> &cards, Jpacket &packet, string key);
	void map_to_json_array(std::map<int, Card> &cards, Jpacket &packet, string key);
    void map_to_json_array(std::map<int, int> &from_or_to, Json::Value &val, string key, string key2);
    void map_to_json_array(std::map<int, int> &count,Json::Value &val , string key);
	void json_array_to_vector(std::vector<Card> &cards, Jpacket &packet, string key);
	int sit_down(Player *player);
	void stand_up(Player *player);
	int del_player(Player *player);
	int handler_login(Player *player);
	int handler_login_succ_uc(Player *player);
	int handler_login_succ_bc(Player *player);
	int handler_table_info(Player *player);
	int handler_ready(Player *player);
	int handler_preready();
	static void preready_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void ready_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
    // static void single_ready_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void start_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
	int test_game_start();
	int game_start();
	int count_next_bet();
	int start_next_bet(int flag);
	static void bet_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void compare_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
    static void ready_out_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
	int handler_bet(Player *player);
    static void subs_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
    static void club_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
	void bet_timeout();
	void compare_timeout();
    int ready_timeout();
	void lose_update(Player *player);
    void update_money(Player* player, int value);
	void win_update(Player *player);

	int handler_fold(Player *player);
    int handler_chi(Player* player);
    int handler_chu(Player* player);
    int handler_peng(Player* player);
    int handler_gang(Player* player);
    int handler_hu(Player* player);
    int handler_guo(Player* player);
    int handler_ting(Player* player);
    int handler_cancel(Player* player);
    int handler_jiabei(Player* player);

	int test_game_end();
	int game_end(int flag = 0);
	int handler_logout(Player *player);
	int handler_chat(Player *player);
	int handler_face(Player *player);
	int handler_interFace(Player *player);
	int handler_player_info(Player *player);
	int handler_prop(Player *player);

    int check_ready_out();

	void handler_prop_error(Player *player, int prop_id);
    void handler_bet_error(Player *player, int action);

	int next_betting_seat(int pos);
	int prev_betting_seat(int pos);
	int count_betting_seats();
	int get_last_betting();

    int next_player_seat();

	int insert_flow_log(int flag);
    int handler_send_gift_req(Player *player);
    void send_result_to_robot();

    void init_dealer();
    void vector_to_json_array(std::list<Card> &cards, Jpacket &packet, string key);
    void vector_to_json_array(std::vector<Card> &cards, Json::Value &val, string key);
    void vector_to_json_array(std::vector<int> &values, Json::Value &val, string key);
    void vector_to_json_array(std::vector<ji_data> &cards, Json::Value &val, string key, string key2);
    void reset_actions();

    void clean_table();
    int handler_dismiss_table(Player *player);
    static void dismiss_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
    int dismiss_timeout();

    int insert_flow_record();
    int insert_flow_round_record();
    void broadcast_dismiss_status(int flag);

    void handler_net_status(Player* player, int status);
    void update_account_bet();
    int calculate_base_score(int sid, int pao, int card_value = -1 );

    void dump_hole_cards(std::vector<Card>& cards, int seatid, int action);

    void vector_to_json_string(std::vector<Card> &cards, Jpacket &packet, string key);

    void check_ip_conflict();

    void broadcast_forbid_hu(Player* player, int num);

    void handler_gps_notice(Player* player);

    void handler_gps_dist_req(Player* player);

    double GetDistance(double dLongitude1, double dLatitude1, double dLongitude2, double dLatitude2);

    void handler_voice_req(Player* player);


	void handler_start_game_req(Player *player);
	void broadcast_ahead_start_status(Player* player, int flag);
    string format_card_desc(int card_type, int seatid);
	static void ahead_start_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
	int ahead_start_timeout();
    int handler_need_card_req(Player *player);
    int handler_transfer_owner_req(Player *player);
    void handler_transfer_owner_req_error(Player* player, int reason);

    void accumulate_hu();
    int set_create_rmb(int rmb, int aarmb = 1);
    int handler_cmd(int cmd, Player* player);
	int handler_redpacket();
    int record_redpacket(Player *player, int value);
    int handler_get_redpacket_req(Player *player);
	int handler_invite_advantage(Player *player);
	int permit_join(Player* player);
    void handler_substitute_req(Player* player);
    void modify_substitute_info(Player* player, int flag);
    void modify_substitute_info(int status);
    void clear_substitute_info();
	void create_table_cost();		//创建房间扣费
	int random_dealer();

    int handler_owner_dismiss_table();
    int handler_get_internet_req(Player * player);
    void huang_zhuang_cha_jiao(); //黄庄查叫

    int next_player_seatid_of(int cur_player);
    int pre_player_seatid_of(int cur_player);
    int get_set_hole_cards(Player *player);

    static void ji_card_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
    void ji_game_end();
    void record_table_info();
    void handler_redpacket_open_condition();
    void handler_club_create_req(Player * player);
    void modify_club_info(Player* player, int flag);
    void modify_club_info(int status);
    void clear_club_info();
    void handler_club_auto_create_req(std::string playway_desc, int player_max , std::string name);
};

#endif
