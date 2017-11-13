#include "table.h"
#include "card.h"
#include "client.h"
#include "game.h"
#include "log.h"
#include "player.h"
#include "proto.h"
#include "zjh.h"
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

extern ZJH zjh;
extern Log mjlog;

//杠分值
#define MING_GANG_BET 1
#define PENG_GANG_BET 1
#define AN_GANG_BET 2
//鸡分值
#define YAO_BAI_JI_BET 1          // 摇摆鸡
#define BEN_JI_BET 1              // 本鸡
#define WU_GU_JI_BET 1            // 乌骨鸡
#define CHONG_FENG_WU_GU_JI_BET 2 // 冲锋乌骨鸡
#define YAO_JI_BET 1              // 幺鸡
#define CHONG_FEN_JI_BET 2        // 冲锋鸡
#define ZE_REN_JI_BET 2           // 责任鸡
#define JIN_JI_BET 2              // 金鸡
#define BAO_JI_BET 1              // 包鸡

#define PREREADY_TIME_OUT 6
#define SINGLE_READY_TIME_OUNT 8
#define READY_TIME_OUT 6
#define START_TIME_OUT 1
#define BET_TIME_OUT 10
#define COMPARE_TIME_OUT 7
#define READY_OUT_TIME_OUT 60
#define DISMISS_TIME_OUT 30

#define SUBS_TIME_OUT 108000

#define JI_CARD_TIME_OUT 1 

Table::Table() : preready_timer_stamp(PREREADY_TIME_OUT),
                 ready_timer_stamp(READY_TIME_OUT),
                 start_timer_stamp(START_TIME_OUT),
                 bet_timer_stamp(BET_TIME_OUT),
                 compare_timer_stamp(COMPARE_TIME_OUT),
                 dismiss_timer_stamp(DISMISS_TIME_OUT),
                 subs_timer_stamp(SUBS_TIME_OUT),
				 ji_card_stamp(JI_CARD_TIME_OUT)
{
    preready_timer.data = this;
    ev_timer_init(&preready_timer, Table::preready_timer_cb,
                  preready_timer_stamp, preready_timer_stamp);

    ready_timer.data = this;
    ev_timer_init(&ready_timer, Table::ready_timer_cb, ready_timer_stamp,
                  ready_timer_stamp);

    start_timer.data = this;
    ev_timer_init(&start_timer, Table::start_timer_cb, start_timer_stamp, start_timer_stamp);

    bet_timer.data = this;
    ev_timer_init(&bet_timer, Table::bet_timer_cb, bet_timer_stamp, bet_timer_stamp);

    compare_timer.data = this;
    ev_timer_init(&compare_timer, Table::compare_timer_cb, compare_timer_stamp, compare_timer_stamp);
    //single_ready_timer.data = this;
    //ev_timer_init(&single_ready_timer, Table::single_ready_timer_cb, single_ready_timer_stamp, single_ready_timer_stamp);

    dismiss_timer_stamp = zjh.conf["tables"]["dismiss_time"].asInt();
    dismiss_timer.data = this;
    ev_timer_init(&dismiss_timer, Table::dismiss_timer_cb, dismiss_timer_stamp, dismiss_timer_stamp);
    ahead_start_timer_stamp = zjh.conf["tables"]["ahead_time"].asInt();
    ahead_start_timer.data = this;
    ev_timer_init(&ahead_start_timer, Table::ahead_start_timer_cb, ahead_start_timer_stamp, ahead_start_timer_stamp);

    subs_timer.data = this;
    ev_timer_init(&subs_timer, Table::subs_timer_cb, subs_timer_stamp, subs_timer_stamp);

	ji_card_timer.data = this;
    ev_timer_init(&ji_card_timer, Table::ji_card_timer_cb, ji_card_stamp, ji_card_stamp);

    cur_flow_mode = FLOW_END;

    owner_uid = -1;
    owner_sex = -1;
    owner_name = "";
    owner_remote_ip = "";
    origin_owner_uid = -1;
    transfer_flag = false;
    game_end_flag = 0;
}

Table::~Table()
{
    ev_timer_stop(zjh.loop, &preready_timer);
    ev_timer_stop(zjh.loop, &ready_timer);
    ev_timer_stop(zjh.loop, &start_timer);
    ev_timer_stop(zjh.loop, &bet_timer);
    ev_timer_stop(zjh.loop, &compare_timer);

    ev_timer_stop(zjh.loop, &dismiss_timer);
    ev_timer_stop(zjh.loop, &ahead_start_timer);
    ev_timer_stop(zjh.loop, &subs_timer);
	ev_timer_stop(zjh.loop, &ji_card_timer);
}

int Table::init(int my_tid, int my_vid, int my_zid, int my_type, float my_fee,
                int my_min_money, int my_max_money, int my_base_money, int my_base_ratio,
                int my_min_round, int my_max_round)
{
    // mjlog.debug("begin to init table [%d]\n", table_id);
    tid = my_tid;
    ttid = my_tid;
    vid = my_vid;
    zid = my_zid;
    type = my_type;
    fee = my_fee;
    min_money = my_min_money;
    max_money = my_max_money;
    base_money = my_base_money;
    base_ratio = my_base_ratio;
    min_round = my_min_round;
    max_round = my_max_round;
    seat_max = 4;
    max_play_board = 4;
    round_count = 0;
    fang_8_tong = 0;
    cur_players = 0;
    players.clear();
    ready_players = 0;
    

    reset();
    dismiss_flag = 0;
    dismiss_uid = 0;
    hu_seat = -1;
    get_card_seat = -1;
    gang_hu_seat = -1;
    pao_hu_seat = -1;
    pao_hu_count = 0;
    gang_hu_count = 0;

    tian_hu_flag = 0;        //天胡
    di_hu_flag = 0;          //地胡
    hai_di_hu_flag = 0;      //海底胡
    gang_shang_hua_flag = 0; //杠上花
    gang_shang_hua_seat = -1;
    gang_shang_pao = 0;   //杠上炮
    quan_qiu_ren_pao = 0; //全求人炮
    hai_di_pao = 0;       //海底炮
    lian_gang_flag = 0;
    lian_gang_pao_flag = 0;

    chi_count = 0;
    peng_count = 0;
    gang_count = 0;
    max_ready_players = seat_max;
    create_rmb = 0;

    state = ROOM_WAIT_GAME;
    redpackes.clear();
    red_type = 0;
    is_re_pao = 0;     //热炮
    is_qiang_gang = 0; //抢扛
    jiao_pai_cnt = 0;
    has_chong_feng_ji = 0;
    has_chong_feng_wu_gu_ji = 0;
    has_ze_ren_ji = 0;
    has_wu_gu_ze_ren_ji = 0;
    has_wu_gu_ji = 0;
    has_yao_ji = 0;
    is_huang_zhuang = 0;

    score_from_players_item_count.clear();
    score_from_players_item_total.clear();
    score_to_players_item_count.clear();
    score_to_players_item_total.clear();

    ji_pai.clear();
    set_card_flag = zjh.conf["tables"]["set_card_flag"].asInt();
    ts = time(NULL);

    return 0;
}

void Table::init_table_type(int set_type, int set_has_ghost, int set_has_feng, int set_hu_pair, int set_horse_num,
                            int set_max_board, int set_fang_pao, int set_dead_double, int set_forbid_same_ip,
                            int set_forbid_same_place, int set_substitute, int set_cost_select_flag, int set_ben_ji, int set_wu_gu_ji, int set_bao_ji)
{
    type = set_type;
    has_ghost = set_has_ghost;
    has_feng = set_has_feng;
    hu_pair = set_hu_pair;
    horse_num = set_horse_num;
    redpackes.clear();
    max_play_board = set_max_board;
    fang_pao = set_fang_pao;
    substitute = set_substitute;
    cost_select_flag = set_cost_select_flag;
    ts = time(NULL);
    ahead_start_flag = 0;
    ahead_start_uid = -1;
    dead_double = set_dead_double;
    for (int i = 0; i < seat_max; i++)
    {
        seats[i].clear();
        seats[i].seatid = i;
        seats[i].lian_zhuang_cnt = 1; //连续作庄
        seats[i].jiao_pai = 0;
    }
    fang_8_tong = 0;
    forbid_same_ip = set_forbid_same_ip;
    forbid_same_place = set_forbid_same_place;
    ben_ji = set_ben_ji;
    wu_gu_ji = set_wu_gu_ji;
    fang_ben_ji = 0;
    fang_fang_ji = 0;
    fang_yao_bai_ji1 = 0;
    fang_yao_bai_ji2 = 0;
    bao_ji = set_bao_ji;
    already_update_account_bet = 0;
    sha_bao = 0;
    deck.init(has_feng, has_ghost, horse_num, hu_pair, 0);
}

int Table::get_card_type(int salt)
{
    int tmp = random(0, 99, salt);
    mjlog.debug("get_card_type[%d] salt[%d].\n", tmp, salt);
    return 0;
}

int Table::broadcast(Player *p, const std::string &packet)
{
    Player *player;
    std::map<int, Player *>::iterator it;
    for (it = players.begin(); it != players.end(); it++)
    {
        player = it->second;
        if (player == p || player->client == NULL)
        {
            continue;
        }
        player->client->send(packet);
    }

    return 0;
}

int Table::unicast(Player *p, const std::string &packet)
{
    if (p->client)
    {
        return p->client->send(packet);
    }
    return -1;
}

int Table::random(int start, int end)
{
    srand((unsigned)time(NULL));
    return start + rand() % (end - start + 1);
}

int Table::random(int start, int end, int seed)
{
    srand((unsigned)time(NULL) + seed);
    return start + rand() % (end - start + 1);
}

void Table::reset()
{
    state = READY;
    ready_players = 0;
    start_seat = 0;
    cur_seat = 0;
    cur_action = 0;
    cur_bet = 0;
    total_bet = 0;
    last_action = 0;
    last_card.set_value(0);
    last_gang_card.set_value(0);
    win_seatid = -1;
    //  gang_hu_seat = -1; //确定下次庄的时候要用到，庄确定后，再重置
    //  pao_hu_seat = -1;
    pao_hu_count = 0;
    gang_hu_count = 0;

    tian_hu_flag = 0;        //天胡
    di_hu_flag = 0;          //地胡
    hai_di_hu_flag = 0;      //海底胡
    gang_shang_hua_flag = 0; //杠上花
    gang_shang_pao = 0;      //杠上炮
    quan_qiu_ren_pao = 0;
    hai_di_pao = 0;
    gang_shang_hua_seat = -1;
    lian_gang_flag = 0;
    lian_gang_pao_flag = 0;
    fang_ben_ji = 0;
    fang_fang_ji = 0;
    fang_yao_bai_ji1 = 0;
    fang_yao_bai_ji2 = 0;
    chi_count = 0;
    peng_count = 0;
    horse_count = 0;
    gang_count = 0;

    wait_handle_action = -1;
    wait_handler_seat = -1;

    forbid_hu_record.clear();
    ahead_start_flag = 0;
    ahead_start_uid = -1;
    fang_8_tong = 0;
    horse_num = 1;
    is_re_pao = 0;
    is_qiang_gang = 0;
    jiao_pai_cnt = 0;
    has_chong_feng_ji = 0;
    has_chong_feng_wu_gu_ji = 0;
    has_ze_ren_ji = 0;
    has_wu_gu_ze_ren_ji = 0;
    has_wu_gu_ji = 0;
    has_yao_ji = 0;
    is_huang_zhuang = 0;
    ji_pai.clear();
    score_from_players_total.clear();
    reset_actions();
    score_from_players_item_count.clear();
    score_from_players_item_total.clear();
    score_to_players_item_count.clear();
    score_to_players_item_total.clear();
    already_update_account_bet = 0;
    sha_bao = 0;
}
void Table::vector_to_json_array(std::vector<ji_data> &cards, Json::Value &val, string key, string key2)
{
    for (unsigned int i = 0; i < cards.size(); i++)
    {
        val[key].append(cards[i].value);
        val[key2].append(cards[i].type);
    }
}

void Table::vector_to_json_array(std::vector<Card> &cards, Jpacket &packet, string key)
{
    for (unsigned int i = 0; i < cards.size(); i++)
    {
        packet.val[key].append(cards[i].value);
    }

    if (cards.size() == 0)
    {
        packet.val[key].append(0);
    }
}

void Table::vector_to_json_array(std::vector<Card> &cards, Json::Value &val, string key)
{
    for (unsigned int i = 0; i < cards.size(); i++)
    {
        val[key].append(cards[i].value);
    }
}

void Table::vector_to_json_array(std::vector<int> &values, Json::Value &val, string key)
{
    for (unsigned int i = 0; i < values.size(); i++)
    {
        val[key].append(values[i]);
    }
}

void Table::vector_to_json_array(std::list<Card> &cards, Jpacket &packet, string key)
{
    std::list<Card>::iterator it = cards.begin();
    for (; it != cards.end(); it++)
    {
        packet.val[key].append(it->value);
    }

    if (cards.size() == 0)
    {
        packet.val[key].append(0);
    }
}

void Table::map_to_json_array(std::map<int, int> &from_or_to, Json::Value &val,
                              string key, string key2)
{
    std::map<int, int>::iterator it;
    for (it = from_or_to.begin(); it != from_or_to.end(); it++)
    {
        if (it->second == 0)
            continue;
        val[key].append(it->first);
        val[key2].append(it->second);
    }
}

void Table::map_to_json_array(std::map<int, int> &count, Json::Value &val,
                              string key)
{
    std::map<int, int>::iterator it;
    for (it = count.begin(); it != count.end(); it++)
    {
        if (it->second == 0)
            continue;
        val[key].append(it->second);
    }
}

void Table::map_to_json_array(std::map<int, Card> &cards, Jpacket &packet,
                              string key)
{
    std::map<int, Card>::iterator it;
    for (it = cards.begin(); it != cards.end(); it++)
    {
        Card &card = it->second;
        packet.val[key].append(card.value);
    }
}

void Table::json_array_to_vector(std::vector<Card> &cards, Jpacket &packet,
                                 string key)
{
    Json::Value &val = packet.tojson();

    for (unsigned int i = 0; i < val[key].size(); i++)
    {
        Card card(val[key][i].asInt());

        cards.push_back(card);
    }
}

int Table::handler_login(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    bool is_create = (val["cmd"].asInt() == CLIENT_CREATE_TABLE_REQ);

    if (substitute == 1 && is_create)
    {
        handler_substitute_req(player);
        ev_timer_again(zjh.loop, &subs_timer);
        return 0;
    }

    if (players.find(player->uid) == players.end())
    {
        players[player->uid] = player;
        player->tid = tid;
        // todo check.
        player->seatid = sit_down(player);
        Seat &seat = seats[player->seatid];
        // seat.ready = 0;
        seat.uid = player->uid;
        if (player->seatid < 0)
        {
            return -1;
        }
        cur_players++;

        handler_login_succ_uc(player);
        handler_login_succ_bc(player);

        if (is_create)
        {
            owner_uid = player->uid;
            owner_name = player->name;
            owner_remote_ip = player->remote_ip;
            owner_sex = player->sex;
            origin_owner_uid = owner_uid;
            state = ROOM_WAIT_GAME;
        }

        handler_table_info(player);

        if (player->uid < 1000 && player->uid > 100)
        {
            handler_ready(player);
        }

        if (substitute == 1)
        {
            modify_substitute_info(player, 1);
        }

        mjlog.info("handler login succ uid[%d] money[%d] cur_players[%d] tid[%d].\n", player->uid, player->money, cur_players, tid);

        return 0;
    }

    return -1;
}

int Table::sit_down(Player *player)
{
    std::vector<int> tmp;
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].occupied == 0)
        {
            tmp.push_back(i);
        }
    }

    int len = tmp.size();
    if (len > 0)
    {
        int index = random(0, len - 1);
        int i = tmp[index];
        mjlog.debug("len[%d] index[%d] i[%d]\n", len, index, i);
        // seats[i].reset();
        seats[i].occupied = 1;
        seats[i].player = player;
        return i;
    }

    return -1;
}

void Table::stand_up(Player *player)
{
    seats[player->seatid].clear();
}

int Table::del_player(Player *player)
{
    if (players.find(player->uid) == players.end())
    {
        mjlog.debug("player uid[%d] talbe del_player is error.", player->uid);
        return -1;
    }
    Seat &seat = seats[player->seatid];
    if (seat.ready == 1)
    {
        ready_players--;
    }
    player->stop_offline_timer();
    players.erase(player->uid);
    stand_up(player);
    cur_players--;

    modify_substitute_info(player, 0);

    return 0;
}

int Table::handler_login_succ_uc(Player *player)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_LOGIN_SUCC_UC;
    packet.val["vid"] = player->vid;
    packet.val["zid"] = player->zid;
    packet.val["tid"] = player->tid;
    packet.end();
    unicast(player, packet.tostring());

    return 0;
}

// SERVER_LOGIN_SUCC_BC
int Table::handler_login_succ_bc(Player *player)
{
    Seat &seat = seats[player->seatid];

    Jpacket packet;
    packet.val["cmd"] = SERVER_LOGIN_SUCC_BC;
    // packet.val["vid"] = player->vid;
    // packet.val["zid"] = player->zid;
    // packet.val["tid"] = player->tid;
    packet.val["seatid"] = player->seatid;
    packet.val["ready"] = seat.ready;
    packet.val["betting"] = seat.betting;
    packet.val["role"] = seat.role;
    packet.val["status"] = seat.status;
    packet.val["bet"] = seat.bet;

    packet.val["uid"] = player->uid;
    packet.val["name"] = player->name;
    packet.val["sex"] = player->sex;
    packet.val["avatar"] = player->avatar;
    packet.val["zone"] = player->zone;

    packet.val["rmb"] = player->rmb;
    packet.val["money"] = player->money;
    packet.val["total_board"] = player->total_board;
    packet.val["pay_total"] = player->pay_total;

    packet.val["ip"] = player->remote_ip;
    packet.end();

    broadcast(player, packet.tostring());
    return 0;
}

int Table::handler_table_info(Player *player)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_TABLE_INFO_UC;
    packet.val["vid"] = player->vid;
    packet.val["zid"] = player->zid;
    packet.val["tid"] = player->tid;
    packet.val["ttid"] = ttid;
    packet.val["seatid"] = player->seatid;

    if (round_count == 0)
    {
        state = ROOM_WAIT_GAME;
    }
    packet.val["state"] = state;
    packet.val["dealer"] = dealer;
    packet.val["cur_seat"] = cur_seat;
    packet.val["cur_bet"] = cur_bet;
    packet.val["total_bet"] = total_bet;
    packet.val["base_money"] = base_money;
    packet.val["remain_cards"] = deck.size();
    packet.val["robot_flag"] = seats[player->seatid].robot_flag;
    packet.val["owner_uid"] = owner_uid;
    packet.val["cur_round"] = round_count;
    packet.val["total_round"] = max_play_board;
    packet.val["dismiss"] = dismiss_flag;

    packet.val["has_feng"] = has_feng;
    packet.val["has_ghost"] = has_ghost;
    packet.val["hu_pair"] = hu_pair;
    packet.val["horse_num"] = horse_num;
    packet.val["max_play_count"] = max_play_board;
    packet.val["table_type"] = type;
    packet.val["fang_pao"] = fang_pao;
    packet.val["ahead_start_flag"] = ahead_start_flag;
    packet.val["ahead_start_uid"] = ahead_start_uid;
    packet.val["chu_seat"] = chu_seat;
    packet.val["chu_card"] = last_card.value;
    packet.val["cost_select_flag"] = cost_select_flag;
    mjlog.debug("handler_table_info redpackes.size() [%d] seats[%d].already_get_red [%d]\n",
                redpackes.size(), player->seatid, seats[player->seatid].already_get_red);
    if (redpackes.size() > 0 && seats[player->seatid].already_get_red == 0)
    {
        packet.val["get_red"] = 1;
    }
    else
    {
        packet.val["get_red"] = 0;
    }

    packet.val["ben_ji"] = ben_ji;
    packet.val["bao_ji"] = bao_ji;
    packet.val["wu_gu_ji"] = wu_gu_ji;
    //packet.val["cost_select_flag"] = cost_select_flag;
    if (has_ghost == 1)
    {
        packet.val["ghost_card"] = Card::ZhongV;
    }
    else
    {
        packet.val["ghost_card"] = 0;
    }
    packet.val["trun_card"] = 0;

    packet.val["tian_hu_flag"] = tian_hu_flag;               //天胡
    packet.val["di_hu_flag"] = di_hu_flag;                   //地胡
    packet.val["hai_di_hu_flag"] = hai_di_hu_flag;           //海底胡
    packet.val["gang_shang_hua_flag"] = gang_shang_hua_flag; //杠上花
    packet.val["gang_shang_pao"] = gang_shang_pao;           //杠上炮
    packet.val["dead_double"] = dead_double;
    packet.val["substitute"] = substitute;

    if (dismiss_flag)
    {
        packet.val["dismiss_time"] = (int)ev_timer_remaining(zjh.loop, &dismiss_timer);
        packet.val["dismiss_uid"] = dismiss_uid;
    }
    else
    {
        packet.val["dismiss_time"] = 0;
        packet.val["dismiss_uid"] = 0;
    }

    if (ahead_start_flag)
    {
        packet.val["ahead_start_time"] = (int)ev_timer_remaining(zjh.loop, &ahead_start_timer);
    }
    else
    {
        packet.val["ahead_start_time"] = 0;
    }

    if (state == BETTING)
    {
        packet.val["remain_time"] = ev_timer_remaining(zjh.loop, &bet_timer);
    }

    packet.val["forbid_same_ip"] = forbid_same_ip;
    packet.val["forbid_same_place"] = forbid_same_place;

    std::map<int, Player *>::iterator it;
    int i = 0;
    for (it = players.begin(); it != players.end(); it++)
    {
        Player *p = it->second;
        Seat &seat = seats[p->seatid];

        packet.val["players"][i]["uid"] = p->uid;
        packet.val["players"][i]["seatid"] = p->seatid;
        packet.val["players"][i]["ready"] = seat.ready;
        packet.val["players"][i]["betting"] = seat.betting;
        packet.val["players"][i]["role"] = seat.role;
        packet.val["players"][i]["status"] = seat.status;
        packet.val["players"][i]["bet"] = seat.bet;
        packet.val["players"][i]["robot_flag"] = seat.robot_flag;
        packet.val["players"][i]["dismiss"] = seat.dismiss;
        packet.val["players"][i]["ahead_start"] = seat.ahead_start;
        packet.val["players"][i]["net_status"] = (p->client == NULL) ? 0 : 1;

        packet.val["players"][i]["forbid"] = seat.forbid_hu;
        packet.val["players"][i]["forbid_hu"] = seat.forbid_hu;
        packet.val["players"][i]["forbided_seatid"] = seat.forbided_seatid;
        packet.val["players"][i]["has_chong_feng_ji"] = seat.has_chong_feng_ji;
        packet.val["players"][i]["has_chong_feng_wu_gu_ji"] = seat.has_chong_feng_wu_gu_ji; //这个字段客端用的has_wu_gu_ji
        packet.val["players"][i]["has_ze_ren_ji"] = seat.has_ze_ren_ji;
        packet.val["players"][i]["has_wu_gu_ze_ren_ji"] = seat.has_wu_gu_ze_ren_ji;

        for (unsigned int j = 0; j < seat.obsorb_seats.size(); j++)
        {
            packet.val["players"][i]["obsorb_seats"].append(seat.obsorb_seats[j]);
        }

        if (player == p)
        {
            if (state == BETTING)
            {
                std::vector<Card>::iterator vit;

                if (seat.hole_cards.cards.size() % 3 == 1)
                {
                    vit = seat.hole_cards.cards.begin();
                    for (; vit != seat.hole_cards.cards.end(); vit++)
                    {
                        packet.val["players"][i]["holes"].append(vit->value);
                    }
                }
                else
                {
                    vit = seat.hole_cards.oldcards.begin();
                    for (; vit != seat.hole_cards.oldcards.end(); vit++)
                    {
                        packet.val["players"][i]["holes"].append(vit->value);
                    }
                }

                if (seat.gang_hu_flag == 1 || seat.pao_hu_flag == 1)
                {
                    packet.val["players"][i]["pao_hu_card"] = hu_card;
                }

                int osize = seat.hole_cards.obsorb_cards.size();
                for (int j = 0; j < osize; j++)
                {
                    vit = seat.hole_cards.obsorb_cards[j].begin();
                    for (; vit != seat.hole_cards.obsorb_cards[j].end(); vit++)
                    {
                        packet.val["players"][i]["obsorb_holes"][j].append(vit->value);
                    }
                }

                std::list<Card>::iterator lit = seat.hole_cards.discard_cards.begin();

                for (; lit != seat.hole_cards.discard_cards.end(); lit++)
                {
                    packet.val["players"][i]["discard_holes"].append(lit->value);
                }

                packet.val["players"][i]["ting_flag"] = seat.ting;
                if (seat.ting == 1)
                {
                    osize = seat.hole_cards.hu_cards.size();

                    for (int j = 0; j < osize; j++)
                    {
                        packet.val["players"][i]["hu_cards"].append(seat.hole_cards.hu_cards[j].value);
                    }
                }

                if (cur_seat == seat.seatid)
                {
                    for (int j = 0; j < NOTICE_SIZE; j++)
                    {
                        packet.val["players"][i]["action"].append(actions[j]);
                    }

                    if (actions[NOTICE_GANG] == 1)
                    {
                        packet.val["gang_flag"] = seat.hole_cards.gang_flags.back();
                        vector_to_json_array(seat.hole_cards.gang_cards, packet, "gang_cards");
                        for (unsigned int j = 0; j < seat.hole_cards.gang_flags.size(); j++)
                        {
                            packet.val["gang_flags"].append(seat.hole_cards.gang_flags[j]);
                        }
                    }

                    // if (actions[NOTICE_TING] == 1)
                    // {
                        int index = 0;
                        std::map<int, vector<Card> >::iterator it = seat.hole_cards.ting_cards.begin();
                        for (; it != seat.hole_cards.ting_cards.end(); it++)
                        {
                            packet.val["ting_cards"].append(it->first);
                            int size = it->second.size();
                            for (int j = 0; j < size; j++)
                            {
                                packet.val["ting_pattern"][index].append(it->second[j].value);
                            }
                            index++;
                        }
                    // }

                    if (actions[NOTICE_CHI] == 1)
                    {
                        vector_to_json_array(seat.hole_cards.eat_cards, packet, "chi_cards");
                        packet.val["card"] = 0;
                        packet.val["chi_card"] = last_card.value;
                    }

                    if (actions[NOTICE_PENG] == 1)
                    {
                        packet.val["card"] = 0;
                        packet.val["peng_card"] = last_card.value;
                    }

                    if (actions[NOTICE_HU] == 1)
                    {
                        if (seat.hole_cards.size() % 3 == 1)
                        {
                            packet.val["card"] = 0;
                            packet.val["hu_card"] = hu_card;
                            packet.val["players"][i]["pao_hu_card"] = hu_card;
                        }
                        else
                        {
                            packet.val["card"] = seat.hole_cards.last_card.value;
                            packet.val["hu_card"] = seat.hole_cards.last_card.value;
                        }
                    }
                }
            }
        }
        else
        {
            if (state == BETTING)
            {
                std::vector<Card>::iterator it = seat.hole_cards.cards.begin();
                for (; it != seat.hole_cards.cards.end(); it++)
                {
                    packet.val["players"][i]["holes"].append(0);
                }

                int osize = seat.hole_cards.obsorb_cards.size();
                for (int j = 0; j < osize; j++)
                {
                    it = seat.hole_cards.obsorb_cards[j].begin();

                    if (seat.hole_cards.obsorb_cards[j].size() == 4 && it->value == 0)
                    {
                        packet.val["players"][i]["obsorb_holes"][j].append(0);
                        packet.val["players"][i]["obsorb_holes"][j].append(0);
                        packet.val["players"][i]["obsorb_holes"][j].append(0);
                        packet.val["players"][i]["obsorb_holes"][j].append(0);
                        continue;
                    }

                    for (; it != seat.hole_cards.obsorb_cards[j].end(); it++)
                    {
                        packet.val["players"][i]["obsorb_holes"][j].append(it->value);
                    }
                }

                std::list<Card>::iterator lit = seat.hole_cards.discard_cards.begin();

                for (; lit != seat.hole_cards.discard_cards.end(); lit++)
                {
                    packet.val["players"][i]["discard_holes"].append(lit->value);
                }

                packet.val["players"][i]["ting_flag"] = seat.ting;
            }
        }

        packet.val["players"][i]["name"] = p->name;
        packet.val["players"][i]["sex"] = p->sex;
        packet.val["players"][i]["avatar"] = p->avatar;
        packet.val["players"][i]["rmb"] = p->rmb;
        packet.val["players"][i]["money"] = p->money;
        packet.val["players"][i]["ip"] = p->remote_ip;

        i++;
    }

    packet.end();
    unicast(player, packet.tostring());

    check_ip_conflict();

    zjh.game->set_in_game_flag(player, 1);
    return 0;
}

