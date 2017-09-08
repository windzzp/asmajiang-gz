#ifndef _CARD_TYPE_H_
#define _CARD_TYPE_H_

enum CardType
{
	CARD_TYPE_ERROR = 0, //错误牌型
	CARD_TYPE_JI_HU = 1, //鸡胡 (包括顺子和刻字)
	CARD_TYPE_PING_HU = 2,//平胡 (全部是顺子没有刻字)
	CARD_TYPE_PENG_HU = 3,//碰碰胡
	CARD_TYPE_HUN_YI_SE = 4,//混一色 (条万筒中的一种加上字牌)
	CARD_TYPE_QING_YI_SE = 5,//清一色 (条万筒中的一种无字牌)
	CARD_TYPE_HUN_PENG = 6,//混碰 （混一色加碰碰胡）
	CARD_TYPE_QING_PENG = 7,//清碰 （清一色加碰碰胡)
	CARD_TYPE_HUN_YAO_JIU = 8,//混幺九(一、九和字牌的碰碰胡)
	CARD_TYPE_XIAO_SAN_YUAN = 9,//小三元（中发白中两种成刻，剩余一种做刻）
	CARD_TYPE_XIAO_SI_XI = 10,//小四喜(东南西北四种风牌中三种的刻或杠加第四种的一对)
	CARD_TYPE_ZI_YI_SE = 11,//字一色
	CARD_TYPE_QING_YAO_JIU = 12, //清幺九(一、九的碰碰胡，没有字牌)
	CARD_TYPE_DA_SAN_YUAN = 13,//大三元 （中发白三个刻字)
	CARD_TYPE_DA_SI_XI = 14,//大四喜(东南西北四种刻或杠加任意一对)
	CARD_TYPE_JIU_LIAN_BAO_DENG = 15,//九宝莲灯（同花色1112345678999加同花色任意一张胡牌)
	CARD_TYPE_SHI_SAN_YAO = 16,//十三幺（十三幺必然是万、条、筒、风、箭五门齐全）
	CARD_TYPE_QUAN_QIU_REN = 17,     // 全求人
	CARD_TYPE_QI_XIAO_DUI = 18, // 7小对
	CARD_TYPE_HAO_HUA_QI_DA_DUI = 19, //豪华七大对
	CARD_TYPE_SHUANG_HAO_HUA_QI_DA_DUI = 20, //双豪华七大对
	CARD_TYPE_SAN_HAO_HUA_QI_DA_DUI = 21, //三豪华七大对
    CARD_TYPE_SHI_BA_LUO_HAN = 22, //十八罗汉
    CARD_TYPE_HAS_YAO_JIU = 23, // 带幺九
    CARD_TYPE_QING_QI_DUI = 24, // 清7对
    CARD_TYPE_LONG_QI_DUI = 25, // 龙7对
    CARD_TYPE_QING_LONG_QI_DUI = 26, // 清龙7对 
    CARD_TYPE_JIN_GOU_DIAO = 27, // 金钩钓
    CARD_TYPE_JIANG_JIN_GOU_DIAO = 28, // 将金钩钓
    CARD_TYPE_QING_JIN_GOU_DIAO = 29, // 清金钩钓
    CARD_TYPE_QING_SHI_BA_LUO_HAN = 30, // 清一色十八罗汉
    CARD_TYPE_QING_HAS_YAO_JIU = 31
};

#endif /* _CARD_TYPE_H_ */
