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
};

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
    int handler_create_table_req(Client* client);
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
    int handler_login_game_succ_uc(Client* client);
    int login_create_table(Client* client);
    int handler_robot_login_table(Client* client);
    int register_server_key();
    bool is_online(Player* player);
    int init_deck();
    static int get_create_rmb();
    static int get_create_aarmb();

	int get_play_rmb(int max_play_count, int cost_select_flag);
	int del_subs_table(int uid);
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