// int Table::single_ready_timeout()
// {
//     ready_players = 0;
// 	for (int i = 0; i < seat_max; i++) {
// 		if (seats[i].occupied == 1) {
// 			Player *player = seats[i].player;
// 			if (player == NULL)
// 				continue;
// 			seats[i].ready = 1;
// 			seats[i].betting = 1;
//             ready_players++;
// 		}
// 	}

//     int ret = test_game_start();

//     if (ret != 0)
//     {
//         ev_timer_again(zjh.loop, &single_ready_timer);
//     }

// 	return 0;
// }

int Table::handler_ready(Player *player)
{
    if (state != READY && state != ROOM_WAIT_GAME)
    {
        mjlog.error("handler_ready state[%d]\n", state);
        return -1;
    }

    if (seats[player->seatid].ready == 1)
    {
        mjlog.error("player[%d] have been seted for game ready\n", player->uid);
        return -1;
    }

    player->stop_offline_timer();

    ready_players++;
    seats[player->seatid].ready = 1;
    seats[player->seatid].betting = 1;

    Jpacket packet;
    packet.val["cmd"] = SERVER_READY_SUCC_BC;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.end();
    broadcast(NULL, packet.tostring());

    mjlog.debug("handler_ready cur_players[%d], ready_players[%d]\n", cur_players, ready_players);

    if (ready_players == cur_players)
    {
        if (ready_players >= max_ready_players)
        {
            return test_game_start();
        }
    }

    // if (ready_players == 2)
    // {
    //     ev_timer_stop(zjh.loop, &ready_timer);
    // }

    return 0;
}

int Table::handler_preready()
{
    if (game_end_flag == 1)
    {
        clean_table();
        game_end_flag = 0;
        return 0;
    }

    for (int i = 0; i < seat_max; i++)
    {
        seats[i].reset();
    }

    reset();

    std::map<int, Player *>::iterator it;
    for (it = players.begin(); it != players.end(); it++)
    {
        Player *player = it->second;
        player->reset();
    }

    Jpacket packet;
    packet.val["cmd"] = SERVER_GAME_PREREADY_BC;
    for (it = players.begin(); it != players.end(); it++)
    {
        Player *player = it->second;
        packet.val["seatids"].append(player->seatid);
    }
    packet.end();
    broadcast(NULL, packet.tostring());

    ev_timer_again(zjh.loop, &ready_timer);

    return 0;
}

void Table::preready_timer_cb(struct ev_loop *loop, struct ev_timer *w,
                              int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->preready_timer);
    table->handler_preready();
}

void Table::ready_timer_cb(struct ev_loop *loop, struct ev_timer *w,
                           int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->ready_timer);

    table->ready_timeout();
}

int Table::ready_timeout()
{
    test_game_start();

    // if (ret != 0)
    // {
    //     ev_timer_again(zjh.loop, &ready_timer);
    // }

    return 0;
}

void Table::start_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->start_timer);

    table->start_next_bet(0);
}

int Table::test_game_start()
{
    int start_players = std::max(max_ready_players, 2);

    if (ready_players < start_players)
    {
        ev_timer_stop(zjh.loop, &ready_timer);
        mjlog.debug("current ready players less than 2\n");
        return 1;
    }

    if (state != READY && state != ROOM_WAIT_GAME)
    {
        mjlog.debug("game state isn't Ready\n");
        return 2;
    }

    ev_timer_stop(zjh.loop, &ready_timer);
    /* here to start the first board */
    game_start();

    return 0;
}

int Table::game_start()
{
    ev_timer_stop(zjh.loop, &subs_timer);
    ts = time(NULL);
    Replay::init_handler();
    // replay.append_record(config_of_replay);
    state = BETTING;
    mjlog.debug("game start.\n");
    cur_flow_mode = zjh.game->flow_mode;
    mjlog.info("current flow mode is %d, tid %d.\n", cur_flow_mode, tid);

    cur_bet = base_money;
    total_bet = 0;

    deck.fill();
    deck.shuffle(tid);
    //deck.debug();

    if (round_count == 0)
    {
        round_ts = time(NULL);
        if (substitute == 1)
        {
            modify_substitute_info(1);
            zjh.game->del_subs_table(owner_uid);
        }
    }

    replay.init(ts, ttid); //记录回放
    round_count++;

    if (round_count == 2 && max_ready_players == seat_max)
    {
        handler_redpacket();
    }

    init_dealer();
    for (int i = 0; i < seat_max; i++)
    {
        if (i == dealer)
            continue;
        seats[i].lian_zhuang_cnt = 1; //连续作庄
    }
    record_table_info();
    hu_seat = -1;
    pao_hu_seat = -1;
    gang_hu_seat = -1;

    int current_betting_seats = count_betting_seats();
    int next = next_betting_seat(dealer);

    for (int c = 0; c < current_betting_seats; c++)
    {
        Seat &seat = seats[next];
        Player *player = seat.player;
        if (player == NULL)
        {
            mjlog.warn("game start player is NULL seatid[%d] tid[%d].\n", next, tid);
            continue;
        }

        if (set_card_flag == 1)
		{
			get_set_hole_cards(player);
		}
        Jpacket packet;
        for (int j = 0; j < seat_max; j++)
        {
            Seat &seat = seats[j];
            Player *player = seat.player;
            if (seat.ready == 0)
            {
                continue;
            }
            packet.val["uids"].append(player->uid);
            packet.val["seats"].append(player->seatid);
            packet.val["names"].append(player->name);
            packet.val["sexs"].append(player->sex);
        }
        packet.val["cmd"] = SERVER_GAME_START_BC;
        packet.val["uid"] = player->uid;
        packet.val["seatid"] = player->seatid;
        packet.val["dealer"] = dealer;
        packet.val["base_money"] = base_money;
        packet.val["remain_cards"] = int(deck.cards.size());
        packet.val["cur_round"] = round_count;
        packet.val["total_round"] = max_play_board;
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].ready != 1)
            {
                continue;
            }
            packet.val["cur_seats"].append(i);
        }

        if (seat.set_hole_cards.size() == 13 && set_card_flag == 1)
		{
			for(unsigned int k = 0; k < seat.set_hole_cards.size(); k ++)
			{
				deck.get_next_card(seat.hole_cards, seat.set_hole_cards[k]);
			}
			deck.get_count = 0;
		}
		else 
		{
			deck.get_hole_cards(seat.hole_cards);
		}
        dump_hole_cards(seat.hole_cards.cards, next, 0);
        vector_to_json_array(seat.hole_cards.cards, packet, "holes");
        packet.end();
        unicast(player, packet.tostring());
        replay.append_record(packet.tojson());
        next = next_betting_seat(next);
    }

    start_seat = cur_seat = dealer;

    //ev_timer_again(zjh.loop, &start_timer);
    start_next_bet(0);
    return 0;
}

int Table::count_next_bet()
{
    struct timeval btime, etime;
    gettimeofday(&btime, NULL);
    int _seat = next_player_seat();
    if (_seat == -1)
    {
        mjlog.error("count next bet no active player.\n");
        handler_preready();
        return -1;
    }

    cur_seat = _seat;
    start_next_bet(0);
    gettimeofday(&etime, NULL);
    mjlog.debug("count next bet diff time %d usec\n", (etime.tv_sec - btime.tv_sec) * 1000000 + etime.tv_usec - btime.tv_usec);
    return 0;
}

int Table::start_next_bet(int flag)
{
    Seat &seat = seats[cur_seat];
    Player *player = seat.player;
    int handler = 0;
    int fetch_flag = 0;

    Jpacket packet;
    packet.val["cmd"] = SERVER_NEXT_BET_BC;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = seat.seatid;
    packet.val["money"] = player->money;
    packet.val["card"] = 0;
    packet.val["remain_cards"] = deck.size();
    packet.val["remain_time"] = BET_TIME_OUT;
    packet.val["robot_flag"] = seat.robot_flag;
    packet.val["fetch_flag"] = 0;

    reset_actions();

    // 处理托管
    if (seat.robot_flag == 1)
    {
        if (deck.permit_get() == 0)
        {
            // 结束游戏
            is_huang_zhuang = 1;
            mjlog.debug("deck no card, liuju\n");
            game_end();
            return 0;
        }

        deck.get_next_card(seat.hole_cards);
        seat.get_next_card_cnt++;
        get_card_seat = cur_seat;
        packet.val["card"] = seat.hole_cards.last_card.value;
        packet.val["remain_cards"] = deck.size();
        packet.val["fetch_flag"] = 1;
        fetch_flag = 1;
        handler = 1;
    }

    if (gang_hu_count > 1 || pao_hu_count > 1)
    {
        mjlog.debug("gang_hu_count %d, pao_hu_count %d\n", gang_hu_count, pao_hu_count);
        //hu_card = last_card.value;

        for (int i = 0; i < seat_max; i++)
        {
            Seat &tseat = seats[i];
            if (tseat.pao_hu_flag == 1)
            {
                // handler_recored_hu(hu_card, i, pao_hu_seat);
            }

            if (tseat.gang_hu_flag == 1)
            {
                // handler_recored_hu(hu_card, i, gang_hu_seat);
            }
        }
        ji_game_end();
        return 0;
    }

    // 处理抢杠胡
    if (last_action == PLAYER_GANG && gang_hu_seat >= 0 && handler == 0 && gang_hu_seat != cur_seat)
    {
        if (seat.hole_cards.permit_hu(last_gang_card.value))
        {
            actions[NOTICE_HU] = 1;
            actions[NOTICE_GUO] = 1;

            //hu_card = last_gang_card.value;
            packet.val["hu_card"] = last_gang_card.value;
            handler = 1;
        }
    }

    if ((last_action == PLAYER_CHU || last_action == PLAYER_GUO) && handler == 0 && seat.handler_flag == 0)
    {
        mjlog.debug("start_next_bet last_action is player_chu\n");
        if (seat.hole_cards.permit_hu(last_card.value))
        {
            if (find(seat.guo_hu_cards.begin(), seat.guo_hu_cards.end(), last_card.value) == seat.guo_hu_cards.end())
            {
                seat.hole_cards.analysis_card_type(last_card.value);
                if (seat.is_bao_ting == 0 && seats[chu_seat].is_bao_ting == 0 &&
                    seat.hole_cards.card_type == CARD_TYPE_PING_HU &&
                    seat.gang_count[1] == 0 &&                        //没有转弯豆（碰杠）是不能胡牌
                    seat.gang_count[2] == 0 &&                        //没有转弯豆（碰杠）是不能胡牌
                    seat.gang_count[3] == 0 &&                        //没有转弯豆（碰杠）是不能胡牌
                    seats[chu_seat].last_actions[1] != PLAYER_GANG && //如果被胡玩家之前的操作是杠,则杠之后出的牌是可以不用碰杠也可以胡
                    seats[chu_seat].last_actions[0] != PLAYER_GANG    //如果被胡玩家的当前操作是碰杠，则这个杠可以被抢胡，不需要碰杠也可以胡
                    && (seat.get_next_card_cnt != 0 ) 
                    )
                {
                    mjlog.debug("mei you zhuang wan dou\n");
                }
                else
                {
                    actions[NOTICE_HU] = 1;
                    actions[NOTICE_GUO] = 1;
                    packet.val["hu_card"] = last_card.value;
                    handler = 1;
                }
            }
        }

        if (seat.ting != 1)
        {
            mjlog.debug("start_next_bet last_action is player_chu, not hu card\n");
            // if (seat.hole_cards.permit_chi(last_card.value))
            // {
            //     mjlog.debug("start_next_bet last_action permit chi\n");
            //     actions[NOTICE_CHI] = 1;
            //     actions[NOTICE_GUO] = 1;
            //     packet.val["chi_card"] = last_card.value;
            //     vector_to_json_array(seat.hole_cards.eat_cards, packet, "chi_cards");
            //     handler = 1;
            // }
            // 没牌摸了，不允许杠
            if (seat.is_bao_ting != 1)
            {
                if (seat.hole_cards.permit_gang(last_card.value) && deck.permit_get() == 1)
                {
                    mjlog.debug("start_next_bet last_action permit gang\n");
                    actions[NOTICE_GANG] = 1;
                    actions[NOTICE_GUO] = 1;
                    packet.val["gang_card"] = last_card.value;
                    handler = 1;
                }

                if (seat.hole_cards.permit_peng(last_card.value) && find(seat.guo_peng_cards.begin(), seat.guo_peng_cards.end(), last_card.value) == seat.guo_peng_cards.end())
                {
                    actions[NOTICE_PENG] = 1;
                    packet.val["peng_card"] = last_card.value;
                    actions[NOTICE_GUO] = 1;
                    mjlog.debug("start_next_bet last_action permit peng\n");
                    handler = 1;
                }
            }
        }
    }

    if (handler == 0)
    {
        // 吃和碰的时候不需要抓牌
        if (seat.hole_cards.size() % 3 == 1)
        {
            if (deck.permit_get() == 0)
            {
                is_huang_zhuang = 1;
                // 结束游戏
                mjlog.debug("deck no card, liuju\n");
                game_end();
                return 0;
            }
            if (set_card_flag == 1 && seat.need_card > 0)
            {
                deck.get_next_card(seat.hole_cards, seat.need_card);
                seat.need_card = 0;
            }
            else
            {
                deck.get_next_card(seat.hole_cards);
            }
            seat.get_next_card_cnt++;

            // handler_recored_mo(seat.hole_cards.last_card.value, seat.seatid, -1);
            seat.guo_hu_cards.clear();
            seat.guo_peng_cards.clear();
            dump_hole_cards(seat.hole_cards.cards, cur_seat, 4);

            get_card_seat = cur_seat;
            packet.val["card"] = seat.hole_cards.last_card.value;
            packet.val["remain_cards"] = deck.size();
            packet.val["fetch_flag"] = 1;
            fetch_flag = 1;
        }
        else
        {
            seat.hole_cards.last_card.set_value(0);
        }

        seat.hole_cards.analysis();
        mjlog.debug("start_next_bet last_action get next card[%d]\n", seat.hole_cards.last_card.value);

        if (seat.hole_cards.permit_hu() && last_action != PLAYER_PENG && seat.forbid_hu == 0)
        {
            actions[NOTICE_HU] = 1;
            actions[NOTICE_GUO] = 1;
            hu_card = seat.hole_cards.last_card.value;
            packet.val["hu_card"] = seat.hole_cards.last_card.value;
            seat.hu = 1;
        }

        if (seat.ting != 1 && actions[NOTICE_HU] == 0)
        {
            if (seat.get_next_card_cnt == 1)
            { //判断是否有天听
                if (seat.hole_cards.ting_cards.size() > 0 && seat.hole_cards.size() >= 13)
                {
                    actions[NOTICE_TING] = 1;
                    actions[NOTICE_GUO] = 1;
                    // handler = 1;
                }
            }
            // 最后一张不允许杠
            if (seat.hole_cards.permit_gang() && handler == 0 && deck.permit_get() == 1 && last_action != PLAYER_PENG)
            {
                actions[NOTICE_GANG] = 1;
                actions[NOTICE_GUO] = 1;
            }
        }
    }

    for (int i = 0; i < NOTICE_SIZE; i++)
    {
        packet.val["action"].append(actions[i]);
    }

    if (actions[NOTICE_GANG] == 1)
    {
        packet.val["gang_flag"] = seat.hole_cards.gang_flags.back();
        vector_to_json_array(seat.hole_cards.gang_cards, packet, "gang_cards");
        for (unsigned int i = 0; i < seat.hole_cards.gang_flags.size(); i++)
        {
            packet.val["gang_flags"].append(seat.hole_cards.gang_flags[i]);
        }
    }

    // if (actions[NOTICE_TING] == 1)
    // {
        int index = 0;
        std::map<int, vector<Card> >::iterator it = seat.hole_cards.ting_cards.begin();
        for (; it != seat.hole_cards.ting_cards.end(); it++)
        {
            packet.val["ting_cards"].append(it->first);
            int size = it->second.size();
            for (int i = 0; i < size; i++)
            {
                packet.val["ting_pattern"][index].append(it->second[i].value);
            }
            index++;
        }
    // }

    packet.end();
    unicast(seat.player, packet.tostring());
    replay.append_record(packet.tojson());

    Jpacket packet1;
    packet1.val["cmd"] = SERVER_NEXT_BET_BC;
    packet1.val["uid"] = player->uid;
    packet1.val["seatid"] = seat.seatid;
    packet1.val["remain_time"] = BET_TIME_OUT;
    packet1.val["remain_cards"] = deck.size();
    packet1.val["fetch_flag"] = fetch_flag;
    packet1.val["robot_flag"] = seat.robot_flag;
    if (actions[NOTICE_GANG] == 1)
    {
        packet1.val["gang_flag"] = seat.hole_cards.gang_flag;
    }
    packet1.end();
    broadcast(seat.player, packet1.tostring());

    if (actions[NOTICE_CHI] == 1 || actions[NOTICE_PENG] == 1 || actions[NOTICE_GANG] == 1 ||
        actions[NOTICE_HU] == 1 || actions[NOTICE_GUO] == 1)
    {
        // handler_record_notice(cur_seat);
    }

    if (seat.robot_flag == 1 || (seat.ting == 1 && actions[NOTICE_HU] != 1))
    {
        ev_timer_set(&bet_timer, 2, 2);
        ev_timer_again(zjh.loop, &bet_timer);
    }
    else
    {
        ev_timer_set(&bet_timer, BET_TIME_OUT, BET_TIME_OUT);
        ev_timer_again(zjh.loop, &bet_timer);
    }

    return 0;
}

void Table::bet_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->bet_timer);
    mjlog.debug("bet_timer_cb\n");
    table->bet_timeout();
}

void Table::compare_timer_cb(struct ev_loop *loop, struct ev_timer *w,
                             int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->compare_timer);
    //mjlog.debug("compare_timer_cb\n");
    //table->compare_timeout();
}

int Table::handler_bet(Player *player)
{
    int ret;
    Json::Value &val = player->client->packet.tojson();
    int action = val["action"].asInt();

    if (state != BETTING)
    {
        mjlog.error("handler_bet state is not betting[%d]\n", state);
        handler_bet_error(player, action);
        return -1;
    }

    Seat &seat = seats[player->seatid];
    if (seat.player != player)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_bet player is no match with seat.player\n");
        return -1;
    }

    //player->noplay_count = 0;

    if (cur_seat != player->seatid && (action != PLAYER_TUOGUAN) && (action != PLAYER_CANCEL_TUOGUAN))
    {
        mjlog.error("handler_bet player->seatid[%d] cur_seat[%d]\n",
                    player->seatid, cur_seat);
        return -1;
    }

    if (action == PLAYER_GUO)
    {
        ret = handler_guo(player);
        if (ret < 0)
        {
            return -1;
        }
        //ev_timer_stop(zjh.loop, &bet_timer);
        // count_next_bet();
        return 0;
    }

    // if (action == PLAYER_CHI)
    // {
    // 	ret = handler_chi(player);
    // 	if (ret < 0)
    // 	{
    // 		return -1;
    // 	}

    //     start_next_bet(0);

    // 	return 0;
    // }
    if (action == PLAYER_CHU)
    {
        ret = handler_chu(player);
        if (ret < 0)
        {
            return -1;
        }
        ev_timer_stop(zjh.loop, &bet_timer);
        count_next_bet();
        return 0;
    }
    else if (action == PLAYER_PENG)
    {
        ret = handler_peng(player);
        if (ret < 0)
        {
            return -1;
        }

        start_next_bet(0);
        return 0;
    }
    else if (action == PLAYER_GANG)
    {
        ret = handler_gang(player);
        if (ret < 0)
        {
            return -1;
        }

        ev_timer_stop(zjh.loop, &bet_timer);
        ev_timer_again(zjh.loop, &bet_timer);

        count_next_bet();
        // ev_timer_stop(zjh.loop, &bet_timer);
        return 0;
    }
    else if (action == PLAYER_HU)
    {
        ret = handler_hu(player);
        if (ret < 0)
        {
            return -1;
        }
        ev_timer_stop(zjh.loop, &bet_timer);
        mjlog.debug("player[%d] req hu card\n", player->uid);
        ji_game_end();

        return 0;
    }
    else if (action == PLAYER_TING)
    {
        ret = handler_ting(player);
        if (ret < 0)
        {
            return -1;
        }
        ev_timer_stop(zjh.loop, &bet_timer);
        count_next_bet();
        return 0;
    }
    else if (action == PLAYER_CANCEL)
    {
        ret = handler_cancel(player);
        if (ret < 0)
        {
            return -1;
        }
        ev_timer_stop(zjh.loop, &bet_timer);
        return 0;
    }
    else if (action == PLAYER_JIABEI)
    {
        ret = handler_jiabei(player);
        if (ret < 0)
        {
            return -1;
        }

        if (seat.hole_cards.size() % 3 != 2)
        {
            ev_timer_stop(zjh.loop, &bet_timer);
            start_next_bet(0);
        }
        else
        {
            //start_next_bet(0);
        }
        return 0;
    }

    return 0;
}

void Table::bet_timeout()
{
    // Seat &seat = seats[cur_seat];
    // Player *player = seat.player;

    // if (player == NULL)
    // {
    // 	mjlog.error(
    // 			"bet timeout null player ready_players[%d] cur_players[%d] tid[%d].\n",
    // 			ready_players, cur_players, tid);
    // 	return;
    // }

    // if (seat.hu == 1)
    // {
    //     win_seatid = cur_seat;

    //     vector<Card> cards = seat.hole_cards.cards;
    //     if (cards.size() % 3 == 2)
    //     {
    //         vector<Card>::iterator it = find(cards.begin(), cards.end(), seat.hole_cards.last_card);
    //         if (it != cards.end())
    //         {
    //             cards.erase(it);
    //         }
    //     }

    //     Jpacket packet;
    //     packet.val["cmd"] = SERVER_BET_SUCC_BC;
    //     packet.val["seatid"] = player->seatid;
    //     packet.val["uid"] = player->uid;
    //     packet.val["action"] = PLAYER_HU;
    //     packet.val["card"] = seat.hole_cards.last_card.value;
    //     packet.val["hu_card"] = seat.hole_cards.last_card.value;
    //     packet.val["time_out"] = 0;
    //     vector_to_json_array(cards, packet, "holes");
    //     packet.end();
    //     unicast(player, packet.tostring());

    //     Jpacket packet1;
    //     packet1.val["cmd"] = SERVER_BET_SUCC_BC;
    //     packet1.val["seatid"] = player->seatid;
    //     packet1.val["uid"] = player->uid;
    //     packet1.val["action"] = PLAYER_HU;
    //     packet1.val["card"] = seat.hole_cards.last_card.value;
    //     packet1.val["hu_card"] = seat.hole_cards.last_card.value;
    //     packet1.val["time_out"] = 0;
    //     vector_to_json_array(cards, packet1, "holes");
    //     packet1.end();
    //     broadcast(player, packet1.tostring());

    //     mjlog.debug("player[%d] get a card, hu card on timeout\n", player->uid);
    //     game_end();
    //     return;
    // }
    // else
    // {
    //     return;
    // }
}

void Table::compare_timeout()
{
    if (state != BETTING)
    {
        mjlog.error("compare timeout state[%d] is not BETTING.\n", state);
        return;
    }
    int ret = test_game_end();
    if (ret < 0)
    {
        count_next_bet();
    }
}

void Table::lose_update(Player *player)
{
    Seat &seat = seats[player->seatid];

    player->incr_money(1, seat.bet);
    player->incr_total_board(vid, 1);
    mjlog.debug("lose_update total_board [%d] pre_uid[%d] \n", player->total_board, player->pre_uid);
    if (player->pre_uid > 1000)
    {
        handler_invite_advantage(player);
    }
}

void Table::win_update(Player *player)
{
    Seat &seat = seats[player->seatid];

    player->incr_money(0, seat.bet);

    player->incr_total_board(vid, 1);
    mjlog.debug("win_update total_board [%d] pre_uid[%d] \n", player->total_board, player->pre_uid);
    if (player->pre_uid > 1000)
    {
        handler_invite_advantage(player);
    }
}

void Table::update_money(Player *player, int value)
{
    if (cur_flow_mode != FLOW_IMMEDIATE)
    {
        return;
    }
    player->incr_money(1, value);
}

int Table::test_game_end()
{
    int betting_players = count_betting_seats();
    if (betting_players == 1)
    {
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].betting == 1)
            {
                win_seatid = i;
                break;
            }
        }

        mjlog.debug("bet player only one, game end\n");
        game_end();
        return 0;
    }

    return -1;
}

