#ifndef _PLAYER_H_
#define _PLAYER_H_

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

#include <json/json.h>
#include <ev.h>



class Client;

class Player
{		
public:
	int 				index;
	// table router information
	int					vid;
	int					zid;
    int					tid;
	int					seatid;
	// player information
    int                 uid;
	std::string			skey;
	std::string			name;
	int					sex;
	std::string			avatar;
	std::string			birthday;
	std::string			zone;
    std::string			province;
    std::string         city;
	int					rmb;
	int					money;
	int					total_board;
    int                 total_create;
    int                 total_play;
	int                 total_red;
    int                 curtid;
    int                 gameid;
    int                 pre_uid;
    int                 ac_board;

	// connect to client
	Client              *client;
	
	/*
	int					ready;
	int					see;
	int					role; // 0 : Player 1 : Dealer
	HoleCards			hole_cards;
	*/
	int 				logout_type;
	int					time_cnt;

	std::string 		remote_ip;
    int                 pay_total;
    int                 is_owner;

    float               jin_du;
    float               wei_du;
	
private:
    ev_timer			_offline_timer;
    ev_tstamp			_offline_timeout;

public:
    Player();
    virtual ~Player();
    void set_client(Client *c);
	int init();
	void reset();
	int money_check();
	int update_info();
	int set_money(int value);
	int set_rmb(int value);
	int incr_money(int type, int value);
    static int incr_rmb(int pid, int value);
    int incr_rmb(int value);
	int incr_total_board(int vid, int value);
	int incr_ac_board(int vid, int value);
    int incr_total_create(int value);
    int incr_total_play(int value);
	int incr_total_red(int value);
    void start_offline_timer();
	void stop_offline_timer();
    static void offline_timeout(struct ev_loop *loop, ev_timer *w, int revents);
    int random(int start, int end);
    int set_curtid(int myid);
    int set_gameid(int myid);
private:

};

#endif
