#ifndef __PROTO_H__
#define __PROTO_H__

enum system_command
{
    SYS_ECHO						= 0001,       /* echo */
    SYS_ONLINE						= 0002,       /* get online */
	SYS_DUIKANG						= 0003,       /* duikang */
    SYS_TERMINAL                    = 0004,       /* terminal */
	SYS_DISMISS                     = 0006        /* dismiss table */
};

typedef enum
{
	READY = 0,
	BETTING,
    END_GAME,
    ROOM_WAIT_GAME
} State;

enum client_command
{
	CLIENT_LOGIN_REQ				= 1001,       /* join table */
	CLIENT_LOGOUT_REQ				= 1002,
	CLIENT_READY_REQ				= 1003,	      /* game ready */
	CLIENT_BET_REQ		 			= 1004,
	CLIENT_CHAT_REQ					= 1005,
	CLIENT_FACE_REQ					= 1006,
	CLIENT_INTERFACE_REQ			= 1007,
	CLIENT_SWITCH_TABLE_REQ			= 1008,
	CLIENT_TABLE_INFO_REQ			= 1009,
	CLIENT_PLAYER_INFO_REQ			= 1010,
	CLIENT_PROP_REQ					= 1011,
    CLIENT_SEND_GIFT_REQ            = 1012,
    CLIENT_CREATE_TABLE_REQ         = 1013,
    CLIENT_JOIN_TABLE_REQ           = 1014,
    CLIENT_DISMISS_TABLE_REQ        = 1015,
    CLIENT_GPS_POS_NOTICE           = 1016,
    CLIENT_GPS_DIS_REQ              = 1017,
    CLIENT_VOICE_REQ                = 1018,
	CLIENT_START_GAME_REQ			= 1019,
	CLIENT_NEED_CARD_REQ 			= 1020,
    CLIENT_TRANSFER_OWNER_REQ       = 1021,
	CLIENT_GET_REDPACKET_REQ        = 1022,
    CLIENT_GET_INTERNET_REQ         = 1029,
};

enum client_action
{
	PLAYER_CHU 					    = 2001,
    PLAYER_GUO                      = 2002,
    PLAYER_CHI	                    = 2003, 
	PLAYER_PENG		                = 2004, 
	PLAYER_GANG						= 2005,	
    PLAYER_TING	                    = 2006, 
    PLAYER_HU                       = 2007,
    PLAYER_JIABEI                   = 2008,
    PLAYER_CANCEL                   = 2009,
    PLAYER_TUOGUAN                  = 2010,
    PLAYER_CANCEL_TUOGUAN           = 2011
};

enum notice_action
{
    NOTICE_GUO  =  0,
    NOTICE_CHI,
    NOTICE_PENG,
    NOTICE_GANG,
    NOTICE_TING,
    NOTICE_HU,
    NOTICE_JIABEI,
    NOTICE_CANCEL,

    NOTICE_SIZE
};

enum server_command
{
	SERVER_LOGIN_SUCC_UC       		 = 4000,
    SERVER_LOGIN_SUCC_BC       		 = 4001,
    SERVER_LOGIN_ERR_UC         	 = 4002,
	SERVER_REBIND_UC				 = 4003,
	SERVER_LOGOUT_SUCC_BC			 = 4004,
	SERVER_LOGOUT_ERR_UC			 = 4005,
	SERVER_TABLE_INFO_UC			 = 4006,
	SERVER_READY_SUCC_BC			 = 4007,
	SERVER_READY_ERR_UC				 = 4008,
	SERVER_GAME_START_BC			 = 4009,
	SERVER_NEXT_BET_BC				 = 4010,
	SERVER_BET_SUCC_BC				 = 4011,
	SERVER_BET_SUCC_UC				 = 4012,
	SERVER_BET_ERR_UC			     = 4013,
	SERVER_GAME_END_BC				 = 4014,
	SERVER_GAME_PREREADY_BC			 = 4015,
	SERVER_CHAT_BC					 = 4016,
	SERVER_FACE_BC					 = 4017,
	SERVER_INTERFACE_BC				 = 4018,
	SERVER_SWITCH_TABLE_BC			 = 4019,
	SERVER_PLAYER_INFO_BC			 = 4020,
	SERVER_PROP_SUCC_BC				 = 4021,
	SERVER_PROP_SUCC_UC				 = 4022,
	SERVER_PROP_ERR_UC				 = 4023,
    SERVER_SEND_GIFT_SUCC_BC         = 4024,
    SERVER_SEND_GIFT_ERR_UC          = 4025,
    SERVER_RESULT_TO_ROBOT           = 4026,

    SERVER_CREATE_TABLE_SUCC_UC      = 4027,
    SERVER_CREATE_TABLE_ERR_UC       = 4028,
    SERVER_JOIN_TABLE_SUCC_UC        = 4029,
    SERVER_JOIN_TABLE_ERR_UC         = 4030,
    SERVER_DISMISS_TABLE_SUCC_BC     = 4031,
    SERVER_LOGIN_GAME_SUCC_UC        = 4032,

    SERVER_NET_STATUS_BC             = 4033,
    SERVER_IP_CONFLICT               = 4044,
    SERVER_FORBID_HU_BC              = 4045, // 禁止胡牌

    SERVER_GPS_POS_NOTICE_SUCC_UC    = 4046,
    SERVER_GPS_DIS_SUCC_UC           = 4047,
    SERVER_VOICE_SUCC_BC             = 4048,
	SERVER_TURN_GHOST_BC			 = 4049,//告诉客户端鬼牌是哪一张
	SERVER_START_GAME_REQ_BC		 = 4050,//申请开始游戏成功
	SERVER_PLAY_PROCEDURE_BC		 = 4051,//玩家吃碰杠过程记录
    SERVER_TRANSFER_OWNER_SUCC_BC    = 4052,
    SERVER_TRANSFER_OWNER_ERR_UC     = 4053,

	SERVER_DISPATCH_REDPACKET_SUCC_BC = 4054,
    SERVER_GET_REDPACKET_SUCC_BC = 4055,
    SERVER_GET_REDPACKET_SUCC_UC = 4056,
    SERVER_GET_REDPACKET_ERR_UC = 4057,
    SERVER_SUBSTITUTE_SUCC_UC = 4058,

    SERVER_GET_INTERNET_UC         = 4070,

    SERVER_DISPLAY_JI_CARD_SUIT_SUCC_BC = 4085,     //显示鸡牌过程
};

#endif