int Table::game_end(int flag)
{
    mjlog.debug("game_end[%d]\n", start_seat);
    ev_timer_stop(zjh.loop, &bet_timer);
    ev_timer_stop(zjh.loop, &start_timer);
    ev_timer_stop(zjh.loop, &compare_timer);

    int save_flag = 0;
	if (state == BETTING)
	{
		save_flag = 1;
	}
    state = END_GAME;

    if (players.size() == 0)
    {
        mjlog.error("game end state[%d], player size is null ttid[%d]\n", state, ttid);
        return -1;
    }

    accumulate_hu();

    Jpacket packet;
    packet.val["cmd"] = SERVER_GAME_END_BC;
    packet.val["win_seatid"] = win_seatid;
    packet.val["total_bet"] = total_bet;
    packet.val["cur_round"] = round_count;

    for (unsigned int i = 0; i < deck.cards.size(); i++)
	{
		packet.val["lave_cards"].append(deck.cards[i].value);
    }

    if (pao_hu_seat >= 0)
    {
        packet.val["fang_pao_seat"] = pao_hu_seat;
    }
    else if (gang_hu_seat >= 0)
    {
        packet.val["fang_pao_seat"] = gang_hu_seat;
    }
    else
    {
        packet.val["fang_pao_seat"] = -1;
    }

    vector_to_json_array(deck.horse_cards, packet, "horse_cards"); //翻到的那张鸡牌

    if (win_seatid < 0 && gang_hu_count < 2 && pao_hu_count < 2)
    {
        win_bet = 0;
        packet.val["is_liuju"] = 1;

        int size = deck.horse_cards.size();
        for (int i = 0; i < size; i++)
        {
            packet.val["zhong_horse"].append(0); //这个字段在鸡中不用
        }
        if (save_flag == 1)
        {
            update_account_bet();

            for (int i = 0; i < seat_max; i++)
            {
                if (seats[i].ready != 1)
                {
                    continue;
                }

                if (seats[i].bet > 0)
                {
                    win_update(seats[i].player);
                }
                else
                {
                    lose_update(seats[i].player);
                }
            }
            // 记录结果
            insert_flow_record();
        }
    }
    else
    {
        packet.val["is_liuju"] = 0;
        if (save_flag == 1)
        {
            update_account_bet();

            // 记录结果
            insert_flow_record();

            //int size = zhong_horse.size();
            int size = ji_pai.size();
            for (int i = 0; i < size; i++)
            {
                //packet.val["zhong_horse"].append(zhong_horse[i]);
                packet.val["zhong_horse"].append(1); //这个字段在鸡中不用
            }

            for (int i = 0; i < seat_max; i++)
            {
                if (seats[i].ready != 1)
                {
                    continue;
                }

                if (seats[i].bet > 0)
                {
                    win_update(seats[i].player);
                }
                else
                {
                    lose_update(seats[i].player);
                }
            }
        }
    }

    packet.val["tian_hu_flag"] = tian_hu_flag;               //天胡
    packet.val["di_hu_flag"] = di_hu_flag;                   //地胡
    packet.val["hai_di_hu_flag"] = hai_di_hu_flag;           //海底胡
    packet.val["gang_shang_hua_flag"] = gang_shang_hua_flag; //杠上花
    packet.val["gang_shang_pao"] = gang_shang_pao;           //杠上炮
    packet.val["dead_double"] = dead_double;

    int j = 0;
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].occupied == 1)
        {
            Player *player = seats[i].player;
            if (player)
            {
                packet.val["players"][j]["seatid"] = player->seatid;
                packet.val["players"][j]["uid"] = player->uid;
                packet.val["players"][j]["name"] = player->name;
                packet.val["players"][j]["rmb"] = player->rmb;
                packet.val["players"][j]["money"] = player->money;
                packet.val["players"][j]["total_board"] = player->total_board;
                packet.val["players"][j]["pay_total"] = player->pay_total;
                packet.val["players"][j]["gang_hu_flag"] = seats[i].gang_hu_flag;
                packet.val["players"][j]["horse_count"] = seats[i].horse_count; //算出来的鸡的个数
                seats[i].total_zhong_horse += seats[i].horse_count;
                packet.val["players"][j]["pao_hu_flag"] = seats[i].pao_hu_flag;
                packet.val["players"][j]["forbid_hu"] = seats[i].forbid_hu;
                packet.val["players"][j]["forbided_seatid"] = seats[i].forbided_seatid;
                packet.val["players"][j]["chi_san_bi_seatid"] = seats[i].forbided_seatid;
                std::list<Card>::iterator lit = seats[i].hole_cards.discard_cards.begin();
                for (; lit != seats[i].hole_cards.discard_cards.end(); lit++)
                {
                    packet.val["players"][j]["discard_holes"].append(lit->value);
                }
                //vector_to_json_array(seats[i].ji_pai, packet.val["players"][j], "settlement_type", "settlement_score");  //结算的类型和分值
                if (is_huang_zhuang)
                {
                    packet.val["players"][j]["jiao_pai_score"] = seats[i].jiao_pai_max_score;
                    packet.val["players"][j]["jiao_pai_type"] = seats[i].jiao_pai_max_type;

                    std::map<int, int>::iterator it;
                    for (it = score_from_players_item_count[i].begin(); it != score_from_players_item_count[i].end(); it++)
                    {
                        if (it->second == 0)
                            continue;
                        if (it->first == JIAO_PAI_TYPE)
                            packet.val["players"][j]["jin_fen_ge_shu"].append(it->second);
                    }

                    for (it = score_to_players_item_count[i].begin(); it != score_to_players_item_count[i].end(); it++)
                    {
                        if (it->second == 0)
                            continue;
                        if (it->first == JIAO_PAI_TYPE)
                            packet.val["players"][j]["chu_fen_ge_shu"].append(it->second);
                    }
                }
                else
                {
                    if (score_from_players_item_total.size() > 0 )
                    {
                        map_to_json_array(score_from_players_item_count[i], packet.val["players"][j], "jin_fen_ge_shu");
                        map_to_json_array(score_to_players_item_count[i], packet.val["players"][j], "chu_fen_ge_shu");
                    }
                }
                map_to_json_array(score_from_players_item_total[i], packet.val["players"][j], "jin_fen_lei_xing", "jin_fen");

                //出分
                map_to_json_array(score_to_players_item_total[i], packet.val["players"][j], "chu_fen_lei_xing", "chu_fen");

                packet.val["players"][j]["hu_pai_lei_xing"] = seats[i].hu_pai_lei_xing.c_str();

                if (seats[i].ji_pai.size() > 0)
                {
                    // handler_record_horse(i);
                    vector_to_json_array(seats[i].ji_pai, packet.val["players"][j], "horse_cards", "horse_type"); //算出来的鸡牌值,和类型
                    //vector_to_json_array(seats[i].ji_pai.type, packet.val["players"][j], "horse_cards"); //算出来的鸡牌类型
                    vector_to_json_array(seats[i].zhong_horse, packet.val["players"][j], "zhong_horse");
                    packet.val["players"][j]["display_horse"] = 1;
                }
                else
                {
                    // vector_to_json_array(deck.horse_cards, packet.val["players"][j], "horse_cards");
                    // for (int z = 0; z < deck.horse_cards.size(); z++)
                    // {
                    //     packet.val["players"][j]["zhong_horse"].append(0);
                    // }
                    packet.val["players"][j]["display_horse"] = 0;
                }

                for (unsigned int z = 0; z < seats[i].obsorb_seats.size(); z++)
                {
                    packet.val["players"][j]["obsorb_seats"].append(seats[i].obsorb_seats[z]);
                }

                if (seats[i].seatid == win_seatid)
                {
                    packet.val["players"][j]["card_type"] = seats[i].hole_cards.card_type;
                    string desc = format_card_desc(seats[i].hole_cards.card_type, i);
                    packet.val["players"][j]["card_desc"] = desc.c_str();
                    packet.val["players"][j]["win"] = 1;

                    for (unsigned int k = 0; k < seats[i].hole_cards.oldcards.size(); k++)
                    {
                        packet.val["players"][j]["holes"].append(seats[i].hole_cards.oldcards[k].value);
                    }

                    if (seats[i].hole_cards.cards.size() % 3 != 2)
                    {
                        packet.val["players"][j]["holes"].append(hu_card);
                    }

                    std::vector<Card>::iterator vit;
                    int osize = seats[i].hole_cards.obsorb_cards.size();
                    for (int z = 0; z < osize; z++)
                    {
                        vit = seats[i].hole_cards.obsorb_cards[z].begin();
                        for (; vit != seats[i].hole_cards.obsorb_cards[z].end(); vit++)
                        {
                            packet.val["players"][j]["obsorb_holes"][z].append(vit->value);
                        }
                    }

                    packet.val["players"][j]["fang_gang_count"] = seats[i].fang_gang_count;

                    for (int k = 0; k < 4; k++)
                    {
                        packet.val["players"][j]["gang_count"].append(seats[i].gang_count[k]);
                    }
                    packet.val["players"][j]["bet"] = seats[i].bet;
                }
                else
                {

                    if ((seats[i].gang_hu_flag == 1 && gang_hu_count > 1) || (seats[i].pao_hu_flag == 1 && pao_hu_count > 1))
                    {
                        packet.val["players"][j]["card_type"] = seats[i].hole_cards.card_type;
                        string desc = format_card_desc(seats[i].hole_cards.card_type, i);
                        packet.val["players"][j]["card_desc"] = desc.c_str();
                        packet.val["players"][j]["win"] = 1;
                    }
                    else
                    {
                        packet.val["players"][j]["card_type"] = 0;
                        packet.val["players"][j]["card_desc"] = "";
                        packet.val["players"][j]["win"] = 0;
                    }

                    for (unsigned int k = 0; k < seats[i].hole_cards.oldcards.size(); k++)
                    {
                        packet.val["players"][j]["holes"].append(seats[i].hole_cards.oldcards[k].value);
                    }

                    if ((seats[i].gang_hu_flag == 1 && gang_hu_count > 1) || (seats[i].pao_hu_flag == 1 && pao_hu_count > 1))
                    {
                        packet.val["players"][j]["holes"].append(hu_card);
                    }

                    std::vector<Card>::iterator vit;
                    int osize = seats[i].hole_cards.obsorb_cards.size();
                    for (int z = 0; z < osize; z++)
                    {
                        vit = seats[i].hole_cards.obsorb_cards[z].begin();
                        for (; vit != seats[i].hole_cards.obsorb_cards[z].end(); vit++)
                        {
                            packet.val["players"][j]["obsorb_holes"][z].append(vit->value);
                        }
                    }

                    packet.val["players"][j]["fang_gang_count"] = seats[i].fang_gang_count;

                    for (int k = 0; k < 4; k++)
                    {
                        packet.val["players"][j]["gang_count"].append(seats[i].gang_count[k]);
                    }

                    packet.val["players"][j]["bet"] = seats[i].bet;
                }

                j++;
            }
        }
    }

    int init_money = zjh.conf["tables"]["init_money"].asInt();
    packet.val["flag"] = 0;
    packet.val["owner_uid"] = owner_uid;
    packet.val["owner_name"] = owner_name;
    packet.val["owner_sex"] = owner_sex;

    if (flag == 1 || round_count >= max_play_board)
    {
        packet.val["flag"] = 1;
        j = 0;
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].occupied == 1)
            {
                Player *player = seats[i].player;
                Seat &seat = seats[i];
                if (player)
                {
                    packet.val["players1"][j]["seatid"] = player->seatid;
                    packet.val["players1"][j]["uid"] = player->uid;
                    packet.val["players1"][j]["name"] = player->name;
                    packet.val["players1"][j]["rmb"] = player->rmb;
                    packet.val["players1"][j]["money"] = player->money;
                    packet.val["players1"][j]["bet"] = player->money - init_money;
                    packet.val["players1"][j]["avatar"] = player->avatar;
                    packet.val["players1"][j]["ip"] = player->remote_ip;
                    packet.val["players1"][j]["total_record_desc"].append("自摸次数");
                    packet.val["players1"][j]["total_record"].append(seat.total_zimo);
                    packet.val["players1"][j]["total_record_desc"].append("接炮次数");
                    packet.val["players1"][j]["total_record"].append(seat.total_jie_pao);
                    packet.val["players1"][j]["total_record_desc"].append("点炮次数");
                    packet.val["players1"][j]["total_record"].append(seat.total_fang_pao);
                    packet.val["players1"][j]["total_record_desc"].append("中鸡数");
                    packet.val["players1"][j]["total_record"].append(seat.total_zhong_ji);
                    packet.val["players1"][j]["total_record_desc"].append("冲锋鸡数");
                    packet.val["players1"][j]["total_record"].append(seat.total_chong_feng_ji);
                    packet.val["players1"][j]["total_record_desc"].append("责任鸡数");
                    packet.val["players1"][j]["total_record"].append(seat.total_ze_ren_ji);
                    j++;
                }
            }
        }

        insert_flow_round_record();
        game_end_flag = 1;
    }
    packet.end();
    broadcast(NULL, packet.tostring());
    replay.append_record(packet.tojson());

    if (save_flag == 1)
	{
        // replay.save();
		// replay.save(ts, ttid);
		replay.async_save(ts, ttid);
	}
    struct timeval btime, etime;
    gettimeofday(&btime, NULL);
    gettimeofday(&etime, NULL);
    mjlog.debug("game end diff time %d usec\n", (etime.tv_sec - btime.tv_sec) * 1000000 + etime.tv_usec - btime.tv_usec);

    //for (int i = 0; i < seat_max; i++)
    //{
    //	seats[i].reset();
    //}

    //reset();

    ev_timer_again(zjh.loop, &preready_timer);

    return 0;
}

int Table::handler_fold(Player *player)
{
    Seat &seat = seats[player->seatid];

    if (seat.betting == 0)
    {
        mjlog.error("handler_fold seat betting[0] uid[%d] tid[%d].\n",
                    player->uid, tid);
        return -1;
    }

    //lose_update(seat.player);
    seat.betting = 0;

    return 0;
}

int Table::handler_logout(Player *player)
{
    // if (state == BETTING)
    // {
    // 	Seat &seat = seats[player->seatid];
    // 	if (seat.betting == 1)
    // 	{
    // 		handler_fold(player);
    // 		int ret = test_game_end();
    // 		if (ret < 0)
    // 		{
    // 			if (player->seatid == cur_seat)
    // 			{
    // 				count_next_bet();
    // 			}
    // 		}
    // 	}
    // }

    Jpacket packet;
    packet.val["cmd"] = SERVER_LOGOUT_SUCC_BC;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.val["type"] = player->logout_type;
    packet.end();
    broadcast(NULL, packet.tostring());

    return 0;
}

int Table::handler_chat(Player *player)
{
    Json::Value &val = player->client->packet.tojson();

    Jpacket packet;
    packet.val["cmd"] = SERVER_CHAT_BC;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.val["text"] = val["text"];
    packet.val["chatid"] = val["chatid"];
    packet.val["sex"] = val["sex"];
    packet.end();
    broadcast(NULL, packet.tostring());

    //player->noplay_count = 0;
    return 0;
}

int Table::handler_face(Player *player)
{
    Json::Value &val = player->client->packet.tojson();

    Jpacket packet;
    packet.val["cmd"] = SERVER_FACE_BC;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.val["faceid"] = val["faceid"];
    packet.end();
    broadcast(NULL, packet.tostring());

    //player->noplay_count = 0;

    return 0;
}

int Table::handler_interFace(Player *player)
{
    Json::Value &val = player->client->packet.tojson();

    Jpacket packet;
    packet.val["cmd"] = SERVER_INTERFACE_BC;
    packet.val["uid"] = player->uid;
    packet.val["fromid"] = player->seatid;
    packet.val["toid"] = val["toid"];
    packet.val["faceid"] = val["faceid"];
    packet.end();
    broadcast(NULL, packet.tostring());

    //player->noplay_count = 0;
    return 0;
}

int Table::handler_player_info(Player *player)
{
    int ret = 0;
    //player->noplay_count = 0;
    ret = player->update_info();
    if (ret < 0)
    {
        mjlog.error("handler_player_info update_info error.\n");
        return -1;
    }

    if (state == BETTING)
    {
        Seat &seat = seats[player->seatid];
        if (seat.betting == 1)
        {
            player->money -= seat.bet;
            mjlog.info("handler_player_info o uid[%d] money[%d] bet[%d].\n",
                       player->uid, player->money, seat.bet);
        }
    }

    Jpacket packet;
    packet.val["cmd"] = SERVER_PLAYER_INFO_BC;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.val["money"] = player->money;
    packet.val["rmb"] = player->rmb;
    packet.val["total_board"] = player->total_board;

    packet.end();
    broadcast(NULL, packet.tostring());

    mjlog.info("handler_player_info uid[%d] seatid[%d] money[%d] tid[%d].\n",
               player->uid, player->seatid, player->money, tid);

    return 0;
}

int Table::handler_prop(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    int action = val["action"].asInt();

    if (type != 0)
    {
        mjlog.error("handler_prop error uid[%d] money[%d] is not in prop\n",
                    player->uid, player->money);
        return 0;
    }

    if (state != BETTING)
    {
        mjlog.error("handler_prop state[%d] is not betting tid[%d]\n", state,
                    tid);
        return -1;
    }

    Seat &seat = seats[player->seatid];
    if (seat.player != player)
    {
        mjlog.error("handler_prop player is no match with seat.player\n");
        return -1;
    }

    mjlog.info("handler_prop succ. uid[%d] seatid[%d] action[%d] tid[%d].\n",
               player->uid, player->seatid, action, tid);
    return 0;
}

void Table::handler_prop_error(Player *player, int action)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_PROP_ERR_UC;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.end();
    unicast(player, packet.tostring());
}

void Table::handler_bet_error(Player *player, int action)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_BET_ERR_UC;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.val["chu_seat"] = chu_seat;

    Seat &seat = seats[player->seatid];

    vector_to_json_array(seat.hole_cards.cards, packet, "holes");

    std::list<Card>::iterator lit = seat.hole_cards.discard_cards.begin();
    for (; lit != seat.hole_cards.discard_cards.end(); lit++)
    {
        packet.val["discard_holes"].append(lit->value);
    }

    packet.end();
    unicast(player, packet.tostring());
}

int Table::next_betting_seat(int pos)
{
    int cur = pos;

    for (int i = 0; i < seat_max; i++)
    {
        cur++;
        if (cur >= seat_max)
            cur = 0;

        if (seats[cur].betting == 1)
        {
            mjlog.debug("cur is betting seat[%d]\n", cur);
            return cur;
        }
    }

    mjlog.error("error active player\n");

    return -1;
}

int Table::next_player_seat()
{
    int first_hu_seat = -1;
    pao_hu_count = 0;
    gang_hu_count = 0;

    if (last_action == PLAYER_CHU)
    {
        for (int i = 1; i < seat_max; i++)
        {
            int next = (cur_seat + i) % seat_max;
            Seat &seat = seats[next];
            if (seats[next].ready != 1)
            {
                continue;
            }

            if (seat.hole_cards.permit_hu(last_card.value) && seat.forbid_hu != 1)
            {
                if (find(seat.guo_hu_cards.begin(), seat.guo_hu_cards.end(), last_card.value) != seat.guo_hu_cards.end())
                {
                    mjlog.debug("cur_seat[%d], next hu seat[%d], guo hu\n", cur_seat, next);
                    continue;
                }

                seat.hole_cards.analysis_card_type(last_card.value);
                if (seat.is_bao_ting == 0 && seats[chu_seat].is_bao_ting == 0 &&
                    seat.hole_cards.card_type == CARD_TYPE_PING_HU &&
                    seat.gang_count[1] == 0 &&                        //没有转弯豆（碰杠）是不能胡牌
                    seat.gang_count[2] == 0 &&                        //没有转弯豆（碰杠）是不能胡牌
                    seat.gang_count[3] == 0 &&                        //没有转弯豆（碰杠）是不能胡牌
                    seats[chu_seat].last_actions[1] != PLAYER_GANG && //如果被胡玩家之前的操作是杠,则杠之后出的牌是可以不用碰杠也可以胡
                    seats[chu_seat].last_actions[0] != PLAYER_GANG    //如果被胡玩家的当前操作是碰杠，则这个杠可以被抢胡，不需要碰杠也可以胡
                    && (seat.get_next_card_cnt != 0 )   //地胡是可以直接胡的，不需要转弯豆
                    )
                {
                    mjlog.debug("mei you zhuang wan dou\n");
                    continue;
                }

                //if (seat.hole_cards.card_type == CARD_TYPE_PING_HU && fang_pao == 0)
                //{
                //if ((!(seats[chu_seat].hole_cards.cards.size() == 1 && seats[chu_seat].hole_cards.an_gang_count == 0))
                //&& (!(seats[chu_seat].last_actions[1] == PLAYER_GANG && seats[chu_seat].last_actions[0] == PLAYER_CHU))
                //&& (!(deck.permit_get() == 0)))
                //{
                //mjlog.debug("cur_seat[%d], next hu seat[%d], card_type %d, last card %d\n",
                //cur_seat, next, seat.hole_cards.card_type, last_card.value);
                //continue;
                //}
                //}
                hu_card = last_card.value;
                mjlog.debug("cur_seat[%d], next hu seat[%d]\n", cur_seat, next);
                if (first_hu_seat == -1)
                {
                    first_hu_seat = next;
                }

                seat.pao_hu_flag = 1;
                pao_hu_count++;
            }
        }

        if (first_hu_seat >= 0)
        {
            pao_hu_seat = cur_seat;
            return first_hu_seat;
        }

        for (int i = 1; i < seat_max; i++)
        {
            int next = (cur_seat + i) % seat_max;
            if (seats[next].ready != 1)
            {
                continue;
            }

            if (seats[next].is_bao_ting == 1)
            {//天听不允许碰和杠
                continue;
            }

            vector<int> &cards = seats[next].guo_peng_cards;
            if (find(cards.begin(), cards.end(), last_card.value) != cards.end())
            {
                mjlog.debug("cur_seat[%d], guo peng seat[%d]\n", cur_seat, next);
                continue;
            }

            if (seats[next].hole_cards.permit_peng(last_card.value) || seats[next].hole_cards.permit_gang(last_card.value))
            {
                mjlog.debug("cur_seat[%d], next peng gang seat[%d]\n", cur_seat, next);
                return next;
            }
        }
    }

    if (last_action == PLAYER_GANG)
    {
        if (seats[cur_seat].hole_cards.gang_flag == 1)
        {
            for (int i = 1; i < seat_max; i++)
            {
                int next = (cur_seat + i) % seat_max;
                if (seats[next].ready != 1)
                {
                    continue;
                }

                if (seats[next].hole_cards.permit_hu(last_gang_card.value) && seats[next].forbid_hu != 1)
                {
                    // 抢杠胡
                    mjlog.debug("cur_seat[%d], next hu seat[%d]\n", cur_seat, next);

                    if (first_hu_seat == -1)
                    {
                        first_hu_seat = next;
                    }

                    hu_card = last_gang_card.value;
                    seats[next].gang_hu_flag = 1;
                    gang_hu_count++;
                }
            }
        }

        if (first_hu_seat >= 0)
        {
            gang_hu_seat = cur_seat;
            return first_hu_seat;
        }
        else
        {
            return cur_seat;
        }
    }

    if (last_action == PLAYER_GUO)
    {
        if (wait_handle_action == PLAYER_CHU)
        {
            for (int i = 1; i < seat_max; i++)
            {
                int next = (cur_seat + i) % seat_max;

                if (seats[next].ready != 1)
                {
                    continue;
                }

                if (seats[next].handler_flag == 1 || next == wait_handler_seat)
                {
                    continue;
                }
                if (seats[next].is_bao_ting == 1)
                { //天听不允许碰和杠
                    continue;
                }

                if (seats[next].hole_cards.permit_peng(last_card.value) || seats[next].hole_cards.permit_gang(last_card.value))
                {
                    mjlog.debug("cur_seat[%d], next peng gang seat[%d]\n", cur_seat, next);
                    return next;
                }
            }

            int seatid = next_betting_seat(wait_handler_seat);
            if (seatid >= 0)
            {
                mjlog.debug("guo pai, cur_seat[%d], next play seat[%d]\n", cur_seat, seatid);
                return seatid;
            }
            else
            {
                mjlog.error("guo pai error next player\n");
                return -1;
            }
        }
        else if (wait_handle_action == PLAYER_GANG)
        {
            return wait_handler_seat;
        }
    }

    int seatid = next_betting_seat(cur_seat);
    if (seatid >= 0)
    {
        mjlog.debug("cur_seat[%d], next play seat[%d]\n", cur_seat, seatid);
        return seatid;
    }
    else
    {
        mjlog.error("error next player\n");
        return -1;
    }
}

int Table::prev_betting_seat(int pos)
{
    int cur = pos;

    for (int i = 0; i < seat_max; i++)
    {
        cur--;
        if (cur < 0)
            cur = seat_max - 1;

        if (seats[cur].betting == 1)
        {
            mjlog.debug("cur is betting seat[%d]\n", cur);
            return cur;
        }
    }

    mjlog.error("error active player\n");

    return -1;
}

int Table::count_betting_seats()
{
    int count = 0;

    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].betting == 1)
        {
            count++;
        }
    }

    return count;
}

int Table::get_last_betting()
{
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].betting == 1)
        {
            return i;
        }
    }

    return -1;
}

/*  type: 1、钻石 2、金币 3、踢人卡 4、换牌 5、禁比卡 6、四倍卡 7、八倍卡 8、座驾 9、兑换券
	flag: 1、加 0、减
 	pos: 51、转转乐 52、摇摇乐
		 60、充值-购买钻石  61、钻石兑换金币 62、钻石兑换道具 63、钻石购买座驾 65、奖券兑换奖品
		 70、注册 71、登录领奖 72、破产领取 73、参拜财神 74、充值抽奖 75、领取邮件礼物 76、发送邮件礼物
		 77、玩N局奖励  78、领取任务奖励
*/
int Table::insert_flow_log(int ts, int uid, string ip, int pos, int vid, int zid, int tid, int type, int flag, int num, int anum)
{
    Jpacket packet;
    packet.val["roomid"] = ttid;
    packet.val["masterid"] = owner_uid;

    int index = 0;
    for (int i = 0; i < seat_max; i++)
    {
        Player *player = seats[i].player;

        int occ = 1;
        if (player == NULL)
        {
            occ = 0;
        }
        else if (player->uid == owner_uid)
        {
            continue;
        }

        switch (index)
        {
        case 0:
            packet.val["player1"] = occ == 1 ? player->uid : 0;
            break;
        case 1:
            packet.val["player2"] = occ == 1 ? player->uid : 0;
            break;
        case 2:
            packet.val["player3"] = occ == 1 ? player->uid : 0;
            break;
        default:
            break;
        }

        index++;
    }

    packet.val["rmb"] = num;
    packet.val["vid"] = type;
    packet.val["ts"] = (int)time(NULL);

    string str = packet.val.toStyledString().c_str();
    int ret = zjh.temp_rc->command("LPUSH rmb_flow_list %s", str.c_str());

    if (ret < 0)
    {
        mjlog.error("insert rmb_flow_record %s", str.c_str());
        return -1;
    }
    return 0;
}

int Table::handler_send_gift_req(Player *player)
{
    return 0;
}

void Table::send_result_to_robot()
{
}

