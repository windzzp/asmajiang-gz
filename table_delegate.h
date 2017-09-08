#ifndef _TABLE_DELEGATE_H
#define _TABLE_DELEGATE_H

#include <json/json.h>

class Table;
class Player;

class TableDelegate
{
public:
    static TableDelegate* getInstance();

    Table* getTable();
    int init_table(Table*, int i);
    void handler_net_status(Table* table, Player* player, int status);
    int handler_login(Table* table, Player *player);
    int handler_login_succ_uc(Table* table, Player* player);
    int handler_table_info(Table* table, Player* player);
    int handler_logout(Table* table, Player* player);
    int del_player(Table* table, Player* player);
    int handler_cmd(Table* table, int cmd, Player* player);
    int get_tid(Table* table);
    int get_ttid(Table* table);
    std::map<int, Player*>* get_players(Table* table);
    int get_state(Table* table);
    int get_ts(Table* table);
    void clean_table(Table* table);
    void set_ttid(Table* table, int ttid);
    void set_state(Table* table, int state);
    void set_owner_uid(Table* table, int uid);
    void init_table_type(Table* table, Json::Value &val, int robot = 0);
	int init_deck(int type);
	int get_max_players(Table* table);
	int get_ahead_start(Table* table);
    int permit_join(Table* table, Player* player);
	int set_create_rmb(Table* table, int rmb, int aarmb = 0);
	int get_create_rmb(Table* table);
	int get_cost_select_flag(Table* table);
    int get_rmb_num(int play_board);
	int get_substitute_flag(Table* table);
    void handler_dismiss_table(Table * table);

private:
    static TableDelegate* m_pdl;
};

#endif
