#ifndef _GAME_H_
#define _GAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ev++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <vector>

class Client;
class Player;
class Table;

enum FlowMode
{
    FLOW_IMMEDIATE = 0,
    FLOW_END
};

enum CreateTableErr
{
    CREATE_TABLE_HAVE_NOT_RMB = 0,
    CREATE_TABLE_HAVE_NOT_TABLES,
    CREATE_TABLE_STATE_NOT_PERMIT,
    CREATE_TABLE_IN_TABLE,
    CREATE_TABLE_MAX_SUBS,
    CREATE_TABLE_NOT_FIND_CLUB,//没有找到该俱乐部
    CREATE_TABLE_CLUB_HAVE_NOT_RMB,//俱乐部钻石不足
    CREATE_TABLE_CLUB_ALREADY_AUTO_ROOM,//存在自动开的房间未开始
    CREATE_TABLE_ONLY_CHARGE_CREATE_TABLE,//只有管理者才能开房
    CREATE_TABLE_NOT_CLUB_UIDS,//不是俱乐部成员不能开房
    CREATE_TABLE_IN_CLUB_BLACK_UIDS,//在俱乐部黑名单中
    CREATE_TABLE_CLUB_OVER_EVERY_RMB,//超过每日上限

};

enum JoinTableErr
{
    JOIN_TABLE_TABLE_NOT_EXSIT = 0,
    JOIN_TABLE_HAS_NOT_PERMIT,
    JOIN_TABLE_HAS_NOT_SEAT,
    JOIN_TABLE_IS_TABLE_OWNER,
    JOIN_TABLE_STATE_NOT_PERMIT,
    JOIN_TABLE_HAVE_NOT_MONEY,
    JOIN_TABLE_FORBID_SAME_IP,
    JOIN_TABLE_FORBID_SAME_GPS,
    JOIN_TABLE_IN_CLUB_BLACK_UIDS,//在该俱乐部的黑名单中
    JOIN_TABLE_ONLY_CLUB_UIDS,//只允许本俱乐部成员可进
};

typedef struct
{
    int club_id;                 //俱乐部id
    int aid;                     //俱乐部代理id
    std::string aname;           //代理的姓名
    std::vector<int> uids;       //俱乐部成员id
    std::vector<int> black_uids; //俱乐部黑名单成员
    int rmb;                     //俱乐部账户钻石
    int rmb_every;               //俱乐部成员每人每天能消耗钻石数
    int status;                  //俱乐部状态 0 可用 -1 解散
    int auto_room;               //是否已经存在俱乐部机器人开房了 没有开始的。
    int rmb_cost;                //付费模式 0 扣俱乐部 1 开房者
    int join_room;               // 加入房间条件 0 任何人 1 俱乐部成员
    int create_play;             // 开房权限 0群主开房  1 俱乐部成员(具体还不知道怎么用)

    void clear(void)
    {
        club_id = 0;
        aid = 0;
        aname = "";
        uids.clear();
        black_uids.clear();
        rmb = 0;
        rmb_every = 0;
        status = 0;
        auto_room = 0;
        rmb_cost = 0;
        join_room = 0;
        create_play = 0;
    }
}CLUB;


class Game
{
public:
	std::map<int, Table*>       seven_tables;
	std::map<int, Table*>       six_tables;
	std::map<int, Table*>       five_tables;
	std::map<int, Table*>       four_tables;
	std::map<int, Table*>       three_tables;
	std::map<int, Table*>       two_tables;
	std::map<int, Table*>       one_tables;
	std::map<int, Table*>       zero_tables;
	std::map<int, Table*>       all_tables;
    std::map<int, Table*>       substitute_tables;
    std::map<int, Client*> 		fd_client;
	std::map<int, Player*>      offline_players;
	std::map<int, Player*>      online_players;
    std::map<int, int>          table_owners; // uid -> table id, don't store substitute tables
    std::map<int, int>          table_ttid;
    std::map<int, int>          player_table_count;//开放房间数

    int                         terminal_flag;

    ev_timer timeout_timer;
    ev_tstamp timeout_timer_stamp;

    ev_timer check_login_timer;
    ev_tstamp check_login_timer_stamp;

private:
    ev_io _ev_accept;
	int _fd;

public:
    Game();
    virtual ~Game();	
	int start();
	static void accept_cb(struct ev_loop *loop, struct ev_io *w, int revents);
    void del_client(Client *client, int flag = 0);

    int dispatch(Client *client);
	int safe_check(Client *client, int cmd);
    int handler_login_table(Client *client);
    
	int login_table(Client *client, std::map<int, Table*> &a, std::map<int, Table*> &b);
	
	int handle_logout_table(int tid);
	int send_error(Client *client, int cmd, int error_code);
	
	int check_skey(Client *client);
    int add_player(Client *client);
    int del_player(Player *player);
    int switch_table(Player *player);

    int check_up_list(Client *client);
    int handler_create_req(Client * client);
    int handler_create_table_req(Client* client);
    int handler_create_table_club_req(Client* client);
    void handler_create_table_req_error(Client* client, int reason);
    void handler_create_table_req_error(Client* client, int uid, int reason);
    int handler_join_table_req(Client* client);
    void handler_join_table_req_error(Client* client, int reason);
    static void timeout_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
    int handler_timeout();

    static void check_login_timer_cb(struct ev_loop *loop, struct ev_timer *w, int revents);
    int handler_check_login_timeout();

    void clear_in_game_flag();
    void set_in_game_flag(Player *player, int flag);

    void set_table_flag(int tid, int flag);
    void clear_table_flag();
    void clear_online_uid();
    int handler_login_game_succ_uc(Client* client);
    int login_create_table(Client* client);
    int login_create_club_table(Client * client, CLUB & club);
    int handler_robot_login_table(Client* client);
    int register_server_key();
    bool is_online(Player* player);
    int init_deck();
    static int get_create_rmb();
    static int get_create_aarmb();

	int get_play_rmb(int max_play_count, int cost_select_flag);
    int del_subs_table(int uid);
    int get_clubid_rmb_every(int cur_clubid, int uid);
    int init_club(CLUB & club, int ciub_id);
public:
	int     init_table();
	bool 	robot_is_ok(int pid, int num);
    int     init_accept();
	int 	robot_begin;
	int 	robot_end;
	int 	robot_max;
	int		duikang;

    volatile int     flow_mode;
};

#endif