void Table::init_dealer()
{
    // 第一轮庄家做主
    if (round_count == 1)
    {
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].ready == 0)
            {
                continue;
            }
            if (seats[i].uid == owner_uid)
            {
                dealer = i;
                seats[dealer].lian_zhuang_cnt = 1;
                return;
            }
        }
        dealer = random_dealer();
        seats[dealer].lian_zhuang_cnt = 1;
        return;
    }

    //三人胡了，则放炮的玩家作庄
    if (gang_hu_seat > -1 && gang_hu_count > 2)
    {
        if (dealer == gang_hu_seat)
            seats[dealer].lian_zhuang_cnt++;
        else
            seats[gang_hu_seat].lian_zhuang_cnt = 2;
        dealer = gang_hu_seat;
        return;
    }

    if (pao_hu_seat > -1 && pao_hu_count > 2)
    {
        if (dealer == pao_hu_seat)
            seats[dealer].lian_zhuang_cnt++;
        else
            seats[pao_hu_seat].lian_zhuang_cnt = 2;
        dealer = pao_hu_seat;
        return;
    }

    //二人胡了,第一人胡的玩家作庄
    if (gang_hu_seat > -1 && gang_hu_count == 2)
    {
        int first_hu = gang_hu_seat;
        for (int i = 1; i < seat_max; i++)
        {
            first_hu = (gang_hu_seat + i) % seat_max;
            if (seats[first_hu].occupied != 1)
            {
                continue;
            }
            if (seats[first_hu].gang_hu_flag != 1)
            {
                continue;
            }
            if (dealer == first_hu)
                seats[dealer].lian_zhuang_cnt++;
            else
                seats[first_hu].lian_zhuang_cnt = 2;
            dealer = first_hu;
        }
        return;
    }
    if (pao_hu_seat > -1 && pao_hu_count == 2)
    {
        int first_hu = pao_hu_seat;
        for (int i = 1; i < seat_max; i++)
        {
            first_hu = (pao_hu_seat + i) % seat_max;
            if (seats[first_hu].occupied != 1)
            {
                continue;
            }
            if (seats[first_hu].pao_hu_flag != 1)
            {
                continue;
            }
            if (dealer == first_hu)
                seats[dealer].lian_zhuang_cnt++;
            else
                seats[first_hu].lian_zhuang_cnt = 2;
            dealer = first_hu;
        }
        return;
    }

    // 谁胡谁做庄
    if (hu_seat >= 0)
    {
        if (dealer == hu_seat)
            seats[dealer].lian_zhuang_cnt++;
        else
            seats[hu_seat].lian_zhuang_cnt = 2;
        dealer = hu_seat;
        return;
    }
    // 荒庄
    //没人叫牌，庄家连庄
    if (jiao_pai_cnt == 0)
    {
        seats[dealer].lian_zhuang_cnt++;
        return;
    }
    //都叫牌，庄家连庄
    if (jiao_pai_cnt == max_ready_players)
    {
        seats[dealer].lian_zhuang_cnt++;
        return;
    }
    //有3家叫了且只有一家未叫，则未叫者接庄
    if (jiao_pai_cnt == 3)
    {
        for (int i = 1; i < seat_max; i++)
        {
            if (seats[i].occupied != 1)
            {
                continue;
            }
            if (seats[i].jiao_pai == 0)
            {
                if (dealer == i)
                    seats[dealer].lian_zhuang_cnt++;
                else
                    seats[i].lian_zhuang_cnt = 2;
                dealer = i;
            }
        }
        return;
    }
    //有人叫牌，有人不叫牌。在叫牌者中，有庄家则连庄。没庄家则离庄家最近的下家接庄;
    int next = dealer;
    for (int i = 0; i < seat_max; i++)
    {
        next = (dealer + i) % seat_max;
        if (seats[next].occupied != 1)
        {
            continue;
        }
        if (seats[next].jiao_pai == 1)
        {
            if (dealer == i)
                seats[dealer].lian_zhuang_cnt++;
            else
                seats[i].lian_zhuang_cnt = 2;
            dealer = i;
        }
    }
    return;
}

int Table::handler_chi(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();
    int card = val["card"].asInt();
    int pattern[3] = {
        0,
    };
    int size = val["pattern"].size();

    for (int index = 0; index < size; index++)
    {
        pattern[index] = val["pattern"][index].asInt();
    }

    if (seat.betting == 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chi error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (cur_seat != player->seatid)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chi seat error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (last_action != PLAYER_CHU && last_action != PLAYER_GUO)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chi action  error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (last_card.value != card)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chi value  error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (actions[NOTICE_CHI] != 1)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chi notice error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    last_action = PLAYER_CHI;

    seat.guo_peng_cards.clear();
    seat.hole_cards.handler_chi(card, pattern, 0);
    seats[chu_seat].hole_cards.handler_chi(card, pattern, 1);
    seat.obsorb_seats.push_back(chu_seat);
    // handler_recored_chi(card, pattern, seat.seatid, chu_seat);

    dump_hole_cards(seat.hole_cards.cards, cur_seat, 1);
    chi_count++;

    seat.last_actions[1] = seat.last_actions[0];
    seat.last_actions[0] = action;
    seat.actions.push_front(action);

    Jpacket packet;
    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.val["card"] = card;
    packet.val["pattern"] = val["pattern"];
    packet.val["chu_seat"] = chu_seat;
    vector_to_json_array(seat.hole_cards.cards, packet, "holes");
    packet.end();
    unicast(player, packet.tostring());

    Jpacket packet1;
    packet1.val["cmd"] = SERVER_BET_SUCC_BC;
    packet1.val["seatid"] = player->seatid;
    packet1.val["uid"] = player->uid;
    packet1.val["action"] = action;
    packet1.val["card"] = card;
    packet1.val["chu_seat"] = chu_seat;
    packet1.val["pattern"] = val["pattern"];
    size = seat.hole_cards.cards.size();
    for (int i = 0; i < size; i++)
    {
        packet1.val["holes"].append(0);
    }
    packet1.end();
    broadcast(player, packet1.tostring());

    return 0;
}

int Table::handler_chu(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();
    int card = val["card"].asInt();

    if (seat.betting == 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chu error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (player->seatid != cur_seat)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chu seat error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (!seat.hole_cards.has_card(card))
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chu card error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (seat.ting == 1 && card != seat.hole_cards.last_card.value)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chu card error, because has ting. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (seat.robot_flag == 1)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chu card error, because has robot flag. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (cur_seat != player->seatid)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chu seat error, because has robot flag. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (seat.hole_cards.cards.size() % 3 != 2)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_chu card size error, because has robot flag. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    last_action = PLAYER_CHU;
    last_card.set_value(card);
    chu_seat = seat.seatid;

    seat.hole_cards.handler_chu(card);
    seat.timeout_count = 0;
    Jpacket packet;
    Jpacket packet1;
    if (card == 1 && has_chong_feng_ji != 1 && has_ze_ren_ji != 1) //1条
    {
        seat.has_chong_feng_ji = 1; //记录哪个玩家有冲锋鸡
        has_chong_feng_ji = 1;      //用于决断是否出现冲锋鸡。
        packet.val["is_chong_feng_ji"] = 1;
        packet1.val["is_chong_feng_ji"] = 1;
        seat.has_ze_ren_ji = 2; //为2的时候，表示下次谁碰了，冲锋鸡变成责任鸡
        has_ze_ren_ji = 2;
    }
    else
    {
        if (has_ze_ren_ji == 2 || seat.has_ze_ren_ji == 2)
        {
            seat.has_ze_ren_ji = 0; //为0的时候，表示下次谁碰了，冲锋鸡不会变成责任鸡
            has_ze_ren_ji = 0;
        }
        if (card == 1)
        {
            seat.has_yao_ji++;
            has_yao_ji++;
        }
        packet.val["is_chong_feng_ji"] = 0;
        packet1.val["is_chong_feng_ji"] = 0;
    }

    if (wu_gu_ji == 1)
    {
        if (card == (2 * 16 + 8) && has_chong_feng_wu_gu_ji != 1 && has_wu_gu_ze_ren_ji != 1) //八筒
        {
            seat.has_chong_feng_wu_gu_ji = 1; //记录哪个玩家有乌骨鸡
            has_chong_feng_wu_gu_ji = 1;      //用于决断是否出现乌骨鸡。
            packet1.val["is_wu_gu_ji"] = 1;
            packet.val["is_wu_gu_ji"] = 1;
            seat.has_wu_gu_ze_ren_ji = 2; //为2的时候，表示下次谁碰了，冲锋鸡变成责任鸡
            has_wu_gu_ze_ren_ji = 2;
        }
        else
        {
            if (has_wu_gu_ze_ren_ji == 2 || seat.has_wu_gu_ze_ren_ji == 2) //第2个八筒，即使碰了也不会变成责任鸡
            {
                seat.has_wu_gu_ze_ren_ji = 0; //为0的时候，表示下次谁碰了，乌骨鸡不会变成责任鸡
                has_wu_gu_ze_ren_ji = 0;
            }
            if (card == 2 * 16 + 8)
            {
                seat.has_wu_gu_ji++;
                has_wu_gu_ji++;
            }
            packet1.val["is_wu_gu_ji"] = 0;
            packet.val["is_wu_gu_ji"] = 0;
        }
    }

    // handler_record_chu(card, seat.seatid);
    dump_hole_cards(seat.hole_cards.cards, cur_seat, 5);

    seat.last_actions[1] = seat.last_actions[0];
    seat.last_actions[0] = action;
    seat.actions.push_front(action);

    seat.lian_gang_cards.clear();
    seat.guo_peng_cards.clear();
    // 测试可否胡牌
    // seat.hole_cards.analysis();
    if (seat.pao_hu_flag == 1)
    {
        seat.pao_hu_flag = 0;
        pao_hu_seat = -1;
    }

    if (seat.gang_hu_flag == 1)
    {
        seat.gang_hu_flag = 0;
        gang_hu_seat = -1;
    }

    int pg_flag = 0;
    for (int i = 0; i < seat_max; i++)
    {
        if (i == player->seatid)
        {
            continue;
        }

        if (seats[i].hole_cards.permit_peng(card) || seats[i].hole_cards.permit_gang(card))
        {
            pg_flag = 1;
            mjlog.debug("next peng gang seat is seat[%d]\n", i);
            break;
        }
    }

    for (int i = 0; i < seat_max; i++)
    {
        seats[i].handler_flag = 0;
    }

    wait_handle_action = action;
    wait_handler_seat = player->seatid;

    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.val["card"] = card;
    packet.val["time_out"] = 0;
    packet.val["pg_flag"] = pg_flag;
    packet.val["chu_seat"] = -1;
    vector_to_json_array(seat.hole_cards.cards, packet, "holes");

    seat.hole_cards.handler_ting(card);
    if (seat.hole_cards.hu_cards.size() > 0)
    {
        vector_to_json_array(seat.hole_cards.hu_cards, packet, "hu_cards");
    }

    packet.end();
    unicast(player, packet.tostring());
    replay.append_record(packet.tojson());

    packet1.val["cmd"] = SERVER_BET_SUCC_BC;
    packet1.val["seatid"] = player->seatid;
    packet1.val["uid"] = player->uid;
    packet1.val["action"] = action;
    packet1.val["card"] = card;
    packet1.val["time_out"] = 0;
    packet1.val["pg_flag"] = pg_flag;
    packet1.val["chu_seat"] = -1;

    int size = seat.hole_cards.cards.size();
    for (int i = 0; i < size; i++)
    {
        packet1.val["holes"].append(0);
    }
    packet1.end();
    broadcast(player, packet1.tostring());
    return 0;
}

int Table::handler_peng(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();
    int card = val["card"].asInt();

    if (seat.betting == 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_peng error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (cur_seat != player->seatid)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_peng seat error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (last_action != PLAYER_CHU && last_action != PLAYER_GUO)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_peng action  error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (last_card.value != card || actions[NOTICE_PENG] != 1)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_peng value  error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    last_action = PLAYER_PENG;
    seat.hole_cards.handler_peng(card, 0);
    seat.guo_hu_cards.clear();
    seat.guo_peng_cards.clear();
    seats[chu_seat].hole_cards.handler_peng(card, 1);
    // handler_recored_peng(card, seat.seatid, chu_seat);
    dump_hole_cards(seat.hole_cards.cards, cur_seat, 2);

    seat.obsorb_seats.push_back(chu_seat);

    Jpacket packet;
    Jpacket packet1;
    if (card == 1 && seats[chu_seat].has_chong_feng_ji == 1 && has_ze_ren_ji == 2)
    {
        seats[chu_seat].has_ze_ren_ji = 1;
        has_ze_ren_ji = 1;
        seats[chu_seat].has_chong_feng_ji = 0;
        has_chong_feng_ji = 0;
        packet.val["ze_ren_ji"] = 1;
        packet1.val["ze_ren_ji"] = 1;
    }
    if (wu_gu_ji == 1)
    {
        if (card == 2 * 16 + 8 && seats[chu_seat].has_chong_feng_wu_gu_ji == 1 && has_wu_gu_ze_ren_ji == 2)
        {
            seats[chu_seat].has_wu_gu_ze_ren_ji = 1;
            has_wu_gu_ze_ren_ji = 1;
            seats[chu_seat].has_chong_feng_wu_gu_ji = 0;
            has_chong_feng_wu_gu_ji = 0;
            packet.val["wu_gu_ze_ren_ji"] = 1;
            packet1.val["wu_gu_ze_ren_ji"] = 1;
        }
    }

    if (seat.peng_record.find(chu_seat) == seat.peng_record.end())
    {
        seat.peng_record[chu_seat] = 1;
    }
    else
    {
        seat.peng_record[chu_seat] += 1;
    }
    seat.peng_card_record[card] = chu_seat;
    peng_count++;
    seat.last_actions[1] = seat.last_actions[0];
    seat.last_actions[0] = action;
    seat.actions.push_front(action);

    if (seat.pao_hu_flag == 1)
    {
        seat.pao_hu_flag = 0;
        pao_hu_seat = -1;
    }

    if (seat.gang_hu_flag == 1)
    {
        seat.gang_hu_flag = 0;
        gang_hu_seat = -1;
    }

    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.val["card"] = card;
    packet.val["chu_seat"] = chu_seat;
    vector_to_json_array(seat.hole_cards.cards, packet, "holes");
    packet.end();
    unicast(player, packet.tostring());
    replay.append_record(packet.tojson());

    packet1.val["cmd"] = SERVER_BET_SUCC_BC;
    packet1.val["seatid"] = player->seatid;
    packet1.val["uid"] = player->uid;
    packet1.val["action"] = action;
    packet1.val["card"] = card;
    packet1.val["chu_seat"] = chu_seat;
    int size = seat.hole_cards.cards.size();
    for (int i = 0; i < size; i++)
    {
        packet1.val["holes"].append(0);
    }
    packet1.end();
    broadcast(player, packet1.tostring());

    //if (seat.peng_record[chu_seat] == 1)
    //{
    //broadcast_forbid_hu(seats[chu_seat].player, 1);
    //}
    //else if (seat.peng_record[chu_seat] == 2)
    //{
    //broadcast_forbid_hu(seats[chu_seat].player, 2);
    //}
    //else if (seat.peng_record[chu_seat] == 3)
    //{
    //seats[chu_seat].forbid_hu = 1;
    //seats[chu_seat].forbided_seatid = player->seatid;
    //forbid_hu_record[seat.seatid] = chu_seat;
    //broadcast_forbid_hu(seats[chu_seat].player, 3);
    //}

    return 0;
}

int Table::handler_gang(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();
    int card = val["card"].asInt();
    int gang_flag = val["gang_flag"].asInt();

    if (seat.betting == 0)
    {
        handler_prop_error(player, action);
        mjlog.error("handler_gang error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (cur_seat != player->seatid)
    {
        handler_prop_error(player, action);
        mjlog.error("handler_gang seat error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (actions[NOTICE_GANG] != 1)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_gang notice error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    vector<Card>::iterator bit = seat.hole_cards.gang_cards.begin();
    vector<Card>::iterator eit = seat.hole_cards.gang_cards.end();

    if (find(bit, eit, card) == eit)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_gang flag error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    int i = 0;
    for (; bit != eit; bit++)
    {
        if (seat.hole_cards.gang_cards[i].value == card)
        {
            gang_flag = seat.hole_cards.gang_flags[i];
            break;
        }
        i++;
    }

    last_action = PLAYER_GANG;
    seat.hole_cards.handler_gang(card, gang_flag, 0);
    seat.guo_hu_cards.clear();
    seat.guo_peng_cards.clear();

    seat.last_actions[1] = seat.last_actions[0];
    seat.last_actions[0] = action;
    seat.actions.push_front(action);

    if (gang_flag == 0)
    {
        seat.obsorb_seats.push_back(chu_seat);
    }
    else if (gang_flag > 1)
    {
        seat.obsorb_seats.push_back(-1);
    }

    dump_hole_cards(seat.hole_cards.cards, cur_seat, 3);
    last_gang_card.set_value(card);
    seat.lian_gang_cards.push_back(card);

    wait_handle_action = action;
    wait_handler_seat = player->seatid;

    Jpacket packet;
    Jpacket packet1;
    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    // 只有是杠别的玩家的牌的时候才需要下面的处理
    if (gang_flag == 0)
    {
        seats[chu_seat].hole_cards.handler_gang(card, gang_flag, 1);
        // handler_recored_gang(card, seat.seatid, chu_seat, gang_flag);

        if (seat.peng_record.find(chu_seat) == seat.peng_record.end())
        {
            seat.peng_record[chu_seat] = 1;
        }
        else
        {
            seat.peng_record[chu_seat] += 1;
        }

        if ( card == 1 && seats[chu_seat].has_chong_feng_ji == 1 && has_ze_ren_ji == 2)
        {
            seats[chu_seat].has_ze_ren_ji = 1;
            has_ze_ren_ji = 1;
            seats[chu_seat].has_chong_feng_ji = 0;
            has_chong_feng_ji = 0;
            packet.val["ze_ren_ji"] = 1;
            packet1.val["ze_ren_ji"] = 1;
        }
        if (wu_gu_ji == 1)
        {
            if ( card == 2*16 + 8 && seats[chu_seat].has_chong_feng_wu_gu_ji == 1 && has_wu_gu_ze_ren_ji == 2)
            {
                seats[chu_seat].has_wu_gu_ze_ren_ji = 1;
                has_wu_gu_ze_ren_ji = 1;
                seats[chu_seat].has_chong_feng_wu_gu_ji = 0;
                has_chong_feng_wu_gu_ji = 0;
                packet.val["wu_gu_ze_ren_ji"] = 1;
                packet1.val["wu_gu_ze_ren_ji"] = 1;
            }
        }
    }
    else
    {
        // handler_recored_gang(card, seat.seatid, -1, gang_flag);
    }

    if (gang_flag == 0)
    {
        seat.gang_seats.push_back(chu_seat);
        seat.total_ming_gang++;
        seat.score_from_players_detail[chu_seat][MING_GANG_TYPE] += (max_ready_players -1) * MING_GANG_BET;
        // score_from_players_item_count[player->seatid][MING_GANG_TYPE]++;
        // score_to_players_item_count[chu_seat][MING_GANG_TYPE]++;
    }
    else if (gang_flag == 1)
    {
        seat.gang_seats.push_back(seat.peng_card_record[card]);
        seat.total_ming_gang++;
        seat.score_from_players_detail[-1][PENG_GANG_TYPE] += PENG_GANG_BET;
        // score_from_players_item_count[player->seatid][PENG_GANG_TYPE]++;
        for (int j = 0; j < seat_max; j++)
        {
            if (seats[j].ready != 1 || player->seatid == j)
            {
                continue;
            }
            // score_to_players_item_count[j][PENG_GANG_TYPE]++;
        }
    }
    else
    {
        seat.gang_seats.push_back(-1);
        seat.total_an_gang++;
        seat.score_from_players_detail[-1][AN_GANG_TYPE] += AN_GANG_BET;
        // score_from_players_item_count[player->seatid][AN_GANG_TYPE]++;
        for (int j = 0; j < seat_max; j++)
        {
            if (seats[j].ready != 1 || player->seatid == j)
            {
                continue;
            }
            // score_to_players_item_count[j][AN_GANG_TYPE]++;
        }
    }

    if (seat.pao_hu_flag == 1)
    {
        seat.pao_hu_flag = 0;
        pao_hu_seat = -1;
    }

    if (seat.gang_hu_flag == 1)
    {
        seat.gang_hu_flag = 0;
        gang_hu_seat = -1;
    }

    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.val["card"] = card;
    packet.val["gang_flag"] = gang_flag;
    vector_to_json_array(seat.hole_cards.cards, packet, "holes");
    if (gang_flag == 0)
    {
        packet.val["chu_seat"] = chu_seat;
    }
    else if (gang_flag == 1)
    {
        packet.val["chu_seat"] = seat.peng_card_record[card];
    }
    else
    {
        packet.val["chu_seat"] = -1;
    }
    //vector_to_json_array(seat.hole_cards.obsorb_cards, packet, "obsorb_holes");
    packet.end();
    unicast(player, packet.tostring());
    replay.append_record(packet.tojson());

    packet1.val["cmd"] = SERVER_BET_SUCC_BC;
    packet1.val["seatid"] = player->seatid;
    packet1.val["uid"] = player->uid;
    packet1.val["action"] = action;
    packet1.val["card"] = card;
    packet1.val["gang_flag"] = gang_flag;
    if (gang_flag == 0)
    {
        packet1.val["chu_seat"] = chu_seat;
    }
    else if (gang_flag == 1)
    {
        packet1.val["chu_seat"] = seat.peng_card_record[card];
    }
    else
    {
        packet1.val["chu_seat"] = -1;
    }
    //vector_to_json_array(seat.hole_cards.obsorb_cards, packet1, "obsorb_holes");
    int size = seat.hole_cards.cards.size();
    for (int i = 0; i < size; i++)
    {
        packet1.val["holes"].append(0);
    }
    packet1.end();
    broadcast(player, packet1.tostring());

    seat.gang_count[gang_flag]++;
    gang_count++;

    if (gang_flag == 0)
    {
        seats[chu_seat].fang_gang_count++;

        //if (seat.peng_record[chu_seat] == 1)
        //{
        //broadcast_forbid_hu(seats[chu_seat].player, 1);
        //}
        //else if (seat.peng_record[chu_seat] == 2)
        //{
        //broadcast_forbid_hu(seats[chu_seat].player, 2);
        //}
        //else if (seat.peng_record[chu_seat] == 3)
        //{
        //seats[chu_seat].forbid_hu = 1;
        //seats[chu_seat].forbided_seatid = player->seatid;
        //forbid_hu_record[seat.seatid] = chu_seat;
        //broadcast_forbid_hu(seats[chu_seat].player, 3);
        //}
    }
    else if (gang_flag == 1)
    {
        //int fang_gang_seat = seat.peng_card_record[card];
        //if (fang_gang_seat >= 0)
        //{
        //  seats[fang_gang_seat].fang_gang_count++;
        //}
    }

    return 0;
}

int Table::handler_hu(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();
    int card = val["card"].asInt();
    int hu_flag = 0;

    if (seat.betting == 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_hu error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (cur_seat != player->seatid)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_hu seatid error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (actions[NOTICE_HU] != 1)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_hu state error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (seat.hole_cards.cards.size() % 3 == 1)
    {
        if (card != last_card.value && card != last_gang_card.value)
        {
            handler_bet_error(player, action);
            mjlog.error("handler_hu error. betting[1] uid[%d] seatid[%d] tid[%d].\n",
                        player->uid, player->seatid, tid);
            return -1;
        }
    }

    if (seat.hole_cards.size() % 3 == 1)
    {
        if (seat.hole_cards.permit_hu(card))
        {
            win_seatid = player->seatid;
            hu_flag = 0;
        }
    }
    else if (seat.hole_cards.permit_hu())
    {
        win_seatid = player->seatid;
        hu_flag = 1;
    }

    if (win_seatid < 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_hu error. betting[2] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    last_action = PLAYER_HU;

    seat.last_actions[1] = seat.last_actions[0];
    seat.last_actions[0] = action;
    seat.actions.push_front(action);

    //被胡玩家出牌之前的操作是杠,则是热炮牌型。
    if (seats[chu_seat].last_actions[1] == PLAYER_GANG && seats[chu_seat].last_actions[0] == PLAYER_CHU && (pao_hu_seat >= 0 || gang_hu_seat >= 0))
    {
        is_re_pao = 1;
        mjlog.debug("handler_hu is re pao\n");
    }
    //被胡玩家是碰杠,则是抢扛牌型。
    if (gang_hu_seat >= 0)
    {
        is_qiang_gang = 1;
        mjlog.debug("handler_hu is qiang pao\n");
    }
    vector<Card> cards = seat.hole_cards.cards;
    if (cards.size() % 3 == 2)
    {
        vector<Card>::iterator it = find(cards.begin(), cards.end(), card);
        if (it != cards.end())
        {
            cards.erase(it);
        }
    }

    hu_seat = player->seatid;
    hu_card = card;

    if (hu_flag == 0)
    {
        // handler_recored_hu(card, seat.seatid, chu_seat);
        seat.lian_gang_cards.clear();
    }
    else
    {
        // handler_recored_hu(0, seat.seatid, -1);
    }

    Jpacket packet;
    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.val["card"] = card;
    packet.val["hu_flag"] = hu_flag;
    vector_to_json_array(cards, packet, "holes");
    packet.end();
    unicast(player, packet.tostring());
    replay.append_record(packet.tojson());

    Jpacket packet1;
    packet1.val["cmd"] = SERVER_BET_SUCC_BC;
    packet1.val["seatid"] = player->seatid;
    packet1.val["uid"] = player->uid;
    packet1.val["action"] = action;
    packet1.val["card"] = card;
    packet1.val["hu_flag"] = hu_flag;
    vector_to_json_array(cards, packet1, "holes");
    packet1.end();
    broadcast(player, packet1.tostring());

    return 0;
}

int Table::handler_guo(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();

    if (seat.betting == 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_guo error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (cur_seat != player->seatid)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_guo seat error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    // 过胡不胡
    if (actions[NOTICE_HU] == 1 && seat.hole_cards.size() % 3 != 2)
    {
        seat.guo_hu_cards.push_back(last_card.value);
    }

    // 过碰不碰
    if (actions[NOTICE_PENG] == 1 && seat.hole_cards.size() % 3 == 1)
    {
        seat.guo_peng_cards.push_back(last_card.value);
    }

    last_action = PLAYER_GUO;

    // handler_recored_guo(0, seat.seatid, chu_seat);

    Jpacket packet;
    packet.val["cmd"] = SERVER_BET_SUCC_UC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.end();
    unicast(player, packet.tostring());
    replay.append_record(packet.tojson());

    if (seat.pao_hu_flag == 1)
    {
        seat.pao_hu_flag = 0;
        pao_hu_seat = -1;
    }

    if (seat.gang_hu_flag == 1)
    {
        seat.gang_hu_flag = 0;
        gang_hu_seat = -1;
    }

    seat.handler_flag = 1;

    if (seat.hole_cards.size() % 3 == 2)
    {
    }
    else
    {
        count_next_bet();
    }

    return 0;
}

int Table::handler_ting(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();
    int card = val["card"].asInt();

    if (seat.betting == 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_ting error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    last_card.set_value(card);
    chu_seat = seat.seatid;
    seat.hole_cards.handler_chu(card);
    int ret = seat.hole_cards.handler_ting(card);
    if (ret < 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_ting error. card[%d] uid[%d] seatid[%d] tid[%d].\n",
                    card, player->uid, player->seatid, tid);
        return -1;
    }

    if (seat.last_actions[0] == -1)
    {
        seat.is_bao_ting = 1;
        mjlog.debug("is bao ting\n");
    }

    last_action = PLAYER_TING;
    seat.ting = 1;

    Jpacket packet;
    Jpacket packet1;
    if (card == 1 && has_chong_feng_ji != 1 && has_ze_ren_ji != 1) //1条
    {
        seat.has_chong_feng_ji = 1; //记录哪个玩家有冲锋鸡
        has_chong_feng_ji = 1;      //用于决断是否出现冲锋鸡。
        packet.val["is_chong_feng_ji"] = 1;
        packet1.val["is_chong_feng_ji"] = 1;
        seat.has_ze_ren_ji = 2; //为2的时候，表示下次谁碰了，冲锋鸡变成责任鸡
        has_ze_ren_ji = 2;
    }
    else
    {
        if (has_ze_ren_ji == 2 || seat.has_ze_ren_ji == 2)
        {
            seat.has_ze_ren_ji = 0; //为0的时候，表示下次谁碰了，冲锋鸡不会变成责任鸡
            has_ze_ren_ji = 0;
        }
        if (card == 1)
        {
            seat.has_yao_ji++;
            has_yao_ji++;
        }
        packet.val["is_chong_feng_ji"] = 0;
        packet1.val["is_chong_feng_ji"] = 0;
    }

    if (wu_gu_ji == 1)
    {
        if (card == (2 * 16 + 8) && has_chong_feng_wu_gu_ji != 1 && has_wu_gu_ze_ren_ji != 1) //八筒
        {
            seat.has_chong_feng_wu_gu_ji = 1; //记录哪个玩家有乌骨鸡
            has_chong_feng_wu_gu_ji = 1;      //用于决断是否出现乌骨鸡。
            packet1.val["is_wu_gu_ji"] = 1;
            packet.val["is_wu_gu_ji"] = 1;
            seat.has_wu_gu_ze_ren_ji = 2; //为2的时候，表示下次谁碰了，冲锋鸡变成责任鸡
            has_wu_gu_ze_ren_ji = 2;
        }
        else
        {
            if (has_wu_gu_ze_ren_ji == 2 || seat.has_wu_gu_ze_ren_ji == 2) //第2个八筒，即使碰了也不会变成责任鸡
            {
                seat.has_wu_gu_ze_ren_ji = 0; //为0的时候，表示下次谁碰了，乌骨鸡不会变成责任鸡
                has_wu_gu_ze_ren_ji = 0;
            }
            if (card == 2 * 16 + 8)
            {
                seat.has_wu_gu_ji++;
                has_wu_gu_ji++;
            }
            packet1.val["is_wu_gu_ji"] = 0;
            packet.val["is_wu_gu_ji"] = 0;
        }
    }

    seat.last_actions[1] = seat.last_actions[0];
    seat.last_actions[0] = action;
    seat.actions.push_front(action);

    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.val["card"] = card;
    if (seat.hole_cards.hu_cards.size() > 0)
    {
        vector_to_json_array(seat.hole_cards.hu_cards, packet, "hu_cards");
    }
    vector_to_json_array(seat.hole_cards.cards, packet, "holes");
    packet.end();
    unicast(player, packet.tostring());
    replay.append_record(packet.tojson());

    packet1.val["cmd"] = SERVER_BET_SUCC_BC;
    packet1.val["seatid"] = player->seatid;
    packet1.val["uid"] = player->uid;
    packet1.val["action"] = action;
    packet1.val["card"] = card;
    packet1.end();
    broadcast(player, packet1.tostring());

    return 0;
}

int Table::handler_cancel(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();

    if (seat.betting == 0)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_cancel error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    last_action = PLAYER_CANCEL;
    seat.ting = 0;

    Jpacket packet;
    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.end();
    unicast(player, packet.tostring());
    replay.append_record(packet.tojson());

    return 0;
}

int Table::handler_jiabei(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int action = val["action"].asInt();

    if (actions[NOTICE_JIABEI] != 1)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_jiabei error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    if (seat.jiabei >= MAX_JIABEI)
    {
        handler_bet_error(player, action);
        mjlog.error("handler_jiabei limit error. betting[0] uid[%d] seatid[%d] tid[%d].\n",
                    player->uid, player->seatid, tid);
        return -1;
    }

    seat.hu = 0;
    seat.jiabei *= 2;
    actions[NOTICE_JIABEI] = 0;
    last_action = PLAYER_JIABEI;

    Jpacket packet;
    packet.val["cmd"] = SERVER_BET_SUCC_BC;
    packet.val["seatid"] = player->seatid;
    packet.val["uid"] = player->uid;
    packet.val["action"] = action;
    packet.end();
    unicast(player, packet.tostring());

    return 0;
}

void Table::reset_actions()
{
    for (int i = 0; i < NOTICE_SIZE; i++)
    {
        actions[i] = 0;
    }
}

int Table::handler_dismiss_table(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    int agree = val["flag"].asInt();

    if (round_count == 0)
    {
        if (player->uid == owner_uid)
        {
            Jpacket packet;
            packet.val["cmd"] = SERVER_DISMISS_TABLE_SUCC_BC;
            packet.val["uid"] = player->uid;
            packet.val["name"] = player->name;
            packet.val["seatid"] = player->seatid;
            packet.val["agree"] = agree;
            packet.val["flag"] = 0;
            packet.end();
            broadcast(NULL, packet.tostring());

            if (transfer_flag)
            {
                //int create_rmb = Game::get_create_rmb();
                int base_play_board = zjh.conf["tables"]["base_board"].asInt();
                base_play_board = base_play_board <= 0 ? 8 : base_play_board;
                int ratio = 1;
                if (max_play_board <= base_play_board || base_play_board == 0)
                {
                    ratio = 1;
                }
                else
                {
                    ratio = max_play_board / base_play_board;
                }

                if (players.find(origin_owner_uid) != players.end())
                {
                    players[origin_owner_uid]->incr_rmb(create_rmb * ratio);
                    Player *player1 = players[origin_owner_uid];
                    insert_flow_log((int)time(NULL), player1->uid, player1->remote_ip, 0, type, zid, ttid, type, 1, 1 * ratio, player1->rmb);
                }
            }

            clean_table();
            reset();
        }
        else
        {
            zjh.game->del_player(player);
        }

        return 0;
    }

    // 有一个人拒绝就取消解散房间
    if (agree == 0)
    {
        seats[player->seatid].dismiss = 2;
        broadcast_dismiss_status(4);
        ev_timer_stop(zjh.loop, &dismiss_timer);
        dismiss.clear();
        dismiss_flag = 0;
        dismiss_uid = 0;

        for (int i = 0; i < seat_max; i++)
        {
            seats[i].dismiss = 0;
        }
        return 0;
    }

    //int dismiss_size = zjh.conf["tables"]["min_dis_players"].asInt();

    // 第一个人申请解散
    if (dismiss.size() == 0)
    {
        dismiss_uid = player->uid;
        dismiss_flag = 1;
        seats[player->seatid].dismiss = 1;
        dismiss.insert(player->uid);

        ev_timer_again(zjh.loop, &dismiss_timer);
        broadcast_dismiss_status(1);
    }
    else if (dismiss.find(player->uid) == dismiss.end())
    {
        seats[player->seatid].dismiss = 1;
        dismiss.insert(player->uid);

        if (static_cast<int>(dismiss.size()) >= (max_ready_players - 1)) // 满足解散请求同意人数
        {
            broadcast_dismiss_status(3);
            ev_timer_stop(zjh.loop, &dismiss_timer);

            for (int i = 0; i < seat_max; i++)
            {
                seats[i].bet = 0;
            }

            game_end(1);
            dismiss.clear();
            reset();
        }
        else
        {
            broadcast_dismiss_status(2);
        }
    }

    return 0;
}

void Table::broadcast_dismiss_status(int flag)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_DISMISS_TABLE_SUCC_BC;
    packet.val["flag"] = flag;

    packet.val["dismiss"] = dismiss_flag;
    if (dismiss_flag)
    {
        packet.val["dismiss_time"] = (int)ev_timer_remaining(zjh.loop, &dismiss_timer);
        packet.val["dismiss_uid"] = dismiss_uid;
    }
    else
    {
        packet.val["dismiss_time"] = 0;
        packet.val["dismiss_uid"] = 0;
    }

    int i = 0;
    std::map<int, Player *>::iterator it;
    for (it = players.begin(); it != players.end(); it++)
    {
        Player *p = it->second;
        Seat &seat = seats[p->seatid];

        packet.val["players"][i]["uid"] = p->uid;
        packet.val["players"][i]["name"] = p->name;
        packet.val["players"][i]["seatid"] = p->seatid;
        packet.val["players"][i]["dismiss"] = seat.dismiss;
        i++;
    }

    packet.end();
    broadcast(NULL, packet.tostring());
}

void Table::clean_table()
{
    std::map<int, Player *>::iterator it = players.begin();
    std::map<int, Player *>::iterator nit = players.begin();

    state = ROOM_WAIT_GAME;

    if (round_count == 0 && substitute == 1)
    {
        Player::incr_rmb(owner_uid, create_rmb);
        zjh.game->del_subs_table(owner_uid);
    }

    if (round_count > 0)
    {
        if (!transfer_flag && substitute != 1)
        {
            /*
            players[owner_uid]->incr_rmb(0 - create_rmb * ratio);
            Player* player = players[owner_uid];
            insert_flow_log((int)time(NULL), player->uid, player->remote_ip, 0, type, zid, ttid, type, 0, 1 * ratio, player->rmb);
            */
            create_table_cost();
        }

        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].player != NULL)
            {
                if (seats[i].player->uid == owner_uid)
                {
                    seats[i].player->incr_total_create(1);
                }
                seats[i].player->incr_total_play(1);
            }
        }
    }

    for (int i = 0; i < seat_max; i++)
    {
        seats[i].dismiss = 0;
    }

    for (; it != players.end(); it = nit)
    {
        ++nit;
        if (it->second != NULL)
        {
            for (int i = 0; i < seat_max; i++)
            {
                if (it->second == seats[i].player)
                {
                    mjlog.debug("clean table");
                    zjh.game->del_player(it->second);
                    break;
                }
            }
        }
    }

    ready_players = 0;
    cur_players = 0;
    round_count = 0;
    dismiss_flag = 0;
    dismiss_uid = 0;

    if (substitute == 1)
    {
        substitute = 0;
        zjh.game->substitute_tables.erase(tid);
        clear_substitute_info();
        modify_substitute_info(2);
    }

    zjh.temp_rc->command("LPUSH tids_list %d", ttid);
    zjh.game->table_ttid.erase(ttid);
    zjh.game->set_table_flag(ttid, 0);
    zjh.game->table_owners.erase(owner_uid);

    ttid = -1;
    owner_uid = -1;
    owner_sex = -1;
    owner_name = "";
    owner_remote_ip = "";
    origin_owner_uid = -1;
    transfer_flag = false;
    max_ready_players = seat_max;
    //hu_seat = -1;
    get_card_seat = -1;
    ahead_start_flag = 0;
    ahead_start_uid = -1;
    game_end_flag = 0;
    dismiss.clear();
    redpackes.clear();

    for (int i = 0; i < seat_max; i++)
    {
        seats[i].clear();
    }

    reset();

    ev_timer_stop(zjh.loop, &preready_timer);
    ev_timer_stop(zjh.loop, &ready_timer);
    ev_timer_stop(zjh.loop, &dismiss_timer);
    ev_timer_stop(zjh.loop, &subs_timer);
    ev_timer_stop(zjh.loop, &ahead_start_timer);
    //ev_timer_stop(zjh.loop, &single_ready_timer);
}

void Table::dismiss_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->dismiss_timer);

    table->dismiss_timeout();
}

int Table::dismiss_timeout()
{
    broadcast_dismiss_status(5);

    for (int i = 0; i < seat_max; i++)
    {
        seats[i].bet = 0;
    }

    game_end(1);
    dismiss.clear();
    reset();
    return 0;
}

void Table::vector_to_json_string(std::vector<Card> &cards, Jpacket &packet, string key)
{
    char buff[256] = {
        0,
    };
    int len = 0;

    if (cards.size() > 0)
    {
        for (unsigned int i = 0; i < cards.size() - 1; i++)
        {
            len = strlen(buff);
            snprintf(buff + len, sizeof(buff) - len, "%d_", cards[i].value);
        }

        len = strlen(buff);
        snprintf(buff + len, sizeof(buff) - len, "%d", cards[cards.size() - 1].value);
        len = strlen(buff);
        buff[len] = '\0';
    }

    packet.val[key] = buff;
}

int Table::insert_flow_record()
{
    Jpacket packet;
    char rid[32] = {
        0,
    };
    char replay_key[32] = {
        0,
    };
    char replayid[64] = {
        0,
    };

    snprintf(rid, 32, "%d_%d", round_ts, ttid);
    snprintf(replay_key, 32, "%d_%d", ts, ttid);
    packet.val["roundid"] = rid;
    packet.val["replay_key"] = replay_key;
    packet.val["tid"] = ttid;
    packet.val["create_time"] = (int)time(NULL);

    packet.val["did"] = seats[dealer].player->uid;
    packet.val["dname"] = seats[dealer].player->name;
    packet.val["dbet"] = seats[dealer].bet;
    packet.val["dip"] = seats[dealer].player->remote_ip;

    packet.val["owner_uid"] = owner_uid;
    packet.val["owner_name"] = owner_name;
    packet.val["substitute"] = substitute;

    const char *prefix = zjh.conf["replay_prefix"].asCString();
    snprintf(replayid, sizeof(replayid), "%s/%d_%d", prefix, ts, ttid);
    packet.val["replay_url"] = replayid;
    snprintf(replayid, sizeof(replayid), "%d%d", ttid, ts % 300000);
    packet.val["replayid"] = replayid;

    vector_to_json_string(seats[dealer].hole_cards.cards, packet, "dcards");

    // std::vecotr<vecotr<Card> >::iterator it = seats[dealer].hole_cards.obsorb_cards.begin();

    // for (; it != seats[dealer].hole_cards.obsorb_cards.end(); it++)
    // {
    //     vector_to_json_array(*it, packet, "docards");
    // }

    int index = 1;
    for (int i = 0; i < seat_max; i++)
    {
        if (i == dealer || seats[i].occupied == 0)
        {
            mjlog.info("insert_flow_record seatid %d don't dealed\n", i);
            continue;
        }

        Player *player = seats[i].player;

        switch (index)
        {
        case 1:
            packet.val["x1id"] = player->uid;
            packet.val["x1name"] = player->name;
            packet.val["x1bet"] = seats[i].bet;
            //packet.val["x1ratio"] = seats[i].bet_ratio;
            packet.val["x1ip"] = player->remote_ip;
            vector_to_json_string(seats[i].hole_cards.cards, packet, "x1cards");
            //vector_to_json_array(seats[i].hole_cards.obsorb_cards, packet, "x1ocards");
            break;

        case 2:
            packet.val["x2id"] = player->uid;
            packet.val["x2name"] = player->name;
            packet.val["x2bet"] = seats[i].bet;
            packet.val["x2ip"] = player->remote_ip;
            //packet.val["x2ratio"] = seats[i].bet_ratio;
            vector_to_json_string(seats[i].hole_cards.cards, packet, "x2cards");
            //vector_to_json_array(seats[i].hole_cards.obsorb_cards, packet, "x2ocards");
            break;

        case 3:
            packet.val["x3id"] = player->uid;
            packet.val["x3name"] = player->name;
            packet.val["x3bet"] = seats[i].bet;
            packet.val["x3ip"] = player->remote_ip;
            //packet.val["x3ratio"] = seats[i].bet_ratio;
            vector_to_json_string(seats[i].hole_cards.cards, packet, "x3cards");
            //vector_to_json_array(seats[i].hole_cards.obsorb_cards, packet, "x3ocards");
            break;

        default:
            break;
        }
        index++;
    }

    string str = packet.val.toStyledString().c_str();

    int ret = zjh.temp_rc->command("LPUSH flow_list %s", str.c_str());

    if (ret < 0)
    {
        mjlog.error("insert_flow_record %s", str.c_str());
        return -1;
    }

    return 0;
}

int Table::insert_flow_round_record()
{
    Jpacket packet;
    char rid[32] = {
        0,
    };

    int init_money = zjh.conf["tables"]["init_money"].asInt();

    snprintf(rid, 32, "%d_%d", round_ts, ttid);
    packet.val["roundid"] = rid;
    packet.val["tid"] = ttid;
    packet.val["create_time"] = (int)time(NULL);
    packet.val["table_owner"] = owner_uid;
    packet.val["owner_name"] = owner_name;
    packet.val["substitute"] = substitute;
    packet.val["type"] = zjh.conf["tables"]["type"].asInt();

    int index = 1;
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].occupied == 0)
        {
            continue;
        }

        Player *player = seats[i].player;

        switch (index)
        {
        case 1:
            packet.val["p1id"] = player->uid;
            packet.val["p1name"] = player->name;
            packet.val["p1bet"] = player->money - init_money;
            packet.val["p1ip"] = player->remote_ip;
            break;
        case 2:
            packet.val["p2id"] = player->uid;
            packet.val["p2name"] = player->name;
            packet.val["p2bet"] = player->money - init_money;
            packet.val["p2ip"] = player->remote_ip;
            break;
        case 3:
            packet.val["p3id"] = player->uid;
            packet.val["p3name"] = player->name;
            packet.val["p3bet"] = player->money - init_money;
            packet.val["p3ip"] = player->remote_ip;
            break;
        case 4:
            packet.val["p4id"] = player->uid;
            packet.val["p4name"] = player->name;
            packet.val["p4bet"] = player->money - init_money;
            packet.val["p4ip"] = player->remote_ip;
        default:
            break;
        }
        index++;
    }

    int owner_flag = 0;
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].occupied == 0)
        {
            mjlog.error("insert_flow_round_record seatid %d don't dealed\n");
            continue;
        }

        Player *player = seats[i].player;
        packet.val["uid"] = player->uid;
        packet.val["uname"] = player->name;
        packet.val["uip"] = player->remote_ip;

        if (player->uid == owner_uid)
        {
            owner_flag = 1;
        }

        string str = packet.val.toStyledString().c_str();
        int ret = zjh.temp_rc->command("LPUSH flow_round_list %s", str.c_str());

        if (ret < 0)
        {
            mjlog.error("insert_flow_round_record %s", str.c_str());
        }
    }

    if (owner_flag == 0 && substitute == 1)
    {
        packet.val["uid"] = owner_uid;
        packet.val["uname"] = owner_name;
        packet.val["uip"] = owner_remote_ip;
        string str = packet.val.toStyledString().c_str();
        int ret = zjh.temp_rc->command("LPUSH flow_round_list %s", str.c_str());

        if (ret < 0)
        {
            mjlog.error("insert_flow_round_record %s", str.c_str());
        }
    }
    return 0;
}

void Table::handler_net_status(Player *player, int status)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_NET_STATUS_BC;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.val["name"] = player->name;
    packet.val["money"] = player->money;
    packet.val["rmb"] = player->rmb;
    packet.val["total_board"] = player->total_board;
    packet.val["status"] = status;

    packet.end();
    broadcast(player, packet.tostring());
}

int Table::calculate_base_score(int sid, int pao, int card_value)
{
    Seat &seat = seats[sid];
    if (card_value >= 0)
    {
        seat.hole_cards.analysis_card_type(card_value);
    }
    else if (pao_hu_seat >= 0)
    {
        seat.hole_cards.analysis_card_type(last_card.value);
    }
    else if (gang_hu_seat >= 0)
    {
        seat.hole_cards.analysis_card_type(last_gang_card.value);
    }
    else
    {
        seat.hole_cards.analysis_card_type();
    }

    seat.card_type = seat.hole_cards.card_type;

    int score = 0;

    switch (seat.card_type)
    {
    case CARD_TYPE_PING_HU:
        score = 1;
        seat.hu_pai_lei_xing = "平胡";
        break;
    case CARD_TYPE_PENG_HU:
        score = 5;
        seat.hu_pai_lei_xing = "碰碰胡";
        break;
    case CARD_TYPE_QI_XIAO_DUI: // 7小对
        score = 10;
        seat.hu_pai_lei_xing = "七小对";
        break;
    case CARD_TYPE_LONG_QI_DUI: //龙七对
        score = 20;
        seat.hu_pai_lei_xing = "龙七对";
        break;
    case CARD_TYPE_QING_YI_SE: // 清一色
        score = 10;
        seat.hu_pai_lei_xing = "清一色";
        break;
    case CARD_TYPE_QING_PENG: // 清一色碰碰胡
        score = 15;
        seat.hu_pai_lei_xing = "清一色碰碰胡";
        break;
    case CARD_TYPE_QING_QI_DUI: // 清一色七小对
        score = 20;
        seat.hu_pai_lei_xing = "清一色七小对";
        break;
    case CARD_TYPE_QING_LONG_QI_DUI: // 清龙七对
        score = 30;
        seat.hu_pai_lei_xing = "清龙七对";
        break;
    default:
        score = 1;
        break;
    }
    if (seats[sid].is_bao_ting == 1 && is_huang_zhuang == 0) //报听
    {
        if (score == 1)
            score = 10; 
        else
            score +=10;
    }

    if (pao_hu_seat >= 0 && seats[pao_hu_seat].is_bao_ting == 1) //杀报
    {
        sha_bao = 1;
        if (score == 1)
            score = 10; 
        else
            score +=10;
    }
    
    if (pao_hu_seat <0 && gang_hu_seat <0 )
    { //自摸
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].occupied == 0 || i == sid)
            {
                continue;
            }
            if (seats[i].is_bao_ting == 1)
            {
                sha_bao = 1;
            }
        }
    }

    // 额外番
    // 天胡
    if (deck.get_count == 1 && sid == dealer)
    {
        tian_hu_flag = 1;
        if (score == 1)
            score = 10;
        else
            score += 10;
    }

    //地胡
    else if (deck.get_count == 1 && sid != dealer)
    {
        di_hu_flag = 1;
        if (score == 1)
            score = 10;
        else
            score += 10;
    }
    else if (seat.get_next_card_cnt == 0 && seat.hole_cards.size() >= 13)
    {
        // if (chi_count == 0 && peng_count == 0 && gang_count == 0)
        // {
            di_hu_flag = 1;
            if (score == 1)
                score = 10;
            else
                score += 10;
        // }
    }
    else if (seat.get_next_card_cnt == 1 && seat.hole_cards.size() >= 13 && gang_hu_seat < 0 && pao_hu_seat < 0 && sid != dealer )
    {
        // if (chi_count == 0 && peng_count == 0 && gang_count == 0)
        // {
            di_hu_flag = 1;
            if (score == 1)
                score = 10;
            else
                score += 10;
        // }
    }


    if (seat.last_actions[0] == PLAYER_HU && seat.last_actions[1] == PLAYER_GANG)
    {
        gang_shang_hua_flag = 1;
        score += 1; //杠开,牌型分+1

        int size = seat.gang_seats.size();
        int count = seat.lian_gang_cards.size();

        if (count > 0 && count <= size)
        {
            gang_shang_hua_seat = seat.gang_seats[size - count];
        }
        else
        {
            gang_shang_hua_seat = -1;
        }

        int fan_count = seat.lian_gang_cards.size();
        for (int i = 0; i < fan_count - 1; i++)
        {
            //score *= 2;
        }

        if (fan_count > 1)
        {
            lian_gang_flag = 1;
        }
    }

    if (deck.permit_get() == 0 && pao == 0)
    {
        hai_di_hu_flag = 1;
        //score *= 2;
    }


    if (is_qiang_gang)
    {
        score += 1; //抢扛多收取一分
    }
    if (deck.permit_get() == 0 && pao == 1)
    {
        hai_di_pao = 1;

        if (score < 9 && !seat.hole_cards.men_qing_flag)
        {
            //score = 9;
        }
        else if (score >= 9 && !seat.hole_cards.men_qing_flag)
        {
            //score *= 2;
        }

        if (score < 12 && seat.hole_cards.men_qing_flag)
        {
            //score = 12;
        }
        else if (seat.hole_cards.men_qing_flag && score >= 12)
        {
            //score *= 2;
        }
    }

    if (pao == 1 && pao_hu_seat >= 0)
    {
        if (seats[pao_hu_seat].last_actions[1] == PLAYER_GANG && seats[pao_hu_seat].last_actions[0] == PLAYER_CHU)
        {
            gang_shang_pao = 1;

            if (score < 9 && !seat.hole_cards.men_qing_flag)
            {
                //score = 9;
            }
            else if (score >= 9 && !seat.hole_cards.men_qing_flag)
            {
                //score *= 2;
            }

            if (score < 12 && seat.hole_cards.men_qing_flag)
            {
                //score = 12;
            }
            else if (seat.hole_cards.men_qing_flag && score >= 12)
            {
                //score *= 2;
            }

            int fan_count = 0;

            list<int>::iterator it = seats[pao_hu_seat].actions.begin();
            it++;

            for (; it != seats[pao_hu_seat].actions.end(); it++)
            {
                if (*it == PLAYER_GANG)
                {
                    fan_count++;
                }
                else
                {
                    break;
                }
            }

            for (int i = 0; i < fan_count - 1; i++)
            {
                //score *= 2;
            }

            if (fan_count > 1)
            {
                lian_gang_pao_flag = 1;
            }
        }
    }

    if (pao == 1 && seats[pao_hu_seat].hole_cards.an_gang_count == 0 && seats[pao_hu_seat].hole_cards.cards.size() == 1)
    {
        quan_qiu_ren_pao = 1;
        if (score < 9 && !seat.hole_cards.men_qing_flag)
        {
            //score = 9;
        }
        else if (score >= 9 && !seat.hole_cards.men_qing_flag)
        {
            //score *= 2;
        }

        if (score < 12 && seat.hole_cards.men_qing_flag)
        {
            //score = 12;
        }
        else if (seat.hole_cards.men_qing_flag && score >= 12)
        {
            //score *= 2;
        }
    }

    mjlog.debug("gang_shang_hua %d, gang_shang_pao %d, quan_qiu_ren_pao %d, gang_shang_hua_seat %d\n",
                gang_shang_hua_flag, gang_shang_pao, quan_qiu_ren_pao, gang_shang_hua_seat);

    return score;
}

void Table::update_account_bet()
{
    if (already_update_account_bet == 1)
        return;  //如果之前已经调用过这个函数，则不用再次计算
    already_update_account_bet = 1;
    // 抢杠胡，那个杠不算
    if (gang_hu_seat >= 0)
    {
        Seat &seat = seats[gang_hu_seat];
        seat.gang_count[1] -= 1;

        seat.score_from_players_detail[-1][PENG_GANG_TYPE] -= PENG_GANG_BET;
        score_from_players_item_count[gang_hu_seat][PENG_GANG_TYPE]--;
        for (int j = 0; j < seat_max; j++)
        {
            if (seats[j].ready != 1 || gang_hu_seat == j)
            {
                continue;
            }
            score_to_players_item_count[j][PENG_GANG_TYPE]--;
        }
    }

    if (win_seatid < 0 && pao_hu_count < 2 && gang_hu_count < 2)
    {
        //黄庄查叫
        huang_zhuang_cha_jiao();
        return;
    }

    // 计算胡牌的牌型
    // for (int i = 0; i < seat_max; i++)
    // {
    //     if (i == win_seatid || seats[i].pao_hu_flag == 1 || seats[i].gang_hu_flag == 1)
    //     {
    //         seats[i].hole_cards.analysis_card_type();
    //         seats[i].card_type = seats[i].hole_cards.card_type;
    //     }
    // }

    if (pao_hu_seat >= 0) // 放炮胡
    {
        // 放炮算法
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].ready != 1)
            {
                continue;
            }

            if (i == win_seatid || seats[i].pao_hu_flag == 1)
            {
                int score = calculate_base_score(i, 1);
                seats[i].score = score;
                seats[i].score_from_players_detail[pao_hu_seat][PAO_HU_TYPE] = score;
                score_from_players_item_count[i][PAO_HU_TYPE]++;
                score_to_players_item_count[pao_hu_seat][PAO_HU_TYPE]++;

                if (seats[i].lian_zhuang_cnt > 1) //连庄分
                {
                    seats[i].score_from_players_detail[pao_hu_seat][LIAN_ZHUANG_TYPE] = score * (seats[i].lian_zhuang_cnt - 1);
                    score_from_players_item_count[i][LIAN_ZHUANG_TYPE] = seats[i].lian_zhuang_cnt - 1;
                    score_to_players_item_count[pao_hu_seat][LIAN_ZHUANG_TYPE] = seats[i].lian_zhuang_cnt - 1;

                }
            }
            else
            {
                if (seats[i].hole_cards.hu_cards.size() == 0)
                    seats[i].hu_pai_lei_xing = "未叫牌";
                else
                    seats[i].hu_pai_lei_xing = "叫牌";
            }
        }
    }
    else if (gang_hu_seat >= 0) //抢杠胡
    {
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].ready != 1)
            {
                continue;
            }

            if (i == win_seatid || seats[i].gang_hu_flag == 1)
            {
                int score = calculate_base_score(i, 0);
                seats[i].score = score;
                seats[i].score_from_players_detail[gang_hu_seat][GANG_HU_TYPE] = (max_ready_players - 1) * score;
                score_from_players_item_count[i][GANG_HU_TYPE]++;
                score_to_players_item_count[gang_hu_seat][GANG_HU_TYPE]++;

                if (seats[i].lian_zhuang_cnt > 1) //连庄分
                {
                    seats[i].score_from_players_detail[gang_hu_seat][LIAN_ZHUANG_TYPE] = score * (seats[i].lian_zhuang_cnt - 1);
                    score_from_players_item_count[i][LIAN_ZHUANG_TYPE] = seats[i].lian_zhuang_cnt - 1;
                    score_to_players_item_count[gang_hu_seat][LIAN_ZHUANG_TYPE] = seats[i].lian_zhuang_cnt - 1;
                }
            }
            else
            {
                if (seats[i].hole_cards.hu_cards.size() == 0)
                    seats[i].hu_pai_lei_xing = "未叫牌";
                else
                    seats[i].hu_pai_lei_xing = "叫牌";
            }
        }
    }
    else //自摸
    {
        int score = calculate_base_score(win_seatid, 0) * 2; //贵州麻将自摸，牌型分加一倍
        seats[win_seatid].score = score;
        // 自摸平湖牌型基本算法
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].ready != 1)
            {
                continue;
            }

            if (i == win_seatid)
            {
                score_from_players_item_count[i][ZI_MO_TYPE]++;
                if (gang_shang_hua_seat >= 0)
                    seats[i].score_from_players_detail[gang_shang_hua_seat][ZI_MO_TYPE] = (max_ready_players - 1) * score;
                else
                    seats[i].score_from_players_detail[-1][ZI_MO_TYPE] = (max_ready_players - 1) * score;

                if (seats[i].lian_zhuang_cnt > 1) //连庄分
                {
                    score_from_players_item_count[i][LIAN_ZHUANG_TYPE] = seats[i].lian_zhuang_cnt - 1;
                    if (gang_shang_hua_seat >= 0)
                        seats[i].score_from_players_detail[gang_shang_hua_seat][LIAN_ZHUANG_TYPE] = score * (max_ready_players - 1) * (seats[i].lian_zhuang_cnt - 1);
                    else
                        seats[i].score_from_players_detail[-1][LIAN_ZHUANG_TYPE] = score * (max_ready_players - 1) * (seats[i].lian_zhuang_cnt - 1);
                }
            }
            else
            {
                if (seats[i].hole_cards.hu_cards.size() == 0)
                    seats[i].hu_pai_lei_xing = "未叫牌";
                else
                    seats[i].hu_pai_lei_xing = "叫牌";

                // if (gang_shang_hua_seat >= 0)
                // {
                //     score_to_players_item_count[gang_shang_hua_seat][ZI_MO_TYPE] = 1;

                //     if (seats[win_seatid].lian_zhuang_cnt > 1) //连庄分
                //     {
                //         score_to_players_item_count[gang_shang_hua_seat][LIAN_ZHUANG_TYPE]++;
                //     }
                // }
                // else
                // {
                    score_to_players_item_count[i][ZI_MO_TYPE]++;

                    if (seats[win_seatid].lian_zhuang_cnt > 1) //连庄分
                    {
                        score_to_players_item_count[i][LIAN_ZHUANG_TYPE] = seats[win_seatid].lian_zhuang_cnt - 1;
                    }
                    // }
            }
        }
    }

    for (int i = 0; i < seat_max; i++)
    {
        Player *player = seats[i].player;
        if (player == NULL)
        {
            continue;
        }
        mjlog.debug("update acount base core, uid[%d], money[%d], bet[%d]\n", player->uid, player->money, seats[i].bet);
    }

    zhong_horse.clear();

    // 处理鸡牌
    if (horse_num == 0)
    {
        return;
    }

    //根据规则，将所有的鸡牌算出来
    //幺鸡固定为鸡牌
    ji_data yao_ji;
    yao_ji.value = 1;
    yao_ji.type = YAO_JI;
    horse_count++;
    ji_pai.push_back(yao_ji);
    mjlog.debug("jipai yaoji[%d]\n", yao_ji.value);

    //乌骨鸡,八筒和幺鸡一样为固定鸡；
    if (wu_gu_ji == 1)
    {
        ji_data batong; //八筒
        batong.value = 2 * 16 + 8;
        batong.type = WU_GU_JI;
        ji_pai.push_back(batong);
        horse_count++;
        mjlog.debug("jipai wu_gu_ji[%d]\n", batong.value);
    }

    for (unsigned int i = 0; i < deck.horse_cards.size(); i++) //鸡牌一般就翻一个鸡牌，这里写循环以后兼容多个
    {
        //本鸡
        if (ben_ji == 1)
        {
            ji_data ji;
            ji.value = deck.horse_cards[i].value;
            fang_ben_ji = ji.value;
            if (ji.value == 1)
            {
                ji_pai[0].type = BEN_JI;
                mjlog.debug("jipai has jin ji[yao ji]\n", ji.value);
            }
            else if (ji.value == 2 * 16 + 8 && (wu_gu_ji == 1))
            {
                fang_8_tong = 1;
                // ji_pai[1].type = BEN_JI;
                mjlog.debug("jipai has jin ji[wu gu ji]\n", ji.value);
            }
            else
            {
                ji.type = BEN_JI;
                ji_pai.push_back(ji);
                horse_count++;
                mjlog.debug("jipai benji[%d]\n", ji.value);
            }
        }

        //摇摆鸡
        ji_data ji1;
        ji1.type = YAO_BAI_JI;
        ji1.value = deck.horse_cards[i].value - 1;
        if (deck.horse_cards[i].face == 1)
        {
            ji1.value = deck.horse_cards[i].value + 8;
        }
        fang_yao_bai_ji1 = ji1.value;
        if (ji1.value == 1)
        {
            ji_pai[0].type = YAO_BAI_JI;
            mjlog.debug("jipai has jin ji[yao ji]\n", ji1.value);
        }
        else if (ji1.value == 2 * 16 + 8 && (wu_gu_ji == 1) )
        {
            fang_8_tong = 1;
            // ji_pai[1].type = YAO_BAI_JI;
            mjlog.debug("jipai has jin ji[wu gu ji]\n", ji1.value);
        }
        else
        {
            ji_pai.push_back(ji1);
            horse_count++;
            mjlog.debug("jipai yaobaiji[%d]\n", ji1.value);
        }

        ji_data ji2;
        ji2.type = YAO_BAI_JI;
        ji2.value = deck.horse_cards[i].value + 1;
        if (deck.horse_cards[i].face == 9)
        {
            ji2.value = deck.horse_cards[i].value - 8;
        }
        fang_yao_bai_ji2 = ji2.value;
        if (ji2.value == 1)
        {
            ji_pai[0].type = YAO_BAI_JI;
            mjlog.debug("jipai has jin ji[yao ji]\n", ji2.value);
        }
        else if (ji2.value == 2 * 16 + 8 && (wu_gu_ji == 1) )
        {
            fang_8_tong = 1;
            // ji_pai[1].type = YAO_BAI_JI;
            mjlog.debug("jipai has jin ji[wu gu ji]\n", ji2.value);
        }
        else
        {
            ji_pai.push_back(ji2);
            horse_count++;
            mjlog.debug("jipai yaobaiji[%d]\n", ji2.value);
        }
    }

    for (int j = 0; j < seat_max; j++)
    { //查看每个玩家的鸡牌
        if (seats[j].ready != 1)
        {
            continue;
        }
        if (is_re_pao || is_qiang_gang) //热炮，抢杠，和荒庄的情况下鸡和豆的分数全不算。
            continue;
        //热炮的玩家不能计鸡分
        if (is_re_pao && (j == chu_seat))
        {
            continue;
        }

        seats[j].hole_cards.analysis();
        if (j != win_seatid && seats[j].hole_cards.hu_cards.size() <= 0) //没有叫牌的玩家，不算鸡分
        {
            continue;
        }
        if (bao_ji != 1) //在非包鸡情况下，只有赢牌玩家才算鸡分
        {
            if (j != win_seatid)
                continue;
        }

        if ( j == win_seatid || seats[j].pao_hu_flag == 1 || seats[j].gang_hu_flag == 1)
        { //最后胡的那张牌，加入玩家的手牌
            if (pao_hu_count > 0)
            {
                seats[j].hole_cards.cards.push_back(last_card);
            }
            if (gang_hu_count > 0)
            {
                seats[j].hole_cards.cards.push_back(last_gang_card);
            }
        }
        for (int i = 0; i < horse_count; i++)
        { //循环每张鸡牌
            //查看玩家的手牌
            for (unsigned int k = 0; k < seats[j].hole_cards.cards.size(); ++k)
            {
                if (seats[j].hole_cards.cards[k] == ji_pai[i].value)
                {
                    seats[j].ji_pai.push_back(ji_pai[i]);
                    seats[j].horse_count++;
                }
            }

            //查看打出去的牌
            bool dec_chong_feng_ji = false;
            bool dec_chong_feng_wu_gu_ji = false;
            std::list<Card>::iterator ite = seats[j].hole_cards.discard_cards.begin();
            for (; ite != seats[j].hole_cards.discard_cards.end(); ++ite)
            {
                if (ite->value == ji_pai[i].value)
                {
                    if (ite->value == 1 && dec_chong_feng_ji == false && seats[j].has_chong_feng_ji == 1)
                    {
                        dec_chong_feng_ji = true;
                        continue;
                    }
                    if (ite->value == 2 * 16 + 8 && dec_chong_feng_wu_gu_ji == false && wu_gu_ji == 1 && seats[j].has_chong_feng_wu_gu_ji)
                    {
                        dec_chong_feng_wu_gu_ji = true;
                        continue;
                    }
                    seats[j].ji_pai.push_back(ji_pai[i]);
                    seats[j].horse_count++;
                }
            }
            //查看碰和杠的牌
            for (unsigned int k = 0; k < seats[j].hole_cards.obsorb_cards.size(); ++k)
            {
                for (unsigned int h = 0; h < seats[j].hole_cards.obsorb_cards[k].size(); ++h)
                {
                    if (seats[j].hole_cards.obsorb_cards[k][h] == ji_pai[i].value)
                    {
                        seats[j].ji_pai.push_back(ji_pai[i]);
                        seats[j].horse_count++;
                    }
                    if (seats[j].hole_cards.obsorb_cards[k][h] == 0) //暗杠
                    {
                        for (unsigned int h2 = 0; h2 < seats[j].hole_cards.obsorb_cards[k].size(); ++h2)
                        { //找到暗杠的面值是否是鸡牌
                            if (seats[j].hole_cards.obsorb_cards[k][h2] == ji_pai[i].value)
                            {
                                seats[j].ji_pai.push_back(ji_pai[i]);
                                seats[j].horse_count++;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < seat_max; i++)
    { //输出鸡的日志,调试用
        if (seats[i].occupied != 1)
            continue;
        mjlog.debug("jipai seats[%d] horse_count[%d]:\n", i, seats[i].horse_count);
        for (int j = 0; j < seats[i].horse_count; ++j)
        {
            mjlog.debug("---ji_pai%d_value[%d]_type[%d]\n", j, seats[i].ji_pai[j].value, seats[i].ji_pai[j].type);
        }
    }

    for (int i = 0; i < seat_max; i++)
    { //根据玩家手中的鸡牌，算bet值
        if (seats[i].ready != 1)
        {
            continue;
        }
        if (is_re_pao || is_qiang_gang) //热炮，抢杠，和荒庄的情况下鸡和豆的分数全不算。
            continue;

        seats[i].hole_cards.analysis();
        if (i != win_seatid && seats[i].hole_cards.hu_cards.size() <= 0) //没有叫牌的玩家，不算鸡分
        {
            continue;
        }
        if (bao_ji != 1) //在非包鸡情况下，只有赢牌玩家才算鸡分
        {
            if (i != win_seatid)
                continue;
        }
        for (int j = 0; j < seats[i].horse_count; ++j)
        {

            if (seats[i].ji_pai[j].value == 1)
            {
                if (seats[i].ji_pai[j].type == BEN_JI || seats[i].ji_pai[j].type == YAO_BAI_JI)
                {
                    seats[i].ji_pai[j].type = JIN_JI;
                    seats[i].has_jin_ji = 1;
                    mjlog.debug("jipai seats[%d] you jin ji.\n", i);
                }
            }
            switch (seats[i].ji_pai[j].type)
            {
            case YAO_BAI_JI:
                score_from_players_item_count[i][YAO_BAI_JI_TYPE]++;
                break;
            case BEN_JI:
                score_from_players_item_count[i][BEN_JI_TYPE]++;
                break;
            case WU_GU_JI:
                score_from_players_item_count[i][WU_GU_JI_TYPE]++;
                break;
            case YAO_JI:
                score_from_players_item_count[i][YAO_JI_TYPE]++;
                break;
            case JIN_JI:
                score_from_players_item_count[i][JIN_JI_TYPE]++;
                break;
            case BAO_JI:
                score_from_players_item_count[i][BAO_JI]++;
                break;
            default:
                break;
            }
            for (int k = 0; k < seat_max; k++)
            {
                if (seats[k].ready != 1 || i == k)
                {
                    continue;
                }
                switch (seats[i].ji_pai[j].type)
                {
                case YAO_BAI_JI:
                    seats[i].score_from_players_detail[k][YAO_BAI_JI_TYPE] += YAO_BAI_JI_BET;
                    score_to_players_item_count[k][YAO_BAI_JI_TYPE]++;
                    mjlog.debug("YAO BAI JI seatsid[%d] win %d seatsid[%d] lose\n", i, YAO_BAI_JI_BET, k);
                    break;
                case BEN_JI:
                    seats[i].score_from_players_detail[k][BEN_JI_TYPE] += BEN_JI_BET;
                    score_to_players_item_count[k][BEN_JI_TYPE]++;
                    mjlog.debug("BEN JI seatsid[%d] win %d seatsid[%d] lose\n", i, BEN_JI_BET, k);
                    break;
                case WU_GU_JI:
                    seats[i].score_from_players_detail[k][WU_GU_JI_TYPE] += WU_GU_JI_BET * (fang_8_tong + 1);
                    score_to_players_item_count[k][WU_GU_JI_TYPE]++;
                    mjlog.debug("WU_GU_JI seatsid[%d] win %d seatsid[%d] lose\n", i, WU_GU_JI_BET * (fang_8_tong + 1), k);
                    break;
                case YAO_JI:
                    seats[i].score_from_players_detail[k][YAO_JI_TYPE] += YAO_JI_BET;
                    score_to_players_item_count[k][YAO_JI_TYPE]++;
                    mjlog.debug("YAO JI seatsid[%d] win %d seatsid[%d] lose\n", i, YAO_JI_BET, k);
                    break;
                case JIN_JI:
                    seats[i].score_from_players_detail[k][JIN_JI_TYPE] += JIN_JI_BET;
                    score_to_players_item_count[k][JIN_JI_TYPE]++;
                    mjlog.debug("JIN JI seatsid[%d] win %d seatsid[%d] lose\n", i, JIN_JI_BET, k);
                    break;
                case BAO_JI:
                    seats[i].score_from_players_detail[k][BAO_JI_TYPE] += BAO_JI_BET;
                    score_to_players_item_count[k][BAO_JI]++;
                    mjlog.debug("BAO JI seatsid[%d] win %d seatsid[%d] lose\n", i, BAO_JI_BET, k);
                    break;
                default:
                    break;
                }
            }
        }

        if (bao_ji == 1 || win_seatid == i) //算冲锋鸡，冲锋乌骨鸡，责任鸡
        {
            int c = 1;
            if (i != win_seatid && seats[i].gang_hu_flag != 1 && seats[i].pao_hu_flag != 1 && seats[i].hole_cards.hu_cards.size() == 0) //听牌玩家也算鸡分
            {
                c = -1; //给其它玩家分
            }
            for (int j = 0; j < seat_max; j++)
            {
                if (i == j)
                    continue;
                if (seats[j].ready != 1)
                {
                    continue;
                }
                if (seats[i].has_chong_feng_ji == 1)
                { //c为1时,拿到冲锋鸡的获取其它玩家的分
                    if (seats[i].has_jin_ji == 1)
                    { //有金鸡时翻倍
                        if (c == 1)
                        {
                            seats[i].score_from_players_detail[j][CHONG_FENG_JI_TYPE] = 4;
                            score_from_players_item_count[i][CHONG_FENG_JI_TYPE] = 1;
                            score_to_players_item_count[j][CHONG_FENG_JI_TYPE]++;
                        }
                        else
                        {
                            seats[j].score_from_players_detail[i][CHONG_FENG_JI_TYPE] = 4;
                            score_from_players_item_count[j][CHONG_FENG_JI_TYPE] = 1;
                            score_to_players_item_count[i][CHONG_FENG_JI_TYPE]++;
                        }
                        mjlog.debug("jipai seats[%d] you chong feng ji fen bei.\n", i);
                    }
                    else
                    {
                        if (c == 1)
                        {
                            seats[i].score_from_players_detail[j][CHONG_FENG_JI_TYPE] = 2;
                            score_from_players_item_count[i][CHONG_FENG_JI_TYPE] = 1;
                            score_to_players_item_count[j][CHONG_FENG_JI_TYPE]++;
                        }
                        else
                        {
                            seats[j].score_from_players_detail[i][CHONG_FENG_JI_TYPE] = 2;
                            score_from_players_item_count[j][CHONG_FENG_JI_TYPE] = 1;
                            score_to_players_item_count[i][CHONG_FENG_JI_TYPE]++;
                        }
                        mjlog.debug("jipai seats[%d] you chong feng ji.\n", i);
                    }
                }
                if (seats[i].has_chong_feng_wu_gu_ji == 1)
                { //c为1时,拿到冲锋乌骨鸡的获取给其它玩家分
                    if (c == 1)
                    {
                        seats[i].score_from_players_detail[j][CHONG_FENG_WU_GU_JI_TYPE] = 2;
                        score_from_players_item_count[i][CHONG_FENG_WU_GU_JI_TYPE] = 1;
                        score_to_players_item_count[j][CHONG_FENG_WU_GU_JI_TYPE]++;
                    }
                    else
                    {
                        seats[j].score_from_players_detail[i][CHONG_FENG_WU_GU_JI_TYPE] = 2;
                        score_from_players_item_count[j][CHONG_FENG_WU_GU_JI_TYPE] = 1;
                        score_to_players_item_count[i][CHONG_FENG_WU_GU_JI_TYPE]++;
                    }
                    mjlog.debug("jipai seats[%d] you chong feng wu gu ji.\n", i);
                }
                if (seats[i].has_ze_ren_ji == 1)
                {
                    seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] = 1; //不管输赢，有责任鸡都输出1分
                    score_from_players_item_count[j][ZE_REN_JI_TYPE] = 1;
                    score_to_players_item_count[i][ZE_REN_JI_TYPE]++;
                    mjlog.debug("jipai seats[%d] you ze ren ji.\n", i);
                }
                if (seats[i].has_wu_gu_ze_ren_ji == 1)
                {
                    if (seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] == 1)
                        seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] = 2; //不管输赢，有责任鸡都输出1分
                    else
                        seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] = 1;
                    score_from_players_item_count[j][ZE_REN_JI_TYPE] = 1;
                    if (seats[i].has_ze_ren_ji == 1)
                        score_from_players_item_count[j][ZE_REN_JI_TYPE] = 2;
                    score_to_players_item_count[i][ZE_REN_JI_TYPE]++;
                    mjlog.debug("jipai seats[%d] you wu gu ze ren ji.\n", i);
                }
            }
        }
    }

    if (bao_ji && !is_re_pao && !is_qiang_gang)
    { //在包鸡的情况下，没有叫牌和胡牌的玩家，打出去的冲锋鸡，冲锋乌骨鸡，幺鸡，乌骨鸡要出分
        for (int i = 0; i < seat_max; i++)
        { //根据玩家手中的鸡牌，算bet值
            if (seats[i].ready != 1)
            {
                continue;
            }
            seats[i].hole_cards.analysis();
            if (i == win_seatid || seats[i].hole_cards.hu_cards.size() > 0) //胡牌和叫牌的玩家，不要出分
            {
                continue;
            }

            for (int j = 0; j < seat_max; j++)
            {
                if (i == j)
                    continue;
                if (seats[j].ready != 1)
                {
                    continue;
                }
                if (seats[i].has_ze_ren_ji == 1)
                {
                    seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] = 1; //不管输赢，有责任鸡都输出1分
                    score_from_players_item_count[j][ZE_REN_JI_TYPE] = 1;
                    score_to_players_item_count[i][ZE_REN_JI_TYPE]++;
                    mjlog.debug("jipai seats[%d] you ze ren ji.\n", i);
                }
                if (seats[i].has_wu_gu_ze_ren_ji == 1)
                {
                    if (seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] == 1)
                        seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] = 2; //不管输赢，有责任鸡都输出1分
                    else
                        seats[j].score_from_players_detail[i][ZE_REN_JI_TYPE] = 1;
                    score_from_players_item_count[j][ZE_REN_JI_TYPE] = 1;
                    if (seats[i].has_ze_ren_ji == 1)
                        score_from_players_item_count[j][ZE_REN_JI_TYPE] = 2;
                    score_to_players_item_count[i][ZE_REN_JI_TYPE]++;
                    mjlog.debug("jipai seats[%d] you wu gu ze ren ji.\n", i);
                }
                if (j != win_seatid && seats[j].hole_cards.hu_cards.size() <= 0) //胡牌和叫牌的玩家，进分
                {
                    continue;
                }
                if (seats[i].has_chong_feng_ji == 1)
                { //c为1时,拿到冲锋鸡的获取其它玩家的分
                    if (seats[i].has_jin_ji == 1)
                    { //有金鸡时翻倍
                        seats[j].score_from_players_detail[i][CHONG_FENG_JI_TYPE] = 4;
                        score_from_players_item_count[j][CHONG_FENG_JI_TYPE] = 1;
                        score_to_players_item_count[i][CHONG_FENG_JI_TYPE]++;
                        mjlog.debug("jipai seats[%d] you chong feng ji fen bei.\n", i);
                    }
                    else
                    {
                        seats[j].score_from_players_detail[i][CHONG_FENG_JI_TYPE] = 2;
                        score_from_players_item_count[j][CHONG_FENG_JI_TYPE] = 1;
                        score_to_players_item_count[i][CHONG_FENG_JI_TYPE]++;
                        mjlog.debug("jipai seats[%d] you chong feng ji.\n", i);
                    }
                }
                if (seats[i].has_chong_feng_wu_gu_ji == 1)
                { //c为1时,拿到冲锋乌骨鸡的获取给其它玩家分
                    seats[j].score_from_players_detail[i][CHONG_FENG_WU_GU_JI_TYPE] = 2;
                    score_from_players_item_count[j][CHONG_FENG_WU_GU_JI_TYPE] = 1;
                    score_to_players_item_count[i][CHONG_FENG_WU_GU_JI_TYPE]++;
                    mjlog.debug("jipai seats[%d] you chong feng wu gu ji.\n", i);
                }
                if (seats[i].has_yao_ji >= 1)
                {
                    seats[j].score_from_players_detail[i][YAO_JI_TYPE] += seats[i].has_yao_ji;
                    score_from_players_item_count[j][YAO_JI_TYPE] += seats[i].has_yao_ji;
                    score_to_players_item_count[i][YAO_JI_TYPE] += seats[i].has_yao_ji;
                    mjlog.debug("jipai seats[%d] you yao ji cnt[%d].\n", i, seats[i].has_yao_ji);
                }
                if (seats[i].has_wu_gu_ji >= 1)
                {
                    seats[j].score_from_players_detail[i][WU_GU_JI_TYPE] += seats[i].has_wu_gu_ji;
                    score_from_players_item_count[j][WU_GU_JI_TYPE] += seats[i].has_wu_gu_ji;
                    score_to_players_item_count[i][WU_GU_JI_TYPE] += seats[i].has_wu_gu_ji;
                    mjlog.debug("jipai seats[%d] you wu gu ji cnt[%d].\n", i, seats[i].has_wu_gu_ji);
                }
            }
        }
    }

    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].ready != 1)
        {
            continue;
        }
        mjlog.debug("seatid[%d]------------------------------------\n", i);
        std::map<int, std::map<int, int> >::iterator ite = seats[i].score_from_players_detail.begin();
        for (; ite != seats[i].score_from_players_detail.end(); ite++)
        {
            std::map<int, int> &secondValue = ite->second;
            int firstValue = ite->first;

            std::map<int, int>::iterator inner_ite = secondValue.begin();
            for (; inner_ite != secondValue.end(); inner_ite++)
            {
                mjlog.debug("#chu_fen_seatid[%d]  chu_fen_type[%d]  chu_fen[%d]\n", firstValue, inner_ite->first, inner_ite->second);
            }
        }
        mjlog.debug("seatid[%d]------------------------------------\n", i);
    }

    //统计各个玩家每项的进分和出分。
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].ready != 1)
        {
            continue;
        }
        std::map<int, std::map<int, int> >::iterator ite = seats[i].score_from_players_detail.begin();
        for (; ite != seats[i].score_from_players_detail.end(); ite++)
        {
            std::map<int, int> &secondValue = ite->second;
            int firstValue = ite->first;
            //胡
            if (secondValue.find(PAO_HU_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][PAO_HU_TYPE] += secondValue[PAO_HU_TYPE];

                score_to_players_item_total[firstValue][PAO_HU_TYPE] += secondValue[PAO_HU_TYPE];
                mjlog.debug("score item pao_hu_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][PAO_HU_TYPE], score_from_players_item_count[i][PAO_HU_TYPE],
                            score_to_players_item_total[firstValue][PAO_HU_TYPE], score_to_players_item_count[firstValue][PAO_HU_TYPE]);
            }
            if (secondValue.find(GANG_HU_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][GANG_HU_TYPE] += secondValue[GANG_HU_TYPE];

                score_to_players_item_total[firstValue][GANG_HU_TYPE] = secondValue[GANG_HU_TYPE];
                mjlog.debug("score item gang_hu_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][GANG_HU_TYPE], score_from_players_item_count[i][GANG_HU_TYPE],
                            score_to_players_item_total[firstValue][GANG_HU_TYPE], score_to_players_item_count[firstValue][GANG_HU_TYPE]);
            }
            if (secondValue.find(ZI_MO_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][ZI_MO_TYPE] += secondValue[ZI_MO_TYPE];

                mjlog.debug("score item zi_mo_type# from[%d] cnt[%d]\n", score_from_players_item_total[i][ZI_MO_TYPE], score_from_players_item_count[i][ZI_MO_TYPE]);
                for (int j = 0; j < seat_max; j++)
                {
                    if (seats[j].ready != 1 || i == j)
                    {
                        continue;
                    }
                    score_to_players_item_total[j][ZI_MO_TYPE] += secondValue[ZI_MO_TYPE] / (max_ready_players - 1);
                    if (seats[j].is_bao_ting == 1)
                    {//报杀玩家多出分
                        if (seats[i].card_type == CARD_TYPE_PING_HU && tian_hu_flag == 0 && di_hu_flag == 0)
                        {
                            score_to_players_item_total[j][ZI_MO_TYPE] += 18;
                            score_from_players_item_total[i][ZI_MO_TYPE] += 18;
                        }
                        else
                        {
                            score_to_players_item_total[j][ZI_MO_TYPE] += 20;
                            score_from_players_item_total[i][ZI_MO_TYPE] += 20;
                        }
                    }
                    mjlog.debug("score item zi_mo_type# to[%d] cnt[%d] card_type[%d]\n", score_to_players_item_total[j][ZI_MO_TYPE], score_to_players_item_count[j][ZI_MO_TYPE], seats[j].card_type);
                }
            }
            //连庄
            if (secondValue.find(LIAN_ZHUANG_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][LIAN_ZHUANG_TYPE] += secondValue[LIAN_ZHUANG_TYPE];
                mjlog.debug("score item lian_zhuang_type# from[%d] cnt[%d]\n", score_from_players_item_total[i][LIAN_ZHUANG_TYPE], score_from_players_item_count[i][LIAN_ZHUANG_TYPE]);

                if (pao_hu_seat >= 0)
                    score_to_players_item_total[firstValue][LIAN_ZHUANG_TYPE] += secondValue[LIAN_ZHUANG_TYPE];
                else if (gang_hu_seat >= 0)
                    score_to_players_item_total[firstValue][LIAN_ZHUANG_TYPE] += secondValue[LIAN_ZHUANG_TYPE];
                else
					for (int j = 0; j < seat_max; j++)
					{
						if (seats[j].ready != 1 || i == j)
						{
							continue;
						}
                        score_to_players_item_total[j][LIAN_ZHUANG_TYPE] += secondValue[LIAN_ZHUANG_TYPE] / (max_ready_players - 1);
                        if (seats[j].is_bao_ting == 1)
                        {//报杀玩家多出分
                            if (seats[i].card_type == CARD_TYPE_PING_HU && tian_hu_flag == 0 && di_hu_flag == 0)
                            {
                                score_to_players_item_total[j][ZI_MO_TYPE] += 18;
                                score_from_players_item_total[i][ZI_MO_TYPE] += 18;
                            }
                            else
                            {
                                score_to_players_item_total[j][ZI_MO_TYPE] += 20;
                                score_from_players_item_total[i][ZI_MO_TYPE] += 20;
                            }
                        }
						mjlog.debug("score item lian_zhuang_type# to[%d] cnt[%d] \n", score_to_players_item_total[j][LIAN_ZHUANG_TYPE], score_to_players_item_count[j][LIAN_ZHUANG_TYPE]);
					}
            }

            //鸡
            if (secondValue.find(YAO_BAI_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][YAO_BAI_JI_TYPE] += secondValue[YAO_BAI_JI_TYPE];
                score_to_players_item_total[firstValue][YAO_BAI_JI_TYPE] += secondValue[YAO_BAI_JI_TYPE];
                mjlog.debug("score item yao_bai_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][YAO_BAI_JI_TYPE], score_from_players_item_count[i][YAO_BAI_JI_TYPE],
                            score_to_players_item_total[firstValue][YAO_BAI_JI_TYPE], score_to_players_item_count[firstValue][YAO_BAI_JI_TYPE]);
            }
            if (secondValue.find(BEN_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][BEN_JI_TYPE] += secondValue[BEN_JI_TYPE];
                score_to_players_item_total[firstValue][BEN_JI_TYPE] += secondValue[BEN_JI_TYPE];
                mjlog.debug("score item ben_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][BEN_JI_TYPE], score_from_players_item_count[i][BEN_JI_TYPE],
                            score_to_players_item_total[firstValue][BEN_JI_TYPE], score_to_players_item_count[firstValue][BEN_JI_TYPE]);
            }
            if (secondValue.find(WU_GU_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][WU_GU_JI_TYPE] += secondValue[WU_GU_JI_TYPE];
                score_to_players_item_total[firstValue][WU_GU_JI_TYPE] += secondValue[WU_GU_JI_TYPE];
                mjlog.debug("score item wu_gu_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][WU_GU_JI_TYPE], score_from_players_item_count[i][WU_GU_JI_TYPE],
                            score_to_players_item_total[firstValue][WU_GU_JI_TYPE], score_to_players_item_count[firstValue][WU_GU_JI_TYPE]);
            }
            if (secondValue.find(CHONG_FENG_WU_GU_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][CHONG_FENG_WU_GU_JI_TYPE] += secondValue[CHONG_FENG_WU_GU_JI_TYPE];
                score_to_players_item_total[firstValue][CHONG_FENG_WU_GU_JI_TYPE] += secondValue[CHONG_FENG_WU_GU_JI_TYPE];

                mjlog.debug("score item chong_feng_wu_gu_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][CHONG_FENG_WU_GU_JI_TYPE], score_from_players_item_count[i][CHONG_FENG_WU_GU_JI_TYPE],
                            score_to_players_item_total[firstValue][CHONG_FENG_WU_GU_JI_TYPE], score_to_players_item_count[firstValue][CHONG_FENG_WU_GU_JI_TYPE]);
            }
            if (secondValue.find(YAO_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][YAO_JI_TYPE] += secondValue[YAO_JI_TYPE];
                score_to_players_item_total[firstValue][YAO_JI_TYPE] += secondValue[YAO_JI_TYPE];
                mjlog.debug("score item yao_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][YAO_JI_TYPE], score_from_players_item_count[i][YAO_JI_TYPE],
                            score_to_players_item_total[firstValue][YAO_JI_TYPE], score_to_players_item_count[firstValue][YAO_JI_TYPE]);
            }
            if (secondValue.find(CHONG_FENG_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][CHONG_FENG_JI_TYPE] += secondValue[CHONG_FENG_JI_TYPE];
                score_to_players_item_total[firstValue][CHONG_FENG_JI_TYPE] += secondValue[CHONG_FENG_JI_TYPE];
                mjlog.debug("score item chong_feng_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][CHONG_FENG_JI_TYPE], score_from_players_item_count[i][CHONG_FENG_JI_TYPE],
                            score_to_players_item_total[firstValue][CHONG_FENG_JI_TYPE], score_to_players_item_count[firstValue][CHONG_FENG_JI_TYPE]);
            }
            if (secondValue.find(ZE_REN_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][ZE_REN_JI_TYPE] += secondValue[ZE_REN_JI_TYPE];
                score_to_players_item_total[firstValue][ZE_REN_JI_TYPE] += secondValue[ZE_REN_JI_TYPE];
                mjlog.debug("score item ze_ren_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][ZE_REN_JI_TYPE], score_from_players_item_count[i][ZE_REN_JI_TYPE],
                            score_to_players_item_total[firstValue][ZE_REN_JI_TYPE], score_to_players_item_count[firstValue][ZE_REN_JI_TYPE]);
            }
            if (secondValue.find(JIN_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][JIN_JI_TYPE] += secondValue[JIN_JI_TYPE];
                score_to_players_item_total[firstValue][JIN_JI_TYPE] += secondValue[JIN_JI_TYPE];
                mjlog.debug("score item jin_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][JIN_JI_TYPE], score_from_players_item_count[i][JIN_JI_TYPE],
                            score_to_players_item_total[firstValue][JIN_JI_TYPE], score_to_players_item_count[firstValue][JIN_JI_TYPE]);
            }
            if (secondValue.find(BAO_JI_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][BAO_JI_TYPE] += secondValue[BAO_JI_TYPE];
                score_to_players_item_total[firstValue][BAO_JI_TYPE] += secondValue[BAO_JI_TYPE];
                mjlog.debug("score item bao_ji_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][BAO_JI_TYPE], score_from_players_item_count[i][BAO_JI_TYPE],
                            score_to_players_item_total[firstValue][BAO_JI_TYPE], score_to_players_item_count[firstValue][BAO_JI_TYPE]);
            }

            //杠
            if (is_re_pao || is_qiang_gang) //热炮，抢杠，和荒庄的情况下鸡和豆的分数全不算。
                continue;
            if (secondValue.find(MING_GANG_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][MING_GANG_TYPE] += secondValue[MING_GANG_TYPE];
                score_to_players_item_total[firstValue][MING_GANG_TYPE] += secondValue[MING_GANG_TYPE];
                score_from_players_item_count[i][MING_GANG_TYPE] += secondValue[MING_GANG_TYPE] / ((max_ready_players - 1) * MING_GANG_BET);
                score_to_players_item_count[firstValue][MING_GANG_TYPE] += secondValue[MING_GANG_TYPE] / ((max_ready_players - 1) * MING_GANG_BET);
                mjlog.debug("score item ming_gang_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][MING_GANG_TYPE], score_from_players_item_count[i][MING_GANG_TYPE],
                            score_to_players_item_total[firstValue][MING_GANG_TYPE], score_to_players_item_count[firstValue][MING_GANG_TYPE]);
            }
            if (secondValue.find(PENG_GANG_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][PENG_GANG_TYPE] += (max_ready_players - 1) * secondValue[PENG_GANG_TYPE];
                score_from_players_item_count[i][PENG_GANG_TYPE] += secondValue[PENG_GANG_TYPE] / ( PENG_GANG_BET );
                mjlog.debug("score item peng_gang_type# from[%d] cnt[%d]\n", score_from_players_item_total[i][PENG_GANG_TYPE], score_from_players_item_count[i][PENG_GANG_TYPE]);

                for (int j = 0; j < seat_max; j++)
                {
                    if (seats[j].ready != 1 || i == j)
                    {
                        continue;
                    }
                    score_to_players_item_total[j][PENG_GANG_TYPE] += secondValue[PENG_GANG_TYPE];
                    score_to_players_item_count[j][PENG_GANG_TYPE] += secondValue[PENG_GANG_TYPE] / ( PENG_GANG_BET );
                    mjlog.debug("score item peng_gang_type# to[%d] cnt[%d] \n", score_to_players_item_total[j][PENG_GANG_TYPE], score_to_players_item_count[j][PENG_GANG_TYPE]);
                }
            }
            if (secondValue.find(AN_GANG_TYPE) != secondValue.end())
            {
                score_from_players_item_total[i][AN_GANG_TYPE] += (max_ready_players - 1) * secondValue[AN_GANG_TYPE];
                score_from_players_item_count[i][AN_GANG_TYPE] += secondValue[AN_GANG_TYPE] / ( AN_GANG_BET );
                mjlog.debug("score item an_gang_type# from[%d] cnt[%d]\n", score_from_players_item_total[i][AN_GANG_TYPE], score_from_players_item_count[i][AN_GANG_TYPE]);

                for (int j = 0; j < seat_max; j++)
                {
                    if (seats[j].ready != 1 || i == j)
                    {
                        continue;
                    }
                    score_to_players_item_total[j][AN_GANG_TYPE] += secondValue[AN_GANG_TYPE];
                    score_to_players_item_count[j][AN_GANG_TYPE] += secondValue[AN_GANG_TYPE] / (AN_GANG_BET);
                    mjlog.debug("score item an_gang_type# to[%d] cnt[%d] \n", score_to_players_item_total[j][AN_GANG_TYPE], score_to_players_item_count[j][AN_GANG_TYPE]);
                }
            }
        }
        mjlog.debug("upper player seatid[%d]------------------------------------------------\n", i);
    }

    //统计各个玩家每项的总分。
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].ready != 1)
        {
            continue;
        }
        std::map<int, int>::iterator ite = score_from_players_item_total[i].begin();
        for (; ite != score_from_players_item_total[i].end(); ite++)
        {
            score_from_players_total[i] += ite->second;
            mjlog.debug("seatid[%d] fen_type[%d] jin_fen[%d]\n", i, ite->first, ite->second);
        }
        std::map<int, int>::iterator ite2 = score_to_players_item_total[i].begin();
        for (; ite2 != score_to_players_item_total[i].end(); ite2++)
        {
            score_from_players_total[i] -= ite2->second;
            mjlog.debug("seatid[%d] fen_type[%d] chu_fen[%d]\n", i, ite2->first, ite2->second);
        }
        mjlog.debug("-----------------------------------------------\n");
        seats[i].bet = score_from_players_total[i];
    }

    //统计大结算时中鸡数，冲锋鸡数和责任鸡数
    for (int i=0; i<seat_max; ++i)
    {
        if (seats[i].ready != 1)
            continue;
        map<int, int>::iterator it = score_to_players_item_count[i].begin();
        for (; it != score_to_players_item_count[i].end(); ++it)
        {
            if (it->first == BEN_JI_TYPE || it->first == YAO_BAI_JI_TYPE || it->first == WU_GU_JI_TYPE || it->first == YAO_JI_TYPE)
                seats[i].total_zhong_ji++;

            if (it->first == CHONG_FENG_WU_GU_JI_TYPE || it->first == CHONG_FENG_JI_TYPE)
                seats[i].total_chong_feng_ji++;

            if (it->first == ZE_REN_JI_TYPE)
                seats[i].total_ze_ren_ji++;
        }
    }
    
    for (int i = 0; i < seat_max; i++)
    {
        Player *player = seats[i].player;
        if (player == NULL)
        {
            continue;
        }
        mjlog.debug("update acount horse, uid[%d], money[%d], bet[%d]\n", player->uid, player->money, seats[i].bet);
    }
}

// action: 0 为发牌状态，1为吃牌，2为碰牌，3为杠牌，4为摸牌
void Table::dump_hole_cards(std::vector<Card> &cards, int seatid, int action)
{
    char buff[256] = {
        0,
    };
    const char *action_desc[] = {"发牌", "吃", "碰", "杠", "摸牌", "出牌"};

    std::vector<Card>::iterator it = cards.begin();
    for (; it != cards.end(); it++)
    {
        int len = strlen(buff);
        snprintf(buff + len, 256 - len, "%s ", it->get_card().c_str());
    }

    mjlog.debug("dumpcards table[%d] uid[%d] seat[%d] action[%s]\n", tid,
                seats[seatid].player->uid, seatid, action_desc[action]);
    mjlog.debug("dumpcards: %s\n", buff);
}

void Table::check_ip_conflict()
{
    if (static_cast<int>(players.size()) == seat_max && round_count == 0)
    {
        std::map<string, int> ip_count;
        for (int i = 0; i < seat_max; i++)
        {
            string ip = seats[i].player->remote_ip;
            if (ip_count.find(ip) == ip_count.end())
            {
                ip_count[ip] = 1;
            }
            else
            {
                ip_count[ip] += 1;
            }
        }

        if (static_cast<int>(ip_count.size()) != seat_max)
        {
            Jpacket packet;
            packet.val["cmd"] = SERVER_IP_CONFLICT;
            std::map<string, int>::iterator it;
            int j = 0;
            for (it = ip_count.begin(); it != ip_count.end(); it++)
            {
                if (it->second >= 2)
                {
                    for (int i = 0; i < seat_max; i++)
                    {
                        string ip = seats[i].player->remote_ip;
                        if (ip == it->first)
                        {
                            packet.val["sam_ip"][j].append(seats[i].player->name);
                        }
                    }
                    j++;
                }
            }

            packet.end();
            broadcast(NULL, packet.tostring());
        }
    }
}

void Table::broadcast_forbid_hu(Player *player, int num)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_FORBID_HU_BC;
    packet.val["name"] = player->name;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.val["count"] = num;
    packet.end();

    broadcast(NULL, packet.tostring());
}

void Table::handler_gps_notice(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    float jingdu = val["jingdu"].asFloat();
    float weidu = val["weidu"].asFloat();

    Seat &seat = seats[player->seatid];
    seat.jingdu = jingdu;
    seat.weidu = weidu;

    player->jin_du = jingdu;
    player->wei_du = weidu;

    Jpacket packet;
    packet.val["cmd"] = SERVER_GPS_POS_NOTICE_SUCC_UC;
    packet.val["name"] = player->name;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.end();

    unicast(player, packet.tostring());
}

void Table::handler_gps_dist_req(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    int seatid = val["seatid"].asInt();
    std::set<int> seatids;

    Jpacket packet;
    packet.val["cmd"] = SERVER_GPS_DIS_SUCC_UC;
    packet.val["seatid"] = seatid;

    for (int i = 0; i < 4; i++)
    {
        int next = (seatid + i) % 4;
        if (seats[next].jingdu < 1.0f || seats[next].weidu < 1.0f)
        {
            packet.val["ndseatid"].append(next);
            seatids.insert(next);
        }
    }

    for (int i = 1; i < 4; i++)
    {
        int next = (seatid + i) % 4;
        if (seatids.find(next) == seatids.end() && seatids.find(seatid) == seatids.end())
        {
            double distance = GetDistance(seats[seatid].jingdu, seats[seatid].weidu, seats[next].jingdu, seats[next].weidu);
            int idis = (int)distance;
            packet.val["distances"].append(idis);
        }
        else
        {
            packet.val["distances"].append(-1);
        }
    }
    packet.end();

    broadcast(NULL, packet.tostring());
}

double Table::GetDistance(double dLongitude1, double dLatitude1, double dLongitude2, double dLatitude2)
{
    double lat1 = (M_PI / 180) * dLatitude1;
    double lat2 = (M_PI / 180) * dLatitude2;

    double lon1 = (M_PI / 180) * dLongitude1;
    double lon2 = (M_PI / 180) * dLongitude2;

    //地球半径
    double R = 6378.137;

    //两点间距离 km，如果想要米的话，结果*1000就可以了
    double d = acos(sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(lon2 - lon1)) * R;

    return d * 1000;
}

void Table::handler_voice_req(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    string url = val["url"].asString();
    int record_time = val["record_time"].asInt();

    Jpacket packet;
    packet.val["cmd"] = SERVER_VOICE_SUCC_BC;
    packet.val["name"] = player->name;
    packet.val["uid"] = player->uid;
    packet.val["seatid"] = player->seatid;
    packet.val["url"] = url;
    packet.val["record_time"] = record_time;
    packet.end();

    broadcast(NULL, packet.tostring());
}

string Table::format_card_desc(int card_type, int seatid)
{
    string desc;
    string str = "";
    string str_special = "";

    switch (card_type)
    {
    case CARD_TYPE_PING_HU: // 平胡
        str = "平胡";
        break;
    case CARD_TYPE_PENG_HU: // 碰碰胡
        str = "碰碰胡";
        break;
    case CARD_TYPE_QI_XIAO_DUI: // 7小对
        str = "七小对";
        break;
    case CARD_TYPE_LONG_QI_DUI: //龙七对
        str = "龙七对";
        break;
    case CARD_TYPE_QING_YI_SE: // 清一色
        str = "清一色";
        break;
    case CARD_TYPE_QING_PENG: // 清一色碰碰胡
        str = "清一色碰碰胡";
        break;
    case CARD_TYPE_QING_QI_DUI: // 清一色七小对
        str = "清一色七小对";
        break;
    case CARD_TYPE_QING_LONG_QI_DUI: // 清龙七对
        str = "清龙七对";
        break;
    default:
        break;
    }

    if (card_type != 0)
    {
        if (tian_hu_flag == 1)
        {
            str_special = "天胡, ";
        }
        else if (di_hu_flag == 1)
        {
            str_special = "地胡, ";
        }
        else if (hai_di_hu_flag == 1)
        {
            str_special = "海底胡, ";
        }
        else if (gang_shang_hua_flag == 1)
        {
            str_special = "杠上花, ";
        }
        else if (gang_shang_pao == 1)
        {
            str_special = "杠上炮, ";
        }
        else if (quan_qiu_ren_pao == 1)
        {
            str_special = "全求人炮, ";
        }
        else if (hai_di_pao == 1)
        {
            str_special = "海底炮, ";
        }

        if (lian_gang_flag == 1)
        {
            str_special += "连杠杠上花, ";
        }

        if (lian_gang_pao_flag == 1)
        {
            str_special += "连杠杠上炮, ";
        }

        if (seats[seatid].is_bao_ting)
        {
            str_special += "报听, ";
        }
        if (sha_bao)
        {
            str_special += "杀报, ";
        }
        // if (is_re_pao)
        // {
        //     str_special += "热炮, ";
        // }
        // if (is_qiang_gang)
        // {
        //     str_special += "抢杠, ";
        // }
    }

    if (seatid >= 0 && seatid < seat_max)
    {
        if (seats[seatid].hole_cards.men_qing_flag && (card_type != CARD_TYPE_PING_HU || (card_type == CARD_TYPE_PING_HU && seats[seatid].hole_cards.cards.size() % 3 == 2)))
        {
            // str_special += "门清, ";
        }
    }

    desc = str_special + str;
    return desc;
}

//提前开始的申请处理
void Table::handler_start_game_req(Player *player)
{
    Json::Value &val = player->client->packet.tojson();

    Seat &seat = seats[player->seatid];
    int flag = val["flag"].asInt();

    if (state != ROOM_WAIT_GAME)
    {
        return;
    }

    if (players.size() == 1 || static_cast<int>(players.size()) == seat_max)
    {
        return;
    }

    seat.ahead_start = (flag == 0 ? 0 : 1);

    if (ahead_start_flag == 0 && flag == 1)
    {
        ahead_start_flag = 1;
        ahead_start_uid = player->uid;
        seat.ahead_start = flag;
        if (flag == 1)
        {
            ev_timer_again(zjh.loop, &ahead_start_timer);
            broadcast_ahead_start_status(player, 1);
        }
    }
    else if (ahead_start_flag == 0 && flag == 0)
    {
        seat.ahead_start = -1;
    }
    else
    {
        if (flag == 0)
        {
            seat.ahead_start = 0;
            ev_timer_stop(zjh.loop, &ahead_start_timer);
            broadcast_ahead_start_status(player, 0);

            ahead_start_flag = 0;
            ahead_start_uid = -1;

            for (int i = 0; i < seat_max; i++)
            {
                seats[i].ahead_start = -1;
            }
        }
        else
        {
            int total = 0;
            int count = 0;
            for (int i = 0; i < seat_max; i++)
            {
                if (seats[i].player != NULL)
                {
                    total++;
                    if (seats[i].ahead_start == 1)
                    {
                        count++;
                    }
                }
            }

            broadcast_ahead_start_status(player, 1);

            if (total == count)
            {
                ahead_start_flag = 0;
                ahead_start_uid = -1;

                for (int i = 0; i < seat_max; i++)
                {
                    seats[i].ahead_start = -1;
                }

                ev_timer_stop(zjh.loop, &ahead_start_timer);
                max_ready_players = std::max(2, total);

                test_game_start();
            }
        }
    }
}

//0：不同意提前开始 1：谁申请开始，同意开始
void Table::broadcast_ahead_start_status(Player *player, int flag)
{
    Jpacket packet;
    packet.val["cmd"] = SERVER_START_GAME_REQ_BC;

    if (player != NULL)
    {
        packet.val["name"] = player->name;
        packet.val["uid"] = player->uid;
        packet.val["seatid"] = player->seatid;
    }

    packet.val["ahead_start_flag"] = ahead_start_flag;
    packet.val["ahead_start_uid"] = ahead_start_uid;

    if (flag == 1)
    {
        packet.val["ahead_start_time"] = (int)ev_timer_remaining(zjh.loop, &ahead_start_timer);
    }
    else
    {
        packet.val["ahead_start_time"] = 0;
    }

    int i = 0;
    std::map<int, Player *>::iterator it;
    for (it = players.begin(); it != players.end(); it++)
    {
        Player *p = it->second;
        Seat &seat = seats[p->seatid];

        packet.val["players"][i]["uid"] = p->uid;
        packet.val["players"][i]["name"] = p->name;
        packet.val["players"][i]["seatid"] = p->seatid;
        packet.val["players"][i]["ahead_start"] = seat.ahead_start;

        i++;
    }

    packet.end();
    broadcast(NULL, packet.tostring());
}

void Table::ahead_start_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->ahead_start_timer);

    table->ahead_start_timeout();
}

void Table::subs_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    Table *table = (Table *)w->data;
    ev_timer_stop(zjh.loop, &table->ahead_start_timer);

    table->clean_table();
}

//提前开始申请时间到了默认是不提前开始
int Table::ahead_start_timeout()
{
    ahead_start_flag = 0;
    ahead_start_uid = -1;

    for (int i = 0; i < seat_max; i++)
    {
        seats[i].ahead_start = -1;
    }

    broadcast_ahead_start_status(NULL, 0);
    return 0;
}

int Table::handler_transfer_owner_req(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    //Seat &seat = seats[player->seatid];

    int transfer_uid = val["transfer_uid"].asInt();
    if (players.find(transfer_uid) == players.end())
    {
        handler_transfer_owner_req_error(player, 1);
        return -1;
    }

    if (transfer_uid == player->uid || transfer_uid == owner_uid || player->uid != owner_uid)
    {
        handler_transfer_owner_req_error(player, 2);
        return -1;
    }

    if (round_count != 0)
    {
        handler_transfer_owner_req_error(player, 3);
        return -1;
    }

    //AA付费没没有转让房主
    if (cost_select_flag == 2)
    {
        handler_transfer_owner_req_error(player, 5);
        return -1;
    }

    if (round_count != 0)
    {
        handler_transfer_owner_req_error(player, 3);
        return -1;
    }

    if (!transfer_flag)
    {
        players[origin_owner_uid]->incr_rmb(0 - create_rmb);
        Player *player1 = players[origin_owner_uid];
        insert_flow_log((int)time(NULL), player1->uid, player1->remote_ip, 0, type, zid, ttid, type, 0, create_rmb, player1->rmb);
    }

    Jpacket packet;
    packet.val["cmd"] = SERVER_TRANSFER_OWNER_SUCC_BC;
    packet.val["old_owner_uid"] = owner_uid;
    packet.val["owner_uid"] = transfer_uid;
    packet.val["owner_name"] = players[transfer_uid]->name;
    packet.val["old_owner_name"] = players[owner_uid]->name;
    packet.end();
    broadcast(NULL, packet.tostring());

    zjh.game->table_owners.erase(owner_uid);
    zjh.game->table_owners[transfer_uid] = tid;

    owner_uid = transfer_uid;
    transfer_flag = true;

    return 0;
}

void Table::handler_transfer_owner_req_error(Player *player, int reason)
{
    mjlog.error("handler transfer owner error, reason is %d.\n", reason);
    Jpacket packet;
    packet.val["cmd"] = SERVER_TRANSFER_OWNER_ERR_UC;
    packet.val["reason"] = reason;
    packet.end();
    unicast(player, packet.tostring());
}

int Table::handler_need_card_req(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Seat &seat = seats[player->seatid];
    int need_card = val["need_card"].asInt();

    if (set_card_flag == 0)
    {
        mjlog.info("handler need card req error[%d]\n", set_card_flag);
        return 0;
    }

    seat.need_card = need_card;

    return 0;
}

void Table::accumulate_hu()
{
    for (int i = 0; i < seat_max; i++)
    {
        Seat &seat = seats[i];
        if (seat.ready == 0)
        {
            continue;
        }

        if (seat.gang_hu_flag == 1 && gang_hu_seat >= 0)
        {
            seat.total_jie_pao++;
            seat.total_zhong_horse += seat.horse_count;
        }
        else if (seat.pao_hu_flag == 1 && pao_hu_seat >= 0)
        {
            seat.total_jie_pao++;
            seat.total_zhong_horse += seat.horse_count;
        }
        else if (i == win_seatid)
        {
            seat.total_zimo++;
            seat.total_zhong_horse += seat.horse_count;
        }
    }

    if (gang_hu_seat >= 0)
    {
        seats[gang_hu_seat].total_fang_pao++;
    }
    else if (pao_hu_seat >= 0)
    {
        seats[pao_hu_seat].total_fang_pao++;
    }
}

int Table::set_create_rmb(int rmb, int aarmb)
{
    create_rmb = rmb;
    return 0;
}

int Table::permit_join(Player *player)
{
    float gps_distance = zjh.conf["tables"]["gps_distance"].asFloat();
    std::map<int, Player *>::iterator it = players.begin();
    for (; it != players.end(); it++)
    {
        if (forbid_same_ip == 1)
        {
            if (it->second->remote_ip.compare(player->remote_ip) == 0)
            {
                return 1;
            }
        }

        if (forbid_same_place == 1)
        {
            Json::Value &val = player->client->packet.val;
            float jindu = val["jingdu"].asFloat();
            float weidu = val["weidu"].asFloat();

            player->jin_du = jindu;
            player->wei_du = weidu;

            Player *player1 = it->second;

            if (jindu > 1.0f && weidu > 1.0f && player1->jin_du > 1.0f && player1->wei_du > 1.0f)
            {
                if (GetDistance(jindu, weidu, player1->jin_du, player1->wei_du) < gps_distance)
                {
                    return 2;
                }
            }
        }
    }

    return 0;
}

int Table::handler_cmd(int cmd, Player *player)
{
    switch (cmd)
    {
    case CLIENT_READY_REQ:
        handler_ready(player);
        break;
    case CLIENT_BET_REQ:
        handler_bet(player);
        break;
    case CLIENT_CHAT_REQ:
        handler_chat(player);
        break;
    case CLIENT_FACE_REQ:
        handler_face(player);
        break;
    case CLIENT_INTERFACE_REQ:
        handler_interFace(player);
        break;
    case CLIENT_LOGOUT_REQ:
        break;
    case CLIENT_SWITCH_TABLE_REQ:
        break;
    case CLIENT_TABLE_INFO_REQ:
        handler_table_info(player);
        break;
    case CLIENT_PLAYER_INFO_REQ:
        handler_player_info(player);
        break;
    case CLIENT_PROP_REQ:
        handler_prop(player);
        break;
    case CLIENT_SEND_GIFT_REQ:
        handler_send_gift_req(player);
        break;
    case CLIENT_DISMISS_TABLE_REQ:
        handler_dismiss_table(player);
        break;
    case CLIENT_GPS_POS_NOTICE:
        handler_gps_notice(player);
        break;
    case CLIENT_GPS_DIS_REQ:
        handler_gps_dist_req(player);
        break;
    case CLIENT_VOICE_REQ:
        handler_voice_req(player);
        break;
    case CLIENT_START_GAME_REQ:
        handler_start_game_req(player);
        break;
    case CLIENT_NEED_CARD_REQ:
        handler_need_card_req(player);
        break;
    case CLIENT_TRANSFER_OWNER_REQ:
        handler_transfer_owner_req(player);
        break;
    case CLIENT_GET_REDPACKET_REQ:
        handler_get_redpacket_req(player);
        break;
    case CLIENT_GET_INTERNET_REQ:
        handler_get_internet_req(player);
        break;
    default:
        mjlog.error("invalid command[%d]\n", cmd);
        return -1;
    }

    return 0;
}

int Table::handler_redpacket()
{
    // 需要获取活动时间来判断是否允许派发红包
    int ret = zjh.temp_rc->command("hgetall rac");
    if (ret < 0)
    {
        mjlog.debug("no redpacket task.\n");
        return -1;
    }

    if (zjh.temp_rc->is_array_return_ok() < 0)
    {
        mjlog.debug("no redpacket task 1.\n");
        return -1;
    }

    long btime = zjh.temp_rc->get_value_as_int("btime");
    long etime = zjh.temp_rc->get_value_as_int("etime");
    std::string citys = zjh.temp_rc->get_value_as_string("area");
    long now = time(NULL);

    mjlog.debug("handler_redpacket btime [%ld] etime[%ld] citys[%s] now[%ld]\n",
                btime, etime, citys.c_str(), now);

    std::string all_city = "*";

    if (now < btime || now > etime)
    {
        mjlog.debug("no redpacket task 2.\n");
        return -1;
    }

    // 获取一个红包值
    ret = zjh.temp_rc->command("rpop red_list");

    if (ret >= 0 && max_ready_players == seat_max)
    {
        if (zjh.temp_rc->reply->str == NULL)
        {
            mjlog.debug("cann't get a redpacket 3.\n");
            return 0;
        }

        std::vector<int> tmp;
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].ready != 1 || seats[i].player == NULL)
            {
                continue;
            }
            mjlog.debug("handler_redpacket btime [%ld] etime[%ld] citys[%s] now[%ld] player->city[%s]\n",
                        btime, etime, citys.c_str(), now, seats[i].player->city.c_str());
            if (citys != all_city)
            {
                std::size_t found = citys.find(seats[i].player->city);
                if (found != std::string::npos)
                {
                    tmp.push_back(i);
                    seats[i].city_red_falg = 1;
                }
            }
            else
            {
                tmp.push_back(i);
                seats[i].city_red_falg = 1;
            }
        }
        if (tmp.size() > 0)
        {
            if (red_type == 0)
            {
                // 预先分配好红包
                random_shuffle(tmp.begin(), tmp.end());
                int len = tmp.size();
                int red_seatid = 0;
                if (len > 0)
                {
                    int index = random(0, len - 1);
                    red_seatid = tmp[index];
                }
                int red_values[4] = {
                    0,
                };
                red_values[red_seatid] = atoi(zjh.temp_rc->reply->str);
                //random_shuffle(red_value, red_value + 4);
                redpackes.clear();
                for (int i = 0; i < 4; i++)
                {
                    redpackes.push_back(red_values[i]);
                }
                //redpackes.assign(red_value, red_value + 4);
                mjlog.debug("red_seatid [%d] red_values[red_seatid] [%d] redpackes.size() [%d] \n",
                            red_seatid, red_values[red_seatid], redpackes.size());
            }
            else if (red_type == 1)
            {
                redpackes.clear();
                for (int i = 0; i < (seat_max - 1); i++)
                {
                    redpackes.push_back(0);
                }
                int red_values = atoi(zjh.temp_rc->reply->str);
                redpackes.push_back(red_values);
            }
            mjlog.debug("handler_redpacket redpackes.size() [%d]\n", redpackes.size());
            Jpacket packet;
            packet.val["cmd"] = SERVER_DISPATCH_REDPACKET_SUCC_BC;
            packet.end();
            broadcast(NULL, packet.tostring());
        }
    }
    else
    {
        mjlog.debug("cann't get a redpacket.\n");
    }

    return 0;
}

int Table::record_redpacket(Player *player, int value)
{
    Jpacket packet;
    packet.val["tid"] = ttid;
    packet.val["uid"] = player->uid;
    packet.val["money"] = value;
    packet.val["time"] = (int)time(NULL);
    packet.end();

    string str = packet.val.toStyledString().c_str();

    int ret = zjh.temp_rc->command("LPUSH red_record_list %s", str.c_str());

    if (ret < 0)
    {
        mjlog.error("insert_flow_record %s", str.c_str());
        return -1;
    }

    player->incr_total_red(value);
    return 0;
}

int Table::handler_get_redpacket_req(Player *player)
{
    if (redpackes.size() == 0)
    {
        Jpacket packet;
        packet.val["cmd"] = SERVER_GET_REDPACKET_ERR_UC;
        packet.val["err_str"] = "红包不存在。";
        packet.end();
        unicast(player, packet.tostring());
        return 0;
    }

    Seat &seat = seats[player->seatid];
    if (seat.already_get_red == 1)
    {
        Jpacket packet;
        packet.val["cmd"] = SERVER_GET_REDPACKET_ERR_UC;
        packet.val["err_str"] = "您的红包已经领取过了。";
        packet.end();
        unicast(player, packet.tostring());
        return 0;
    }

    if (seat.city_red_falg != 1)
    {
        seat.already_get_red = 1;
        Jpacket packet;
        packet.val["cmd"] = SERVER_GET_REDPACKET_ERR_UC;
        packet.val["err_str"] = "您不在活动所在区域，不能参与活动。";
        packet.end();
        unicast(player, packet.tostring());
        return 0;
    }

    //Json::Value &val = player->client->packet.tojson();
    int value = 0;
    if (red_type == 0)
    {
        value = redpackes[player->seatid];
    }
    else if (red_type == 1)
    {
        if (redpackes.size() > 0 && seat.city_red_falg == 1)
        {
            value = redpackes.back();
            redpackes.pop_back();
        }
    }
    seat.already_get_red = 1;

    Jpacket packet;
    packet.val["cmd"] = SERVER_GET_REDPACKET_SUCC_UC;
    packet.val["value"] = value;
    packet.end();
    unicast(player, packet.tostring());

    if (value > 0)
    {
        Jpacket packet1;
        packet1.val["cmd"] = SERVER_GET_REDPACKET_SUCC_BC;
        packet1.val["value"] = value;
        packet1.val["uid"] = player->uid;
        packet1.val["name"] = player->name;
        packet1.end();
        broadcast(NULL, packet1.tostring());

        record_redpacket(player, value);
    }

    return 0;
}

int Table::handler_invite_advantage(Player *player)
{
    if (player->pre_uid < 1000)
    {
        mjlog.debug("handler_invite_advantage player total_board[%d] pre_uid[%d]\n", player->total_board, player->pre_uid);
        return -1;
    }

    int ret = zjh.temp_rc->command("hgetall bac");
    if (ret < 0)
    {
        mjlog.debug("no invite advantage task.\n");
        return -1;
    }

    if (zjh.temp_rc->is_array_return_ok() < 0)
    {
        mjlog.debug("no invite advantage task 1.\n");
        return -1;
    }

    long btime = zjh.temp_rc->get_value_as_int("btime");
    long etime = zjh.temp_rc->get_value_as_int("etime");
    std::string citys = zjh.temp_rc->get_value_as_string("area");
    std::string all_city = "*";
    long now = time(NULL);

    mjlog.debug("handler_invite_advantage btime [%ld] etime[%ld] citys[%s] now[%ld] player->city[%s]\n",
                btime, etime, citys.c_str(), now, player->city.c_str());

    if (now < btime || now > etime)
    {
        mjlog.debug("handler_redpacket not at right time now [%d] btime[%d] etime[%d].\n", now, btime, etime);
        return -1;
    }

    if (citys != all_city)
    {
        std::size_t found = citys.find(player->city);
        if (found == std::string::npos)
        {
            mjlog.debug("handler_redpacket not at activity city citys[%s] player->city[%s].\n", citys.c_str(), player->city.c_str());
            return -1;
        }
    }

    player->incr_ac_board(vid, 1);

    mjlog.debug("handler_invite_advantage player uid[%d] ac_board[%d] pre_uid[%d]\n",
                player->uid, player->ac_board, player->pre_uid);

    if (player->ac_board != 8)
    {
        return -1;
    }

    ret = zjh.temp_rc->command("sadd tj_uid %d", player->uid);
    if (ret < 0)
    {
        mjlog.debug("handler_invite_advantage sadd tj_uid %d\n", player->uid);
        return -1;
    }
    return 0;
}

void Table::handler_substitute_req(Player *player)
{
    //int create_rmb = zjh.conf["tables"]["create_rmb"].asInt();
    Json::Value &val = player->client->packet.tojson();
    std::string describe = val["playway_desc"].asString();
    int player_max = val["player_max"].asInt();
    // int base_play_board = zjh.conf["tables"]["base_board"].asInt();

    player->tid = -1;

    Jpacket packet;
    packet.val["cmd"] = SERVER_SUBSTITUTE_SUCC_UC;
    packet.val["uid"] = player->uid;
    packet.val["ttid"] = ttid;
    packet.val["rmb"] = create_rmb;
    packet.end();
    unicast(player, packet.tostring());
    owner_uid = player->uid;
    owner_name = player->name;

    player->incr_rmb(0 - create_rmb);

    char buff[128] = {
        0,
    };

    snprintf(buff, 128, "南宁麻将，%d局，%d匹马", max_play_board, horse_num);
    if (fang_pao == 1)
    {
        snprintf(buff + strlen(buff), 128 - strlen(buff), "%s", "平胡可点炮");
    }
    else
    {
        snprintf(buff + strlen(buff), 128 - strlen(buff), "%s", "平胡需自摸");
    }

    if (dead_double == 1)
    {
        snprintf(buff + strlen(buff), 128 - strlen(buff), "%s", "死双");
    }

    int ret = zjh.temp_rc->command("hmset create:%d:%d uid %d, tid %d, type %d, size 0, status 0, ts %d, ruler %s player_max %d",
                                   player->uid, ttid, player->uid, ttid, type, (int)time(NULL), describe.c_str(), player_max);
    owner_uid = player->uid;
    owner_name = player->name;
    owner_sex = player->sex;
    if (ret < 0)
    {
        mjlog.error("insert substitute info error");
        return;
    }

    ret = zjh.temp_rc->command("lpush create:%d create:%d:%d", player->uid, player->uid, ttid);
    if (ret < 0)
    {
        mjlog.error("add substitute info error");
        return;
    }
}

void Table::modify_substitute_info(Player *player, int flag)
{
    int ret = zjh.temp_rc->command("hget create:%d:%d size", owner_uid, ttid);
    if (ret < 0)
    {
        mjlog.error("get substitute info error");
        return;
    }

    if (zjh.temp_rc->reply->str == NULL)
    {
        mjlog.error("get substitute str error");
        return;
    }
    int size = atoi(zjh.temp_rc->reply->str);

    if (flag == 1)
    {
        size += 1;

        ret = zjh.temp_rc->command("hmset create:%d:%d uid%d %d size %d", owner_uid, ttid, size, player->uid, size);
        if (ret < 0)
        {
            mjlog.error("modify substitute info error");
            return;
        }
    }
    else
    {
        if (size <= 0)
        {
            return;
        }

        vector<int> uids;
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].player != NULL)
            {
                uids.push_back(seats[i].uid);
            }
        }

        size = uids.size();

        if (uids.size() == 4)
        {
            ret = zjh.temp_rc->command("hmset create:%d:%d uid1 %d, uid2 %d, uid3 %d, uid4 %d, size %d", owner_uid, ttid,
                                       uids[0], uids[1], uids[2], uids[3], size);
        }
        else if (uids.size() == 3)
        {
            ret = zjh.temp_rc->command("hmset create:%d:%d uid1 %d, uid2 %d, uid3 %d, size %d", owner_uid, ttid,
                                       uids[0], uids[1], uids[2], size);
        }
        else if (uids.size() == 2)
        {
            ret = zjh.temp_rc->command("hmset create:%d:%d uid1 %d, uid2 %d, size %d", owner_uid, ttid, uids[0], uids[1], size);
        }
        else if (uids.size() == 1)
        {
            ret = zjh.temp_rc->command("hmset create:%d:%d uid1 %d, size %d", owner_uid, ttid, uids[0], size);
        }
        else if (uids.size() == 0)
        {
            ret = zjh.temp_rc->command("hset create:%d:%d size %d", owner_uid, ttid, size);
        }

        if (ret < 0)
        {
            mjlog.error("set substitute info error");
        }
    }
}

void Table::modify_substitute_info(int status)
{
    int ret = zjh.temp_rc->command("hset create:%d:%d status %d", owner_uid, ttid, status);
    if (ret < 0)
    {
        mjlog.error("modify substitute status error");
        return;
    }
}

void Table::clear_substitute_info()
{
    // int ret = zjh.temp_rc->command("del create:%d:%d", owner_uid, ttid);
    // if (ret < 0)
    // {
    //     mjlog.error("clear substitute info error");
    // }

    int ret = zjh.temp_rc->command("lrem create:%d 0 create:%d:%d", owner_uid, owner_uid, ttid);
    if (ret < 0)
    {
        mjlog.error("clear substitute info1 error");
    }
}

void Table::create_table_cost() //创建房间扣费函数
{
    int base_play_board = zjh.conf["tables"]["base_board"].asInt();
    base_play_board = base_play_board <= 0 ? 8 : base_play_board;
    int ratio = max_play_board / base_play_board;
    ratio = std::max(1, ratio);

    if (cost_select_flag == 1) //房主扣费
    {
        mjlog.debug("create_table_cost cost_select_flag [%d] uid[%d] create_rmb[%d]\n",
                    cost_select_flag, owner_uid, create_rmb);
        players[owner_uid]->incr_rmb(0 - create_rmb);
        Player *player = players[owner_uid];
        insert_flow_log((int)time(NULL), player->uid, player->remote_ip, 0, type, zid, ttid, type, 0, create_rmb, player->rmb);
    }
    else if (cost_select_flag == 2) //AA扣费
    {
        for (int i = 0; i < seat_max; i++)
        {
            if (seats[i].player != NULL)
            {
                mjlog.debug("create_table_cost cost_select_flag [%d] uid[%d] create_rmb[%d]\n",
                            cost_select_flag, seats[i].player->uid, create_rmb);
                seats[i].player->incr_rmb(0 - create_rmb);
                insert_flow_log((int)time(NULL), seats[i].player->uid, seats[i].player->remote_ip, 0, type, zid, ttid, type, 0, create_rmb, seats[i].player->rmb);
            }
        }
    }
}

int Table::random_dealer()
{
    std::vector<int> tmp;
    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].ready == 1)
        {
            tmp.push_back(i);
        }
    }

    int len = tmp.size();
    if (len > 0)
    {
        int index = random(0, len - 1);
        int i = tmp[index];
        mjlog.debug("len[%d] index[%d] i[%d]\n", len, index, i);
        return i;
    }

    return -1;
}

int Table::handler_owner_dismiss_table()
{
    if (substitute == 1)
    {
        Jpacket packet;
        packet.val["cmd"] = SERVER_DISMISS_TABLE_SUCC_BC;
        packet.val["uid"] = owner_uid;
        packet.val["name"] = owner_name;
        packet.val["flag"] = 0;
        packet.end();
        broadcast(NULL, packet.tostring());
    }
    return 0;
}

int Table::handler_get_internet_req(Player *player)
{
    Json::Value &val = player->client->packet.tojson();
    Jpacket packet;
    packet.val["cmd"] = SERVER_GET_INTERNET_UC;
    packet.val["uid"] = player->uid;
    packet.val["massage"] = val["massage"];
    packet.end();
    unicast(player, packet.tostring());
    return 0;
}

void Table::huang_zhuang_cha_jiao()
{
    if (!(win_seatid < 0 && gang_hu_count < 2 && pao_hu_count < 2))
    { //如果没有黄庄，则不进行查叫判断
        return;
    }

    jiao_pai_cnt = 0;
    for (int i = 0; i < seat_max; i++)
    {
        seats[i].jiao_pai = 0; //叫牌玩家清0
        if (seats[i].occupied != 1)
        {
            continue;
        }
        seats[i].hole_cards.analysis();

        //不叫牌玩家需要给叫牌玩家按牌型分别支付分值。
        if (seats[i].hole_cards.hu_cards.size() > 0)
        { //选出最大听牌的牌型
            seats[i].jiao_pai = 1;
            ++jiao_pai_cnt;

            seats[i].jiao_pai_max_score = 0;
            seats[i].jiao_pai_max_type = 0;

            for (unsigned int j = 0; j < seats[i].hole_cards.hu_cards.size(); j++)
            {
                int score = calculate_base_score(i, 1, seats[i].hole_cards.hu_cards[j].value);
                if (seats[i].jiao_pai_max_score < score)
                {
                    seats[i].jiao_pai_max_score = score;
                    seats[i].jiao_pai_max_type = seats[i].hole_cards.card_type;
                }
            }
            seats[i].hu_pai_lei_xing = "叫牌";
            seats[i].hole_cards.card_type = seats[i].jiao_pai_max_type;
            seats[i].card_type = seats[i].jiao_pai_max_type;
            mjlog.debug("huangzhuangchajia seats[%d] max_type[%d] max_score[%d]\n", i,
                        seats[i].jiao_pai_max_type, seats[i].jiao_pai_max_score);
            //没有听牌的玩家，给这个玩家相应的分値
            for (int j = 0; j < seat_max; j++)
            {
                if (seats[j].ready != 1 || i == j)
                {
                    continue;
                }
                if (seats[j].hole_cards.hu_cards.size() <= 0) //没有听牌玩家
                {
                    seats[i].score = seats[i].jiao_pai_max_score;
                    seats[i].bet += seats[i].jiao_pai_max_score;
                    seats[i].score_from_players_detail[j][JIAO_PAI_TYPE] += seats[i].jiao_pai_max_score;
                    score_from_players_item_count[i][JIAO_PAI_TYPE]=1;
                    score_to_players_item_count[j][JIAO_PAI_TYPE]++;
                    mjlog.debug("HUAN ZHUANG CHA JIAO seatsid[%d] win %d seatsid[%d] lose\n", i, seats[i].jiao_pai_max_score, j);
                    seats[j].bet -= seats[i].jiao_pai_max_score;

                    score_from_players_item_total[i][JIAO_PAI_TYPE] += seats[i].jiao_pai_max_score;
                    score_to_players_item_total[j][JIAO_PAI_TYPE] += seats[i].jiao_pai_max_score;
                }
            }
        }
        else
        {
            seats[i].hu_pai_lei_xing = "未叫牌";
        }
    }

    for (int i = 0; i < seat_max; i++)
    {
        if (seats[i].ready != 1)
        {
            continue;
        }
        mjlog.debug("score item jiao_pai_type# from[%d] cnt[%d] to[%d] cnt[%d] \n", score_from_players_item_total[i][JIAO_PAI_TYPE], score_from_players_item_count[i][JIAO_PAI_TYPE],
                    score_to_players_item_total[i][JIAO_PAI_TYPE], score_to_players_item_count[i][JIAO_PAI_TYPE]);
    }

    return;
}

int Table::next_player_seatid_of(int cur_player)
{ //计算cur_player的下一个玩家
    int next_player;
    for (int i = 1; i < seat_max; ++i)
    {
        next_player = (cur_player + i) % seat_max;
        if (seats[next_player].ready != 1)
            continue;
        return next_player;
    }
    return -1;
}

int Table::pre_player_seatid_of(int cur_player)
{ //计算cur_player的上一个玩家
    int pre_player;
    for (int i = 1; i < seat_max; ++i)
    {
        pre_player = (cur_player - i + seat_max) % seat_max;
        if (seats[pre_player].ready != 1)
            continue;
        return pre_player;
    }
    return -1;
}

//从redis 中读取测试的所需牌值
int Table::get_set_hole_cards(Player * player)
{
	if(set_card_flag ==0)
	{
		return -1;
	}
    char buff[128] = {0, };

   	snprintf(buff, 128, "hc_%d", player->uid);

	if(zjh.temp_rc->command("LLEN %s",buff) < 0)
	{
		mjlog.debug("get_set_hole_cards llen %s error \n", buff);
		return-1;
	}

	int len  = zjh.temp_rc->reply->integer;
	mjlog.debug("get_set_hole_cards cards len [%d] \n", len);
	if(len < 0)
	{
		return -1;
	}

	char buff1[128] = {0, };
	for(int i =0;i<len;i++)
	{
		int ret = zjh.temp_rc->command("RPOP %s",buff);
		if(ret < 0)
		{
			continue;
		}
		
		unsigned int card_value = 0;
		sscanf(zjh.temp_rc->reply->str, "%x", &card_value);
		snprintf(buff1, 128, "%d_", card_value);
		seats[player->seatid].set_hole_cards.push_back(card_value);
	}
	mjlog.debug("get_set_hole_cards buff1 [%s]\n",buff1);
	return 0;
}

void Table::ji_card_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    Table *table = (Table*) w->data;
	ev_timer_stop(zjh.loop, &table->ji_card_timer);
	mjlog.debug("ji_card_timer_cb\n");
	table->game_end();
}

void Table::ji_game_end()
{
    update_account_bet();
	Jpacket packet;
    packet.val["cmd"] = SERVER_DISPLAY_JI_CARD_SUIT_SUCC_BC;
    // for (unsigned int i = 0; i < ji_pai.size(); ++i)
    // {
    //     if (ji_pai[i].type == BEN_JI || ji_pai[i].type == YAO_BAI_JI)
    //     {
    //         packet.val["value"].append(ji_pai[i].value);
    //         packet.val["type"].append(ji_pai[i].type);
    //     }
    // }
    if (fang_ben_ji != 0)
    {
        packet.val["value"].append(fang_ben_ji);
        packet.val["type"].append(BEN_JI);
    }
    // if (fang_fang_ji != 0)
    // {
    //     packet.val["value"].append(fang_fang_ji);
    //     packet.val["type"].append(FANG_JI);
    // }
    if (fang_yao_bai_ji1 != 0)
    {
        packet.val["value"].append(fang_yao_bai_ji1);
        packet.val["type"].append(YAO_BAI_JI);
    }
    if (fang_yao_bai_ji1 != 0)
    {
        packet.val["value"].append(fang_yao_bai_ji2);
        packet.val["type"].append(YAO_BAI_JI);
    }
	packet.end();
    broadcast(NULL, packet.tostring());
    replay.append_record(packet.tojson());
    ev_timer_again(zjh.loop, &ji_card_timer);
}

void Table::record_table_info()
{
	Jpacket packet;
	packet.val["owner_uid"] = owner_uid;
    packet.val["cur_round"] = round_count;
    packet.val["total_round"] = max_play_board;
	packet.val["has_feng"] = has_feng;
    // packet.val["has_ghost"] = has_ghost;
    packet.val["hu_pair"] = hu_pair;
    packet.val["horse_num"] = horse_num;
    packet.val["max_play_count"] = max_play_board;
    packet.val["table_type"] = type;
	packet.val["substitute"] = substitute;
    packet.val["cost_select_flag"] = cost_select_flag;
    packet.val["ttid"] = ttid;
    packet.val["ben_ji"]= ben_ji;
    packet.val["wu_gu_ji"]= wu_gu_ji;
    packet.val["bao_ji"]= bao_ji;
	packet.val["cmd"] = SERVER_TABLE_INFO_UC;
    // packet.val["ghost_card"] = ghost_card;
	packet.end();
	replay.append_record(packet.tojson());	
}
