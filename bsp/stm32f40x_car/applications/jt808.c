/************************************************************
 * Copyright (C), 2008-2012,
 * FileName:		// 文件名
 * Author:			// 作者
 * Date:			// 日期
 * Description:		// 模块描述
 * Version:			// 版本信息
 * Function List:	// 主要函数及其功能
 *     1. -------
 * History:			// 历史修改记录
 *     <author>  <time>   <version >   <desc>
 *     David    96/10/12     1.0     build this moudle
 ***********************************************************/
#include <stdio.h>

#include <board.h>
#include <rtthread.h>
#include <finsh.h>

#include "stm32f4xx.h"
#include "gsm.h"
#include "jt808.h"
#include "msglist.h"
#include "jt808_sprintf.h"
#include "sst25.h"

#include "pt.h"

#include "tree_config.h"

#define ByteSwap2( val )    \
    ( ( ( val & 0xff ) << 8 ) |   \
      ( ( val & 0xff00 ) >> 8 ) )

#define ByteSwap4( val )    \
    ( ( ( val & 0xff ) << 24 ) |   \
      ( ( val & 0xff00 ) << 8 ) |  \
      ( ( val & 0xff0000 ) >> 8 ) |  \
      ( ( val & 0xff000000 ) >> 24 ) )

typedef struct
{
	uint16_t id;
	int ( *func )( JT808_RX_MSG_NODEDATA* nodedata );
}HANDLE_JT808_RX_MSG;

static struct rt_mailbox	mb_gprsdata;
#define MB_GPRSDATA_POOL_SIZE 32
static uint8_t				mb_gprsdata_pool[MB_GPRSDATA_POOL_SIZE];

static struct rt_mailbox	mb_gpsdata;
#define MB_GPSDATA_POOL_SIZE 32
static uint8_t				mb_gpsdata_pool[MB_GPSDATA_POOL_SIZE];

uint32_t					jt808_alarm		= 0x0;
uint32_t					jt808_status	= 0x0;

static uint16_t				tx_seq = 0; /*发送序号*/

static rt_device_t			pdev_gsm = RT_NULL;

static struct pt			pt_jt808_socket;

/*发送信息列表*/
MsgList* list_jt808_tx;

/*接收信息列表*/
MsgList		* list_jt808_rx;

T_GPSINFO	gpsinfo;

GSM_SOCKET	gsm_socket[MAX_GSM_SOCKET];

GSM_SOCKET	curr_gsm_socket;

JT808_PARAM jt808_param =
{
	0x13022200,         /*0x0000 版本*/
	5,                  /*0x0001 心跳发送间隔*/
	5,                  /*0x0002 TCP应答超时时间*/
	3,                  /*0x0003 TCP超时重传次数*/
	3,                  /*0x0004 UDP应答超时时间*/
	5,                  /*0x0005 UDP超时重传次数*/
	3,                  /*0x0006 SMS消息应答超时时间*/
	5,                  /*0x0007 SMS消息重传次数*/
	"CMNET",            /*0x0010 主服务器APN*/
	"",                 /*0x0011 用户名*/
	"",                 /*0x0012 密码*/
	"60.28.50.210",     /*0x0013 主服务器地址*/
	"CMNET",            /*0x0014 备份APN*/
	"",                 /*0x0015 备份用户名*/
	"",                 /*0x0016 备份密码*/
	"www.google.com",   /*0x0017 备份服务器地址，ip或域名*/
	9131,               /*0x0018 TCP端口*/
	5678,               /*0x0019 UDP端口*/
	"",                 /*0x001A ic卡主服务器地址，ip或域名*/
	0,                  /*0x001B ic卡服务器TCP端口*/
	0,                  /*0x001C ic卡服务器UDP端口*/
	"",                 /*0x001D ic卡备份服务器地址，ip或域名*/
	0,                  /*0x0020 位置汇报策略*/
	1,                  /*0x0021 位置汇报方案*/
	30,                 /*0x0022 驾驶员未登录汇报时间间隔*/
	120,                /*0x0027 休眠时汇报时间间隔*/
	5,                  /*0x0028 紧急报警时汇报时间间隔*/
	30,                 /*0x0029 缺省时间汇报间隔*/
	500,                /*0x002c 缺省距离汇报间隔*/
	1000,               /*0x002d 驾驶员未登录汇报距离间隔*/
	1000,               /*0x002e 休眠时距离汇报间隔*/
	100,                /*0x002f 紧急时距离汇报间隔*/
	270,                /*0x0030 拐点补传角度*/
	500,                /*0x0031 电子围栏半径（非法位移阈值），单位为米*/
	"1008611",          /*0x0040 监控平台电话号码*/
	"",                 /*0x0041 复位电话号码*/
	"",                 /*0x0042 恢复出厂设置电话号码*/
	"",                 /*0x0043 监控平台SMS号码*/
	"",                 /*0x0044 接收终端SMS文本报警号码*/
	5,                  /*0x0045 终端接听电话策略*/
	3,                  /*0x0046 每次通话时长*/
	3,                  /*0x0047 当月通话时长*/
	"",                 /*0x0048 监听电话号码*/
	"",                 /*0x0049 监管平台特权短信号码*/
	5,                  /*0x0050 报警屏蔽字*/
	3,                  /*0x0051 报警发送文本SMS开关*/
	5,                  /*0x0052 报警拍照开关*/
	3,                  /*0x0053 报警拍摄存储标志*/
	5,                  /*0x0054 关键标志*/
	3,                  /*0x0055 最高速度kmh*/
	5,                  /*0x0056 超速持续时间*/
	3,                  /*0x0057 连续驾驶时间门限*/
	5,                  /*0x0058 当天累计驾驶时间门限*/
	3,                  /*0x0059 最小休息时间*/
	5,                  /*0x005A 最长停车时间*/
	900,                /*0x0005B 超速报警预警差值，单位为 1/10Km/h */
	90,                 /*0x005C 疲劳驾驶预警差值，单位为秒（s），>0*/
	0x200a,             /*0x005D 碰撞报警参数设置:*/
	30,                 /*0x005E 侧翻报警参数设置： 侧翻角度，单位 1 度，默认为 30 度*/
	0,                  /*0x0064 定时拍照控制*/
	0,                  /*0x0065 定距拍照控制*/
	3,                  /*0x0070 图像视频质量(1-10)*/
	5,                  /*0x0071 亮度*/
	3,                  /*0x0072 对比度*/
	5,                  /*0x0073 饱和度*/
	3,                  /*0x0074 色度*/
	5,                  /*0x0080 车辆里程表读数0.1km*/
	3,                  /*0x0081 省域ID*/
	5,                  /*0x0082 市域ID*/
	"津O-00001",        /*0x0083 机动车号牌*/
	1,                  /*0x0084 车牌颜色  1蓝色 2黄色 3黑色 4白色 9其他*/
	0x0f,               /*0x0090 GNSS 定位模式*/
	0x01,               /*0x0091 GNSS 波特率*/
	0x01,               /*0x0092 GNSS 模块详细定位数据输出频率*/
	0x01,               /*0x0093    GNSS 模块详细定位数据采集频率*/
	0x01,               /*0x0094    GNSS 模块详细定位数据上传方式*/
	0x01,               /*0x0095 GNSS 模块详细定位数据上传设置*/
	0,                  /*0x0100 CAN 总线通道 1 采集时间间隔(ms)，0 表示不采集*/
	0,                  /*0x0101 CAN 总线通道 1 上传时间间隔(s)，0 表示不上传*/
	0,                  /*0x0102 CAN 总线通道 2 采集时间间隔(ms)，0 表示不采集*/
	0,                  /*0x0103 CAN 总线通道 2 上传时间间隔(s)，0 表示不上传*/
	{ 0, 0 },           /*0x0110 CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0111 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0112 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0113 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0114 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0115 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0116 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0117 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0118 其他CAN 总线 ID 单独采集设置*/
	{ 0, 0 },           /*0x0119 其他CAN 总线 ID 单独采集设置*/
};

TERM_PARAM term_param =
{
	{ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 },
	{ "TCBBD" },
	{ "TW701-BD" },
	{ 0x00, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee},
};

#define FLAG_DISABLE_REPORT_INVALID 1       /*设备非法*/
#define FLAG_DISABLE_REPORT_AREA	2       /*区域内禁止上报*/

static uint32_t flag_disable_report = 0;    /*禁止上报的标志位*/

/*保存参数到serialflash*/
void param_save( void )
{
	rt_kprintf( "parma_save size=%d\r\n", sizeof( jt808_param ) );
	sst25_write_back( ADDR_PARAM, (uint8_t*)&jt808_param, sizeof( jt808_param ) );
}

/*加载参数从serialflash*/
void param_load( void )
{
	/*预读一部分数据*/
	uint8_t		ver8[4];
	uint32_t	ver32;
	sst25_read( ADDR_PARAM, ver8, 4 );
	ver32 = ( ver8[0] ) | ( ver8[1] << 8 ) | ( ver8[2] << 16 ) | ( ver8[3]<<24 );
	rt_kprintf( "param_load ver=%08x\r\n", ver32 );
	if( jt808_param.id_0x0000 != ver32 ) /*不管是不是未初始化*/
	{
		rt_kprintf( "%s(%d)\r\n", __func__, __LINE__ );
		param_save( );
	}
	sst25_read( ADDR_PARAM, (uint8_t*)&jt808_param, sizeof( jt808_param ) );
}




#define TYPE_INT32	0x80		/*固定为4字节*/
#define TYPE_STR	0x40		/*固定为32字节*/
#define TYPE_CAN_ID 0x20		/*固定为8字节*/

struct _tbl_id_lookup
{
	uint16_t	id;
	uint8_t		type;
	uint8_t* val;
} tbl_id_lookup[] = {
	{0x0000,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0000)},	//uint32_t	id_0x0000;		/*0x0000 版本*/
	{0x0001,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0001)},	//uint32_t	id_0x0001;		/*0x0001 心跳发送间隔*/
	{0x0002,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0002)},	//uint32_t	id_0x0002;		/*0x0002 TCP应答超时时间*/
	{0x0003,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0003)},	//uint32_t	id_0x0003;		/*0x0003 TCP超时重传次数*/
	{0x0004,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0004)},	//uint32_t	id_0x0004;		/*0x0004 UDP应答超时时间*/
	{0x0005,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0005)},	//uint32_t	id_0x0005;		/*0x0005 UDP超时重传次数*/
	{0x0006,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0006)},	//uint32_t	id_0x0006;		/*0x0006 SMS消息应答超时时间*/
	{0x0007,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0007)},	//uint32_t	id_0x0007;		/*0x0007 SMS消息重传次数*/
	{0x0010,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0010)},	//char		id_0x0010[32];	/*0x0010 主服务器APN*/
	{0x0011,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0011)},	//char		id_0x0011[32];	/*0x0011 用户名*/
	{0x0012,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0012)},	//char		id_0x0012[32];	/*0x0012 密码*/
	{0x0013,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0013)},	//char		id_0x0013[32];	/*0x0013 主服务器地址*/
	{0x0014,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0014)},	//char		id_0x0014[32];	/*0x0014 备份APN*/
	{0x0015,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0015)},	//char		id_0x0015[32];	/*0x0015 备份用户名*/
	{0x0016,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0016)},	//char		id_0x0016[32];	/*0x0016 备份密码*/
	{0x0017,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0017)},	//char		id_0x0017[32];	/*0x0017 备份服务器地址，ip或域名*/
	{0x0018,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0018)},	//uint32_t	id_0x0018;		/*0x0018 TCP端口*/
	{0x0019,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0019)},	//uint32_t	id_0x0019;		/*0x0019 UDP端口*/
	{0x001A,TYPE_STR, (uint8_t*)&(jt808_param.id_0x001A)},	//char		id_0x001A[32];	/*0x001A ic卡主服务器地址，ip或域名*/
	{0x001B,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x001B)},	//uint32_t	id_0x001B;		/*0x001B ic卡服务器TCP端口*/
	{0x001C,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x001C)},	//uint32_t	id_0x001C;		/*0x001C ic卡服务器UDP端口*/
	{0x001D,TYPE_STR, (uint8_t*)&(jt808_param.id_0x001D)},	//char		id_0x001D[32];	/*0x001D ic卡备份服务器地址，ip或域名*/
	{0x0020,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0020)},	//uint32_t	id_0x0020;		/*0x0020 位置汇报策略*/
	{0x0021,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0021)},	//uint32_t	id_0x0021;		/*0x0021 位置汇报方案*/
	{0x0022,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0022)},	//uint32_t	id_0x0022;		/*0x0022 驾驶员未登录汇报时间间隔*/
	{0x0027,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0027)},	//uint32_t	id_0x0027;		/*0x0027 休眠时汇报时间间隔*/
	{0x0028,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0028)},	//uint32_t	id_0x0028;		/*0x0028 紧急报警时汇报时间间隔*/
	{0x0029,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0029)},	//uint32_t	id_0x0029;		/*0x0029 缺省时间汇报间隔*/
	{0x002C,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x002C)},	//uint32_t	id_0x002C;		/*0x002c 缺省距离汇报间隔*/
	{0x002D,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x002D)},	//uint32_t	id_0x002D;		/*0x002d 驾驶员未登录汇报距离间隔*/
	{0x002E,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x002E)},	//uint32_t	id_0x002E;		/*0x002e 休眠时距离汇报间隔*/
	{0x002F,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x002F)},	//uint32_t	id_0x002F;		/*0x002f 紧急时距离汇报间隔*/
	{0x0030,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0030)},	//uint32_t	id_0x0030;		/*0x0030 拐点补传角度*/
	{0x0031,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0031)},	//uint32_t	id_0x0031;		/*0x0031 电子围栏半径（非法位移阈值），单位为米*/
	{0x0040,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0040)},	//char		id_0x0040[32];	/*0x0040 监控平台电话号码*/
	{0x0041,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0041)},	//char		id_0x0041[32];	/*0x0041 复位电话号码*/
	{0x0042,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0042)},	//char		id_0x0042[32];	/*0x0042 恢复出厂设置电话号码*/
	{0x0043,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0043)},	//char		id_0x0043[32];	/*0x0043 监控平台SMS号码*/
	{0x0044,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0044)},	//char		id_0x0044[32];	/*0x0044 接收终端SMS文本报警号码*/
	{0x0045,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0045)},	//uint32_t	id_0x0045;		/*0x0045 终端接听电话策略*/
	{0x0046,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0046)},	//uint32_t	id_0x0046;		/*0x0046 每次通话时长*/
	{0x0047,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0047)},	//uint32_t	id_0x0047;		/*0x0047 当月通话时长*/
	{0x0048,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0048)},	//char		id_0x0048[32];	/*0x0048 监听电话号码*/
	{0x0049,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0049)},	//char		id_0x0049[32];	/*0x0049 监管平台特权短信号码*/
	{0x0050,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0050)},	//uint32_t	id_0x0050;		/*0x0050 报警屏蔽字*/
	{0x0051,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0051)},	//uint32_t	id_0x0051;		/*0x0051 报警发送文本SMS开关*/
	{0x0052,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0052)},	//uint32_t	id_0x0052;		/*0x0052 报警拍照开关*/
	{0x0053,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0053)},	//uint32_t	id_0x0053;		/*0x0053 报警拍摄存储标志*/
	{0x0054,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0054)},	//uint32_t	id_0x0054;		/*0x0054 关键标志*/
	{0x0055,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0055)},	//uint32_t	id_0x0055;		/*0x0055 最高速度kmh*/
	{0x0056,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0056)},	//uint32_t	id_0x0056;		/*0x0056 超速持续时间*/
	{0x0057,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0057)},	//uint32_t	id_0x0057;		/*0x0057 连续驾驶时间门限*/
	{0x0058,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0058)},	//uint32_t	id_0x0058;		/*0x0058 当天累计驾驶时间门限*/
	{0x0059,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0059)},	//uint32_t	id_0x0059;		/*0x0059 最小休息时间*/
	{0x005A,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x005A)},	//uint32_t	id_0x005A;		/*0x005A 最长停车时间*/
	{0x005B,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x005B)},	//uint32_t	id_0x005B;		/*0x005B 超速报警预警差值，单位为 1/10Km/h */
	{0x005C,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x005C)},	//uint32_t	id_0x005C;		/*0x005C 疲劳驾驶预警差值，单位为秒（s），>0*/
	{0x005D,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x005D)},	//uint32_t	id_0x005D;		/*0x005D 碰撞报警参数设置:b7..0：碰撞时间(4ms) b15..8：碰撞加速度(0.1g) 0-79 之间，默认为10 */
	{0x005E,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x005E)},	//uint32_t	id_0x005E;		/*0x005E 侧翻报警参数设置： 侧翻角度，单位 1 度，默认为 30 度*/
	{0x0064,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0064)},	//uint32_t	id_0x0064;		/*0x0064 定时拍照控制*/
	{0x0065,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0065)},	//uint32_t	id_0x0065;		/*0x0065 定距拍照控制*/
	{0x0070,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0070)},	//uint32_t	id_0x0070;		/*0x0070 图像视频质量(1-10)*/
	{0x0071,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0071)},	//uint32_t	id_0x0071;		/*0x0071 亮度*/
	{0x0072,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0072)},	//uint32_t	id_0x0072;		/*0x0072 对比度*/
	{0x0073,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0073)},	//uint32_t	id_0x0073;		/*0x0073 饱和度*/
	{0x0074,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0074)},	//uint32_t	id_0x0074;		/*0x0074 色度*/
	{0x0080,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0080)},	//uint32_t	id_0x0080;		/*0x0080 车辆里程表读数0.1km*/
	{0x0081,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0081)},	//uint32_t	id_0x0081;		/*0x0081 省域ID*/
	{0x0082,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0082)},	//uint32_t	id_0x0082;		/*0x0082 市域ID*/
	{0x0083,TYPE_STR, (uint8_t*)&(jt808_param.id_0x0083)},	//char		id_0x0083[32];	/*0x0083 机动车号牌*/
	{0x0084,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0084)},	//uint32_t	id_0x0084;		/*0x0084 车牌颜色  1蓝色 2黄色 3黑色 4白色 9其他*/
	{0x0090,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0090)},	//uint32_t	id_0x0090;		/*0x0090 GNSS 定位模式*/
	{0x0091,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0091)},	//uint32_t	id_0x0091;		/*0x0091 GNSS 波特率*/
	{0x0092,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0092)},	//uint32_t	id_0x0092;		/*0x0092 GNSS 模块详细定位数据输出频率*/
	{0x0093,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0093)},	//uint32_t	id_0x0093;		/*0x0093	GNSS 模块详细定位数据采集频率*/
	{0x0094,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0094)},	//uint32_t	id_0x0094;		/*0x0094	GNSS 模块详细定位数据上传方式*/
	{0x0095,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0095)},	//uint32_t	id_0x0095;		/*0x0095 GNSS 模块详细定位数据上传设置*/
	{0x0100,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0100)},	//uint32_t	id_0x0100;		/*0x0100 CAN 总线通道 1 采集时间间隔(ms)，0 表示不采集*/
	{0x0101,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0101)},	//uint32_t	id_0x0101;		/*0x0101 CAN 总线通道 1 上传时间间隔(s)，0 表示不上传*/
	{0x0102,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0102)},	//uint32_t	id_0x0102;		/*0x0102 CAN 总线通道 2 采集时间间隔(ms)，0 表示不采集*/
	{0x0103,TYPE_INT32, (uint8_t*)&(jt808_param.id_0x0103)},	//uint32_t	id_0x0103;		/*0x0103 CAN 总线通道 2 上传时间间隔(s)，0 表示不上传*/
	{0x0110,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0110)},	//uint32_t	id_0x110[2];	/*0x0110 CAN 总线 ID 单独采集设置*/
	{0x0111,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0111)},	//uint32_t	id_0x111[2];	/*0x0111 其他CAN 总线 ID 单独采集设置*/
	{0x0112,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0112)},	//uint32_t	id_0x112[2];	/*0x0112 其他CAN 总线 ID 单独采集设置*/
	{0x0113,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0113)},	//uint32_t	id_0x113[2];	/*0x0113 其他CAN 总线 ID 单独采集设置*/
	{0x0114,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0114)},	//uint32_t	id_0x114[2];	/*0x0114 其他CAN 总线 ID 单独采集设置*/
	{0x0115,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0115)},	//uint32_t	id_0x115[2];	/*0x0115 其他CAN 总线 ID 单独采集设置*/
	{0x0116,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0116)},	//uint32_t	id_0x116[2];	/*0x0116 其他CAN 总线 ID 单独采集设置*/
	{0x0117,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0117)},	//uint32_t	id_0x117[2];	/*0x0117 其他CAN 总线 ID 单独采集设置*/
	{0x0118,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0118)},	//uint32_t	id_0x118[2];	/*0x0118 其他CAN 总线 ID 单独采集设置*/
	{0x0119,TYPE_CAN_ID, (uint8_t*)&(jt808_param.id_0x0119)}	//uint32_t	id_0x119[2];	/*0x0119 其他CAN 总线 ID 单独采集设置*/
};                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             


void param_put( uint16_t id, uint8_t* value )
{
	int i,j;
	int len;
	uint8_t *psrc,*pdst;
	uint16_t len;
	
	for( i = 0; i < sizeof( tbl_id_lookup ) / sizeof( struct _tbl_id_lookup ); i++ )
	{
		if( id == tbl_id_lookup[i].id )
		{
			len=tbl_id_lookup[i].len;
			if(tbl_id_lookup[i].type==TYPE_INT32)
			{
				psrc=value;
				pdst=tbl_id_lookup[i].val;
				*pdst++=*(psrc+3);
				*pdst++=*(psrc+2);
				*pdst++=*(psrc+1);
				*pdst=*psrc;
				break;	
			}
			if(tbl_id_lookup[i].type==TYPE_CAN_ID)
			{
				psrc=value;
				pdst=tbl_id_lookup[i].val;
				*pdst++=*(psrc+3);
				*pdst++=*(psrc+2);
				*pdst++=*(psrc+1);
				*pdst++=*psrc;
				psrc=value+4;
				*pdst++=*(psrc+7);
				*pdst++=*(psrc+6);
				*pdst++=*(psrc+5);
				*pdst=*(psrc+4);
				break;	
			}
			if(tbl_id_lookup[i].type=TYPE_STR)
			{
				psrc=tbl_id_lookup[i].val;
				strncpy(psrc,value,);
				*(psrc+tbl_id_lookup[i].len)=0;
			}
		}
	}
}


/*设置参数*/
void param_put_int( uint16_t id, uint32_t value )
{
	int i,j;
	int len;
	uint8_t *p;
	
	for( i = 0; i < sizeof( tbl_id_lookup ) / sizeof( struct _tbl_id_lookup ); i++ )
	{
		if( id == tbl_id_lookup[i].id )
		{
			p=tbl_id_lookup[i].val;
			*p++=value>>24;
			*p++=value>>16;
			*p++=value>>8;
			*p=value&0xff;
			break;	
		}
	}
}

/*读取参数*/
uint32_t param_get_int( uint16_t id )
{
	int i;
	uint8_t *p;
	uint32_t val;
	for( i = 0; i < sizeof( tbl_id_lookup ) / sizeof( struct _tbl_id_lookup ); i++ )
	{
		if( id == tbl_id_lookup[i].id )
		{
			p=tbl_id_lookup[i].val;
			val|=(*p++);
			val|=((*p++)<<8);
			val|=((*p++)<<16);
			val|=((*p)<<24);
			return val;
		}
	}
}

void param_print(void)
{
	int i,j,id;
	int type;
	uint8_t *p;
	uint32_t val=0;
	
	for( i = 0; i < sizeof( tbl_id_lookup ) / sizeof( struct _tbl_id_lookup ); i++ )
	{
		id=tbl_id_lookup[i].id;
		p=tbl_id_lookup[i].val;
		val=0;
		switch(tbl_id_lookup[i].type)
		{
			case TYPE_INT32: /*字节对齐方式 little_endian*/
					val|=(*p++);
					val|=((*p++)<<8);
					val|=((*p++)<<16);
					val|=((*p)<<24);
					rt_kprintf("\r\nid=%04x len=%02d value=%08x\r\n",id,len,val);
			break;
			case TYPE_CAN_ID:
					val|=(*p++);
					val|=((*p++)<<8);
					val|=((*p++)<<16);
					val|=((*p++)<<24);
					rt_kprintf("\r\nid=%04x len=%02d value=%08x",id,len,val);
					val=0;
					val|=(*p++);
					val|=((*p++)<<8);
					val|=((*p++)<<16);
					val|=((*p)<<24);
					rt_kprintf(" %08x\r\n",val);
				break;
				
				break;
			case TYPE_STR:
				rt_kprintf("\r\nid=%04x len=%02d value=%s\r\n",id,len,p);
				break;				
		}
	}
}
FINSH_FUNCTION_EXPORT( param_print, print param );


/*打印参数信息*/
void param_dump( void )
{
	uint8_t tbl[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	int i, count = 0;
	uint8_t c;
	uint8_t *p = (uint8_t*)&jt808_param;
	uint8_t printbuf[70];
	int32_t len = sizeof( jt808_param );

	while( len > 0 )
	{
		count = ( len < 16 ) ? len : 16;
		memset( printbuf, 0x20, 70 );
		for( i = 0; i < count; i++ )
		{
			c					= *p;
			printbuf[i * 3]		= tbl[c >> 4];
			printbuf[i * 3 + 1] = tbl[c & 0x0f];
			if( c < 0x20 )
			{
				c = '.';
			}
			if( c > 0x7f )
			{
				c = '.';
			}
			printbuf[50 + i] = c;
			p++;
		}
		printbuf[69] = 0;
		rt_kprintf( "%s\r\n", printbuf );
		len -= count;
	}
}

FINSH_FUNCTION_EXPORT( param_dump, dump param );


/*
   $GNRMC,074001.00,A,3905.291037,N,11733.138255,E,0.1,,171212,,,A*655220.9*3F0E
   $GNTXT,01,01,01,ANTENNA OK*2B7,N,11733.138255,E,0.1,,171212,,,A*655220.9*3F0E
   $GNGGA,074002.00,3905.291085,N,11733.138264,E,1,11,0.9,8.2,M,-1.6,M,,,1.4*68E
   $GNGLL,3905.291085,N,11733.138264,E,074002.00,A,0*02.9,8.2,M,-1.6,M,,,1.4*68E
   $GPGSA,A,3,18,05,08,02,26,29,15,,,,,,,,,,,,,,,,,,,,,,,,,,1.6,0.9,1.4,0.9*3F8E
   $BDGSA,A,3,04,03,01,07,,,,,,,,,,,,,,,,,,,,,,,,,,,,,1.6,0.9,1.4,0.9*220.9*3F8E
   $GPGSV,2,1,7,18,10,278,29,05,51,063,08,21,052,24,02,24,140,45*4C220.9*3F8E
   $GPGSV,2,2,7,26,72,055,24,29,35,244,37,15,66,224,37*76,24,140,45*4C220.9*3F8E
   $BDGSV,1,1,4,04,27,124,38,03,42,190,34,01,38,146,37,07,34,173,35*55220.9*3F8E

   返回处理的字段数，如果正确的话
 */
uint8_t process_rmc( uint8_t * pinfo )
{
	//检查数据完整性,执行数据转换
	uint8_t year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0, fDateModify = 0;
	uint32_t degrees, minutes;
	uint8_t count;

	uint8_t gps_time[10];
	uint8_t gps_av	= 0;
	uint8_t gps_ns	= 0;
	uint8_t gps_ew	= 0;
	uint8_t gps_latitude[16];
	uint8_t gps_longitude[16];
	uint8_t gps_speed[8];
	uint8_t gps_direct[8];
	uint8_t gps_date[8];

	uint8_t  *psrc = pinfo + 7;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       //指向开始位置
/*时间处理 */
	count = 0;
	while( ( *psrc != ',' ) && ( count < 10 ) )
	{
		gps_time[count++]	= *psrc;
		gps_time[count]		= 0;
		psrc++;
	}
	if( ( count == 0 ) || ( count == 10 ) )
	{
		return 0;
	}
	hour	= ( gps_time[0] - 0x30 ) * 10 + ( gps_time[1] - 0x30 ) + 8;
	min		= ( gps_time[2] - 0x30 ) * 10 + ( gps_time[3] - 0x30 );
	sec		= ( gps_time[4] - 0x30 ) * 10 + ( gps_time[5] - 0x30 );
	if( hour > 23 )
	{
		fDateModify = 1;
		hour		-= 24;
	}
	gpsinfo.datetime[3] = ( ( hour / 10 ) << 4 ) | ( hour % 10 );
	gpsinfo.datetime[4] = ( ( min / 10 ) << 4 ) | ( min % 10 );
	gpsinfo.datetime[5] = ( ( sec / 10 ) << 4 ) | ( sec % 10 );
/*A_V处理*/
	psrc++;
	if( ( *psrc == 'A' ) || ( *psrc == 'V' ) )
	{
		gps_av = *psrc;
	} else
	{
		return 1;
	}
	if( gps_av == 'A' )
	{
		jt808_status &= ~0x01;
	} else
	{
		jt808_status |= 0x01;
	}
/*纬度处理ddmm.mmmmmm*/
	psrc++;
	count = 0;
	while( ( *psrc != ',' ) && ( count < 11 ) )
	{
		gps_latitude [count++]	= *psrc;
		gps_latitude [count]	= 0;
		psrc++;
	}
	if( count == 0 )
	{
		return 2;
	}
	degrees = ( ( gps_latitude [0] - 0x30 ) * 10 + ( gps_latitude [1] - 0x30 ) ) * 60 * 100000;
	minutes = ( gps_latitude [2] - 0x30 ) * 1000000 +
	          ( gps_latitude [3] - 0x30 ) * 100000 +
	          ( gps_latitude [5] - 0x30 ) * 10000 +
	          ( gps_latitude [6] - 0x30 ) * 1000 +
	          ( gps_latitude [7] - 0x30 ) * 100 +
	          ( gps_latitude [8] - 0x30 ) * 10 +
	          ( gps_latitude [9] - 0x30 );

	gpsinfo.latitude = ByteSwap4( degrees + minutes );

/*N_S处理*/
	psrc++;
	if( ( *psrc == 'N' ) || ( *psrc == 'S' ) )
	{
		gps_ns = *psrc;
	} else
	{
		return 3;
	}
	if( gps_ns == 'N' )
	{
		jt808_status &= ~0x02;
	} else
	{
		jt808_status |= 0x02;
	}

/*经度处理*/
	psrc++;
	count = 0;
	while( ( *psrc != ',' ) && ( count < 12 ) )
	{
		gps_longitude [count++] = *psrc;
		gps_longitude [count]	= 0;
		psrc++;
	}
	if( count == 0 )
	{
		return 4;
	}
	degrees = ( ( gps_latitude [0] - 0x30 ) * 100 + ( gps_latitude [1] - 0x30 ) * 10 + ( gps_latitude [2] - 0x30 ) ) * 60 * 100000;
	minutes = ( gps_latitude [3] - 0x30 ) * 1000000 +
	          ( gps_latitude [4] - 0x30 ) * 100000 +
	          ( gps_latitude [6] - 0x30 ) * 10000 +
	          ( gps_latitude [7] - 0x30 ) * 1000 +
	          ( gps_latitude [8] - 0x30 ) * 100 +
	          ( gps_latitude [9] - 0x30 ) * 10 +
	          ( gps_latitude [10] - 0x30 );
	gpsinfo.longitude = ByteSwap4( degrees + minutes );
/*N_S处理*/
	psrc++;
	if( ( *psrc == 'E' ) || ( *psrc == 'W' ) )
	{
		gps_ew = *psrc;
	} else
	{
		return 5;
	}
	if( gps_ew == 'E' )
	{
		jt808_status &= ~0x04;
	} else
	{
		jt808_status |= 0x04;
	}

/*速度处理*/
	psrc++;
	count = 0;
	while( ( *psrc != ',' ) && ( count < 7 ) )
	{
		gps_speed [count++] = *psrc;
		gps_speed [count]	= 0;
		psrc++;
	}
	if( count == 0 )
	{
		return 6;
	}

/*方向处理*/
	psrc++;
	count = 0;
	while( ( *psrc != ',' ) && ( count < 7 ) )
	{
		gps_direct [count++]	= *psrc;
		gps_direct [count]		= 0;
		psrc++;
	}
	if( count == 0 )
	{
		return 7;
	}

/*日期处理*/
	psrc++;
	count = 0;
	while( ( *psrc != ',' ) && ( count < 7 ) )
	{
		gps_date [count++]	= *psrc;
		gps_date [count]	= 0;
		psrc++;
	}
	if( count == 0 )
	{
		return 8;
	}
	day		= ( ( gps_date [0] - 0x30 ) * 10 ) + ( gps_date [1] - 0x30 );
	mon		= ( ( gps_date [2] - 0x30 ) * 10 ) + ( gps_date [3] - 0x30 );
	year	= ( ( gps_date [4] - 0x30 ) * 10 ) + ( gps_date [5] - 0x30 );

	if( fDateModify )
	{
		day++;
		if( mon == 2 )
		{
			if( ( year % 4 ) == 0 )
			{
				if( day == 30 )
				{
					day = 1; mon++;
				}
			} else
			if( day == 29 )
			{
				day = 1; mon++;
			}
		} else
		if( ( mon == 4 ) || ( mon == 6 ) || ( mon == 9 ) || ( mon == 11 ) )
		{
			if( day == 31 )
			{
				mon++; day = 1;
			}
		} else
		{
			if( day == 32 )
			{
				mon++; day = 1;
			}
			if( mon == 13 )
			{
				mon = 1; year++;
			}
		}
	}
	gpsinfo.datetime [0]	= ( ( year / 10 ) << 4 ) | ( year % 10 );
	gpsinfo.datetime [1]	= ( ( mon / 10 ) << 4 ) | ( mon % 10 );
	gpsinfo.datetime [2]	= ( ( day / 10 ) << 4 ) | ( day % 10 );
	return 0;
}

/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
void process_gga( uint8_t * pinfo )
{
}

/***********************************************************
* Function:
* Description:gps收到信息后的处理，头两个字节为长度
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
void gps_analy( uint8_t * pinfo )
{
	uint16_t len;
	uint8_t * psrc;
	len		= ( pinfo [0] << 8 ) | pinfo [1];
	psrc	= pinfo + 2;
	if( ( strncmp( psrc, "$GPRMC,", 7 ) == 0 ) || ( strncmp( psrc, "$BDRMC,", 7 ) == 0 ) || ( strncmp( psrc, "$GNRMC,", 7 ) == 0 ) )
	{
		process_rmc( psrc );
	}
	if( ( strncmp( psrc, "$GPGGA,", 7 ) == 0 ) || ( strncmp( psrc, "$BDGGA,", 7 ) == 0 ) || ( strncmp( psrc, "$GNGGA,", 7 ) == 0 ) )
	{
		process_gga( psrc );
	}
}

/*
   jt808格式数据解码判断
   <标识0x7e><消息头><消息体><校验码><标识0x7e>

   返回有效的数据长度,为0 表明有错

 */
static uint16_t jt808_decode_fcs( uint8_t * pinfo, uint16_t length )
{
	uint8_t * psrc, * pdst;
	uint16_t count, len;
	uint8_t fstuff	= 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                /*是否字节填充*/
	uint8_t fcs		= 0;

	if( length < 5 )
	{
		return 0;
	}
	if( *pinfo != 0x7e )
	{
		return 0;
	}
	if( *( pinfo + length - 1 ) != 0x7e )
	{
		return 0;
	}
	psrc	= pinfo + 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                /*1byte标识后为正式信息*/
	pdst	= pinfo;
	count	= 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        /*转义后的长度*/
	len		= length - 2;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               /*去掉标识位的数据长度*/

	while( len )
	{
		if( fstuff )
		{
			*pdst	= *psrc + 0x7c;
			fstuff	= 0;
			count++;
			fcs ^= *pdst;
		} else
		{
			if( *psrc == 0x7d )
			{
				fstuff = 1;
			} else
			{
				*pdst	= *psrc;
				fcs		^= *pdst;
				count++;
			}
		}
		psrc++;
		pdst++;
		len--;
	}
	if( fcs != 0 )
	{
		rt_kprintf( "%s>fcs error\r\n", __func__ );
		return 0;
	}
	rt_kprintf( "count=%d\r\n", count );
	return count;
}

/*
   jt808终端发送信息
   并将相关信息注册到接收信息的处理线程中
   需要传递消息ID,和消息体，由jt808_send线程完成
    消息的填充
    发送和重发机制
    流水号
    已发信息的回收free
   传递进来的格式
   <msgid 2bytes><msg_len 2bytes><msgbody nbytes>

 */
static void jt808_send( void * parameter )
{
}

/*发送后收到应答处理*/
void jt808_tx_response( JT808_RX_MSG_NODEDATA * nodedata )
{
	uint8_t * msg = nodedata->pmsg;
	uint16_t id;
	uint16_t seq;
	uint8_t res;

	seq = ( *msg << 8 ) | *( msg + 1 );
	id	= ( *( msg + 2 ) << 8 ) | *( msg + 3 );
	res = *( msg + 4 );

	switch( id )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    // 判断对应终端消息的ID做区分处理
	{
		case 0x0200:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                //	对应位置消息的应答
			rt_kprintf( "\r\nCentre ACK!\r\n" );
			break;
		case 0x0002:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                //	心跳包的应答
			rt_kprintf( "\r\n  Centre  Heart ACK!\r\n" );
			break;
		case 0x0101:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                //	终端注销应答
			break;
		case 0x0102:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                //	终端鉴权
			break;
		case 0x0800:                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                // 多媒体事件信息上传
			break;
		case 0x0702:
			rt_kprintf( "\r\n  驾驶员信息上报---中心应答!  \r\n" );
			break;
		case 0x0701:
			rt_kprintf( "\r\n	电子运单上报---中心应答!  \r\n");
			break;
		default:
			rt_kprintf( "\r\nunknown id=%04x\r\n", id );
			break;
	}
}

/*
   消息发送超时
 */
static rt_err_t jt808_tx_timeout( JT808_TX_MSG_NODEDATA * nodedata )
{
	rt_kprintf( "tx timeout\r\n" );
}

/*通用变参填充发送信息*/
static void handle_jt808_tx( const char * fmt, ... )
{
	uint8_t * pdata;
	JT808_TX_MSG_NODEDATA * pnodedata;
	uint8_t buf [512];
	uint8_t * p;
	uint8_t encode_len	= 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            /*编码后的长度*/
	uint8_t fcs			= 0;

	va_list args;
	rt_size_t length;

	pnodedata = rt_malloc( sizeof( JT808_TX_MSG_NODEDATA ) );
	if( pnodedata == NULL )
	{
		return;
	}

	pnodedata->type		= TERMINAL_ACK;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   /*发送即删除，其他不用配置*/
	pnodedata->state	= IDLE;

	va_start( args, fmt );
	length = vsnprintf( buf, sizeof( buf ), fmt, args );
	va_end( args );

	tx_seq++;
}

/*
   终端通用应答
 */
static void handle_jt808_tx_0x0001( uint16_t seq, uint16_t id, uint8_t res )
{
	uint8_t * pdata;
	JT808_TX_MSG_NODEDATA * pnodedata;
	uint8_t buf [64];
	uint8_t * p;
	uint16_t len;

	pnodedata = rt_malloc( sizeof( JT808_TX_MSG_NODEDATA ) );
	if( pnodedata == NULL )
	{
		return;
	}

	pnodedata->type		= TERMINAL_ACK;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               /*发送即删除，其他不用配置*/
	pnodedata->state	= IDLE;

	len		= jt808_pack( buf, "\x00\x01\x00\x05%6s%w%w%w%b", term_param.mobile, tx_seq, seq, id, res );
	pdata	= rt_malloc( len );
	if( pdata == NULL )
	{
		rt_free( pnodedata );
		return;
	}
	memcpy( pdata, buf, len );
	pnodedata->msg_len	= len;
	pnodedata->pmsg		= pdata;
	msglist_append( list_jt808_tx, pnodedata );
	tx_seq++;
}

/*平台通用应答,收到信息，停止发送*/
static int handle_jt808_rx_0x8001( JT808_RX_MSG_NODEDATA * nodedata )
{
	MsgListNode * iter;
	JT808_TX_MSG_NODEDATA * iterdata;
	MsgListNode * iter_tmp;

	uint8_t * msg = nodedata->pmsg;
	uint16_t id;
	uint16_t seq;
	uint8_t res;

	seq = ( *msg << 8 ) | *( msg + 1 );
	id	= ( *( msg + 2 ) << 8 ) | *( msg + 3 );
	res = *( msg + 4 );

	/*逐条处理*/
	iter = list_jt808_tx->first;
	if( ( iterdata->head_id == id ) && ( iterdata->head_sn == seq ) )
	{
		iterdata->cb_tx_response( nodedata );
		iterdata->state = ACK_OK;
	}
}

/* 监控中心对终端注册消息的应答*/
static int handle_jt808_rx_0x8100( JT808_RX_MSG_NODEDATA * nodedata )
{
	MsgListNode * iter;
	JT808_TX_MSG_NODEDATA * iterdata;

	uint16_t ack_seq;
	uint8_t res;
	uint8_t * msg;

	msg		= nodedata->pmsg;
	ack_seq = ( *msg << 8 ) | *( msg + 1 );
	res		= *( msg + 2 );

	iter		= list_jt808_tx->first;
	iterdata	= iter->data;
	if( ( iterdata->head_id == 0x0100 ) && ( iterdata->head_sn == ack_seq ) )
	{
		rt_kprintf( "\r\n%s(%d)>res=%d\r\n", __func__, __LINE__, res );
		if( res == 0 )
		{
			strncpy( term_param.register_code, msg + 3, nodedata->msg_len );
			iterdata->state = ACK_OK;
		}
	}
	return 1;
}

/*设置终端参数*/
static int handle_jt808_rx_0x8103( JT808_RX_MSG_NODEDATA * nodedata )
{
	uint8_t * msg;
	uint8_t * p;

	uint8_t param_count;

	uint16_t msg_len, count = 0;

	uint32_t id;
	uint8_t len;

	msg			= nodedata->pmsg;
	msg_len		= nodedata->msg_len - 1;
	param_count = *msg;
	p			= msg + 1;
/*使用数据长度,判断数据是否结束，没有使用参数总数*/
	while( count < msg_len )
	{
		id		= ( ( *p++ ) << 24 ) | ( ( *p++ ) << 16 ) | ( ( *p++ ) << 8 ) | ( *p++ );
		len		= *p++;
		count	+= ( 5 + len );
		param_put(id,p);
	}
	return 1;
}

/*查询终端参数*/
static int handle_jt808_rx_0x8104( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/*终端控制*/
static int handle_jt808_rx_0x8105( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8201( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8202( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8300( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8301( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8302( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8303( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8304( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8400( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8401( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8500( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8600( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8601( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8602( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8603( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8604( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8605( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8606( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8607( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8700( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8701( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8800( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8801( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8802( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8803( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8804( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8805( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8900( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/**/
static int handle_jt808_rx_0x8A00( JT808_RX_MSG_NODEDATA * nodedata )
{
	return 1;
}

/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
static int handle_jt808_rx_default( JT808_RX_MSG_NODEDATA * nodedata )
{
	rt_kprintf( "\r\nunknown!\r\n" );
	return 1;
}

#define DECL_JT808_RX_HANDLE( a )	{ a, handle_jt808_rx_ ## a }
#define DECL_JT808_TX_HANDLE( a )	{ a, handle_jt808_tx_ ## a }

HANDLE_JT808_RX_MSG handle_jt808_rx_msg[] =
{
	DECL_JT808_RX_HANDLE( 0x8001 ),                                                                                                                                                                                                                 //	通用应答
	DECL_JT808_RX_HANDLE( 0x8100 ),                                                                                                                                                                                                                 //  监控中心对终端注册消息的应答
	DECL_JT808_RX_HANDLE( 0x8103 ),                                                                                                                                                                                                                 //	设置终端参数
	DECL_JT808_RX_HANDLE( 0x8104 ),                                                                                                                                                                                                                 //	查询终端参数
	DECL_JT808_RX_HANDLE( 0x8105 ),                                                                                                                                                                                                                 // 终端控制
	DECL_JT808_RX_HANDLE( 0x8201 ),                                                                                                                                                                                                                 // 位置信息查询    位置信息查询消息体为空
	DECL_JT808_RX_HANDLE( 0x8202 ),                                                                                                                                                                                                                 // 临时位置跟踪控制
	DECL_JT808_RX_HANDLE( 0x8300 ),                                                                                                                                                                                                                 //	文本信息下发
	DECL_JT808_RX_HANDLE( 0x8301 ),                                                                                                                                                                                                                 //	事件设置
	DECL_JT808_RX_HANDLE( 0x8302 ),                                                                                                                                                                                                                 // 提问下发
	DECL_JT808_RX_HANDLE( 0x8303 ),                                                                                                                                                                                                                 //	信息点播菜单设置
	DECL_JT808_RX_HANDLE( 0x8304 ),                                                                                                                                                                                                                 //	信息服务
	DECL_JT808_RX_HANDLE( 0x8400 ),                                                                                                                                                                                                                 //	电话回拨
	DECL_JT808_RX_HANDLE( 0x8401 ),                                                                                                                                                                                                                 //	设置电话本
	DECL_JT808_RX_HANDLE( 0x8500 ),                                                                                                                                                                                                                 //	车辆控制
	DECL_JT808_RX_HANDLE( 0x8600 ),                                                                                                                                                                                                                 //	设置圆形区域
	DECL_JT808_RX_HANDLE( 0x8601 ),                                                                                                                                                                                                                 //	删除圆形区域
	DECL_JT808_RX_HANDLE( 0x8602 ),                                                                                                                                                                                                                 //	设置矩形区域
	DECL_JT808_RX_HANDLE( 0x8603 ),                                                                                                                                                                                                                 //	删除矩形区域
	DECL_JT808_RX_HANDLE( 0x8604 ),                                                                                                                                                                                                                 //	多边形区域
	DECL_JT808_RX_HANDLE( 0x8605 ),                                                                                                                                                                                                                 //	删除多边区域
	DECL_JT808_RX_HANDLE( 0x8606 ),                                                                                                                                                                                                                 //	设置路线
	DECL_JT808_RX_HANDLE( 0x8607 ),                                                                                                                                                                                                                 //	删除路线
	DECL_JT808_RX_HANDLE( 0x8700 ),                                                                                                                                                                                                                 //	行车记录仪数据采集命令
	DECL_JT808_RX_HANDLE( 0x8701 ),                                                                                                                                                                                                                 //	行驶记录仪参数下传命令
	DECL_JT808_RX_HANDLE( 0x8800 ),                                                                                                                                                                                                                 //	多媒体数据上传应答
	DECL_JT808_RX_HANDLE( 0x8801 ),                                                                                                                                                                                                                 //	摄像头立即拍照
	DECL_JT808_RX_HANDLE( 0x8802 ),                                                                                                                                                                                                                 //	存储多媒体数据检索
	DECL_JT808_RX_HANDLE( 0x8803 ),                                                                                                                                                                                                                 //	存储多媒体数据上传命令
	DECL_JT808_RX_HANDLE( 0x8804 ),                                                                                                                                                                                                                 //	录音开始命令
	DECL_JT808_RX_HANDLE( 0x8805 ),                                                                                                                                                                                                                 //	单条存储多媒体数据检索上传命令 ---- 补充协议要求
	DECL_JT808_RX_HANDLE( 0x8900 ),                                                                                                                                                                                                                 //	数据下行透传
	DECL_JT808_RX_HANDLE( 0x8A00 ),                                                                                                                                                                                                                 //	平台RSA公钥
};


/*
   接收处理
   分析jt808格式的数据
   <linkno><长度2byte><标识0x7e><消息头><消息体><校验码><标识0x7e>

 */
uint16_t jt808_rx_proc( uint8_t * pinfo )
{
	uint8_t * psrc;
	uint16_t len;
	uint8_t linkno;
	uint16_t i;
	uint8_t flag_find = 0;

	uint16_t ret;

	MsgListNode * node;
	JT808_RX_MSG_NODEDATA * nodedata;

	MsgListNode * iter;
	JT808_RX_MSG_NODEDATA * iterdata;

	MsgListNode * iter_tmp;
	JT808_RX_MSG_NODEDATA * iterdata_tmp;

	linkno	= pinfo [0];
	len		= ( pinfo [1] << 8 ) | pinfo [2];
	rt_kprintf( ">dump start len=%d\r\n", len );
	psrc = pinfo + 3;
	for( i = 0; i < len; i++ )
	{
		rt_kprintf( "%02x ", *psrc++ );
	}
	rt_kprintf( "\r\n>dump end\r\n" );

	len = jt808_decode_fcs( pinfo + 3, len );
	if( len == 0 )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  /*格式不正确*/
	{
		rt_kprintf( ">len=0\r\n" );
		rt_free( pinfo );
		return 1;
	}

/*对收到的信息进行解析,此时以解码转义到pinfo+3*/
	nodedata = rt_malloc( sizeof( JT808_RX_MSG_NODEDATA ) );
	if( nodedata == RT_NULL )                                                                                                                                                                                                               /*无法处理此信息*/
	{
		rt_free( pinfo );
		return 1;
	}

	psrc				= pinfo;                                                                                                                                                                                                            /*注意开始的linkno len*/
	nodedata->linkno	= linkno;
	nodedata->id		= ( *( psrc + 3 ) << 8 ) | *( psrc + 4 );
	nodedata->attr		= ( *( psrc + 5 ) << 8 ) | *( psrc + 6 );
	memcpy( nodedata->mobileno, psrc + 7, 6 );
	nodedata->seq		= ( *( psrc + 13 ) << 8 ) | *( psrc + 14 );
	nodedata->msg_len	= nodedata->attr & 0x3ff;                                                                                                                                                                                           /*有效的信息长度在attr字段指示*/
	nodedata->tick		= rt_tick_get( );                                                                                                                                                                                                   /*收到的时刻*/

/* 单包数据处理,不需要创建MsgNode */
	if( ( nodedata->attr & 0x2000 ) == 0 )
	{
		nodedata->pmsg = psrc + 15;                                                                                                                                                                                                         /*消息体开始位置*/
		for( i = 0; i < sizeof( handle_jt808_rx_msg ) / sizeof( HANDLE_JT808_RX_MSG ); i++ )
		{
			if( nodedata->id == handle_jt808_rx_msg [i].id )
			{
				handle_jt808_rx_msg [i].func( nodedata );
				flag_find = 1;
			}
		}
		if( !flag_find )
		{
			handle_jt808_rx_default( nodedata );
		}
		rt_free( pinfo );
		rt_free( nodedata );
	}


/*检查是否有超时没有处理的信息，主要是多包信息
   收到消息才会处理，如果长时间没有收到，占用内存
 */

	iter		= list_jt808_rx->first;
	flag_find	= 0;
	while( iter != NULL )
	{
		iterdata = iter->data;
		if( rt_tick_get( ) - iterdata->tick > RT_TICK_PER_SECOND * 10 )                                                                                                                                                                                         /*超过10秒没有数据包*/
		{
			/*整理一下，准备删除该节点串*/
			if( iter->prev == NULL )                                                                                                                                                                                                                            /*队首*/
			{
				list_jt808_rx->first = iter->next;
			} else
			{
				iter->prev->next = iter->next;
				if( iter->next != RT_NULL )
				{
					iter->next->prev = iter->prev;
				}
			}

			while( iter != NULL )
			{
				iter_tmp	= iter->next;
				iterdata	= iter->data;
				rt_free( iterdata->pmsg );
				rt_free( iterdata );
				rt_free( iter );
				iter = iter_tmp;
			}
		}
	}
/*不是多包,返回*/
	if( ( nodedata->attr & 0x2000 ) == 0 )
	{
		return 0;
	}

/*分包处理,创建新的节点*/
	node = msglist_node_create( ( void* )nodedata );
	if( node == RT_NULL )
	{
		rt_free( nodedata );
		rt_free( pinfo );
		return 1;
	}
	nodedata->packetcount	= ( *( psrc + 12 ) << 8 ) | *( psrc + 13 );
	nodedata->packetno		= ( *( psrc + 14 ) << 8 ) | *( psrc + 15 );
/*看是不是第一个分包*/
	flag_find	= 0;
	iter		= list_jt808_rx->first;
	while( iter != NULL )
	{
		iterdata = ( JT808_RX_MSG_NODEDATA* )( iter->data );
		if( iterdata->id == nodedata->id )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       /*判断的消息ID一致,即便已有分包，但ID不一致，也认为是新包*/
		{
			flag_find = 1;
			break;
		}
		iter = iter->next;
	}
/*找到，不是第一个分包，要对已有的分包排序*/
	if( flag_find )                                                                                                                                                                         /*找到了 iter，开始遍历sibling并插入*/
	{
		if( iterdata->packetno < nodedata->packetno )                                                                                                                                       /*在主干上,改变主干上的节点*/
		{
			node->prev			= iter->prev;
			node->next			= iter->next;                                                                                                                                               /*替换原来的位置*/
			node->sibling_dn	= iter;
			iter->sibling_up	= node;
		} else                                                                                                                                                                              /*在多包信息的分支上*/
		{
			flag_find = 0;
			while( iter->sibling_dn != NULL )
			{
				iter		= iter->sibling_dn;
				iterdata	= iter->data;
				if( iterdata->packetno < nodedata->packetno )
				{
					node->sibling_up	= iter->sibling_up;
					node->sibling_dn	= iter;
					iter->sibling_up	= node;
					flag_find			= 1;
					break;
				}
			}
			if( flag_find == 0 )                                                                                                                                                                /*结尾也没有找到*/
			{
				node->sibling_up	= iter;
				iter->sibling_dn	= node;
			}
		}
	} else                                                                                                                                                                                      /*没找到，没有使用msglist_append(list_jt808_rx,nodedata);*/
	{
		if( list_jt808_rx->first == NULL )                                                                                                                                                      /*是第一个节点*/
		{
			list_jt808_rx->first->data = nodedata;
		} else                                                                                                                                                                                  /*已有节点，添加到最后*/
		{
			iter		= iter->prev;                                                                                                                                                           /*此时iter为NULL,应指向前一个有效node*/
			iter->next	= node;
			node->prev	= iter;
		}
	}
}

/*
   处理每个要发送信息的状态
   现在允许并行处理吗?
 */

static MsgListRet jt808_tx_proc( MsgListNode * node )
{
	MsgListNode * pnode					= ( MsgListNode* )node;
	JT808_TX_MSG_NODEDATA * pnodedata	= ( JT808_TX_MSG_NODEDATA* )( pnode->data );
	int i;

	if( node == RT_NULL )
	{
		return MSGLIST_RET_OK;
	}

	if( pnodedata->state == IDLE )                                                                                                                                                                                                                  /*空闲，发送信息或超时后没有数据*/
	{
		if( pnodedata->retry >= jt808_param.id_0x0003 )                                                                                                                                                                                             /*已经达到重试次数*/
		{
			/*表示发送失败*/
			pnodedata->cb_tx_timeout( pnodedata );                                                                                                                                                                                                  /*调用发送失败处理函数*/
			return MSGLIST_RET_DELETE_NODE;
		} else
		{
			//rt_device_write( pdev_gsm, 0, pnodedata->pmsg, pnodedata->msg_len );
			gsm_ipsend( pnodedata->pmsg, pnodedata->msg_len, jt808_param.id_0x0002 * RT_TICK_PER_SECOND );
			pnodedata->tick = rt_tick_get( );
			pnodedata->retry++;
			pnodedata->timeout	= pnodedata->retry * jt808_param.id_0x0002 * RT_TICK_PER_SECOND;
			pnodedata->state	= WAIT_ACK;
			rt_kprintf( "send retry=%d,timeout=%d\r\n", pnodedata->retry, pnodedata->timeout * 10 );
		}
		return MSGLIST_RET_OK;
	}

	if( pnodedata->state == WAIT_ACK )
	{
		if( rt_tick_get( ) - pnodedata->tick > pnodedata->timeout )
		{
			pnodedata->state = IDLE;
		}
	}

	if( pnodedata->state == ACK_OK )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    /*收到ACK，在接收中置位*/
	{
		//pnodedata->cb_tx_response( pnodedata );
		return MSGLIST_RET_DELETE_NODE;
	}

	return MSGLIST_RET_OK;
}

/*
   判断一个字符串是不是表示ip的str
   如果由[0..9|.] 组成

   '.' 0x2e
   '/' 0x2f
   '0' 0x30
   '9' 0x39
   简化一下。不判断 '/'
 */
static uint8_t is_ipstr( char * str )
{
	char * p = str;
	while( *p != NULL )
	{
		if( ( *p > '9' ) || ( *p < '.' ) )
		{
			return 0;
		}
		p++;
	}
	return 1;
}

/*jt808的socket管理

   维护链路。会有不同的原因
   上报状态的维护
   1.尚未登网
   2.中心连接，DNS,超时不应答
   3.禁止上报，关闭模块的区域
   4.当前正在进行空中更新，多媒体上报等不需要打断的工作

 */
static void jt808_socket_proc( void )
{
	volatile uint32_t state;
	static rt_tick_t lasttick;
	static uint8_t flag_connect = 0;

	if( flag_disable_report )
	{
		return;
	}
/*检查GSM状态*/
	state = config_gsmstate( 0 );
	if( state == GSM_IDLE )
	{
		if( flag_connect == 0 )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              /*连接主server*/
		{
			curr_gsm_socket.apn		= jt808_param.id_0x0010;
			curr_gsm_socket.user	= jt808_param.id_0x0011;
			curr_gsm_socket.psw		= jt808_param.id_0x0012;
			curr_gsm_socket.type	= 't';
			if( is_ipstr( jt808_param.id_0x0013 ) )
			{
				strcpy( curr_gsm_socket.ip_str, jt808_param.id_0x0013 );
			} else
			{
				curr_gsm_socket.ip_domain = jt808_param.id_0x0013;
			}
			curr_gsm_socket.port = jt808_param.id_0x0018;
			rt_kprintf( "\r\napn=%s\r\n", curr_gsm_socket.apn );
		} else
		{
			curr_gsm_socket.apn		= jt808_param.id_0x0014;
			curr_gsm_socket.user	= jt808_param.id_0x0015;
			curr_gsm_socket.psw		= jt808_param.id_0x0016;
			curr_gsm_socket.type	= 't';
			if( is_ipstr( jt808_param.id_0x0017 ) )
			{
				strcpy( curr_gsm_socket.ip_str, jt808_param.id_0x0017 );
			} else
			{
				curr_gsm_socket.ip_domain = jt808_param.id_0x0017;
			}

			curr_gsm_socket.port = jt808_param.id_0x0018;
		}
		curr_gsm_socket.state = SOCKET_IDLE;
		config_gsmstate( GSM_POWERON );
		return;
	}
	if( state == GSM_POWERON )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  /*检查超时*/
	{
	}
	if( state != GSM_AT )
	{
		return;
	}

/*检查socket状态,判断用不用DNS*/
	switch( curr_gsm_socket.state )
	{
		case SOCKET_IDLE:
			if( flag_connect == 0 )
			{
				if( is_ipstr( jt808_param.id_0x0013 ) )
				{
					curr_gsm_socket.state = SOCKET_CONNECT;
				} else
				{
					curr_gsm_socket.state = SOCKET_DNS;
				}
			} else
			{
				if( is_ipstr( jt808_param.id_0x0017 ) )
				{
					curr_gsm_socket.state = SOCKET_CONNECT;
				} else
				{
					curr_gsm_socket.state = SOCKET_DNS;
				}
			}
			break;
		case SOCKET_CONNECT_ERR:
		case SOCKET_DNS_ERR:
			flag_connect++;
			if( flag_connect == 2 )                                                                                                                                                                                             /*是否要关闭设备，重新启动链接*/
			{
				flag_connect = 0;
			}
			config_gsmstate( GSM_POWEROFF );                                                                                                                                                                                    /*关闭gsm等待重连*/
			break;
	}
	if( state != SOCKET_READY )
	{
		return;
	}
/*是否发送数据*/
}

#define offsetof( TYPE, MEMBER ) ( (size_t)&( (TYPE*)0 )->MEMBER )


/*
   连接状态维护
   jt808协议处理

 */
ALIGN( RT_ALIGN_SIZE )
static char thread_jt808_stack [4096];
struct rt_thread thread_jt808;

/***/
static void rt_thread_entry_jt808( void * parameter )
{
	rt_err_t ret;
	int i;
	uint8_t * pstr;

	MsgListNode * iter;
	MsgListNode * iter_next;
	JT808_TX_MSG_NODEDATA * pnodedata;


	PT_INIT( &pt_jt808_socket );

/*读取参数，并配置*/
	param_load( );

	list_jt808_tx	= msglist_create( );
	list_jt808_rx	= msglist_create( );

	pdev_gsm = rt_device_find( "gsm" );                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          /*没有出错处理,未找到怎么办*/

	while( 1 )
	{
/*接收gps信息*/
		ret = rt_mb_recv( &mb_gpsdata, ( rt_uint32_t* )&pstr, 5 );
		if( ret == RT_EOK )
		{
			gps_analy( pstr );
			rt_free( pstr );
		}
/*接收gprs信息*/
		ret = rt_mb_recv( &mb_gprsdata, ( rt_uint32_t* )&pstr, 5 );
		if( ret == RT_EOK )
		{
			jt808_rx_proc( pstr );
		}
/*jt808 socket处理*/
		jt808_socket_proc( );
/*逐条处理*/
		iter = list_jt808_tx->first;
		if( jt808_tx_proc( iter ) == MSGLIST_RET_DELETE_NODE )                                                                                                                              /*删除该节点*/
		{
			pnodedata = ( JT808_TX_MSG_NODEDATA* )( iter->data );
			rt_free( pnodedata->pmsg );                                                                                                                                                     /*删除用户数据*/
			rt_free( pnodedata );                                                                                                                                                           /*删除节点数据*/
			list_jt808_tx->first = iter->next;                                                                                                                                              /*指向下一个*/
			rt_free( iter );
		}

		rt_thread_delay( RT_TICK_PER_SECOND / 20 );
	}

	msglist_destroy( list_jt808_tx );
}

/*jt808处理线程初始化*/
void jt808_init( void )
{
	rt_mb_init( &mb_gprsdata, "gprsdata", &mb_gprsdata_pool, MB_GPRSDATA_POOL_SIZE / 4, RT_IPC_FLAG_FIFO );
	rt_mb_init( &mb_gpsdata, "gpsdata", &mb_gpsdata_pool, MB_GPSDATA_POOL_SIZE / 4, RT_IPC_FLAG_FIFO );

	rt_thread_init( &thread_jt808,
	                "jt808",
	                rt_thread_entry_jt808,
	                RT_NULL,
	                &thread_jt808_stack [0],
	                sizeof( thread_jt808_stack ), 10, 5 );
	rt_thread_startup( &thread_jt808 );
}

/*gps接收处理*/
void gps_rx( uint8_t * pinfo, uint16_t length )
{
	uint8_t * pmsg;
	pmsg = rt_malloc( length + 2 );
	if( pmsg != RT_NULL )
	{
		pmsg [0]	= length >> 8;
		pmsg [1]	= length & 0xff;
		memcpy( pmsg + 2, pinfo, length );
		rt_mb_send( &mb_gpsdata, ( rt_uint32_t )pmsg );
	}
}

/*gprs接收处理,收到数据要尽快处理*/
rt_err_t gprs_rx( uint8_t linkno, uint8_t * pinfo, uint16_t length )
{
	uint8_t * pmsg;
	pmsg = rt_malloc( length + 3 );                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              /*包含长度信息*/
	if( pmsg != RT_NULL )
	{
		pmsg [0]	= linkno;
		pmsg [1]	= length >> 8;
		pmsg [2]	= length & 0xff;
		memcpy( pmsg + 3, pinfo, length );
		rt_mb_send( &mb_gprsdata, ( rt_uint32_t )pmsg );
		return 0;
	}
	return 1;
}

FINSH_FUNCTION_EXPORT( gprs_rx, simlute gprs rx );


/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
rt_err_t gprs_tx( uint8_t linkno, uint8_t * pinfo, uint16_t length )
{
	uint8_t * pmsg;
	pmsg = rt_malloc( length + 3 );                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      /*包含长度信息*/
	if( pmsg != RT_NULL )
	{
		pmsg [0]	= linkno;
		pmsg [1]	= length >> 8;
		pmsg [2]	= length & 0xff;
		memcpy( pmsg + 3, pinfo, length );
		rt_mb_send( &mb_gprsdata, ( rt_uint32_t )pmsg );
		return 0;
	}
	return 1;
}

FINSH_FUNCTION_EXPORT( gprs_tx, simlute gprs tx );


/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
rt_err_t server( uint8_t cmd, char * apn, char * name, char * psw )
{
	if( cmd == 0 )                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   /*打印参数*/
	{
		rt_kprintf( "\r\napn=%s user=%s psw=%s ip_domain=%s tcp_port=%d udp_port=%d",
		            jt808_param.id_0x0010,
		            jt808_param.id_0x0011,
		            jt808_param.id_0x0012,
		            jt808_param.id_0x0013,
		            jt808_param.id_0x0018,
		            jt808_param.id_0x0019
		            );
		rt_kprintf( "\r\napn=%s user=%s psw=%s ip_domain=%s tcp_port=%d udp_port=%d\r\n",
		            jt808_param.id_0x0014,
		            jt808_param.id_0x0015,
		            jt808_param.id_0x0016,
		            jt808_param.id_0x0017,
		            jt808_param.id_0x0018,
		            jt808_param.id_0x0019
		            );
	}
	if( cmd == 1 )                                                                                                                                          /*设置主server*/
	{
	}
	if( cmd == 2 )                                                                                                                                          /*设置备份server*/
	{
	}
	return RT_EOK;
}

FINSH_FUNCTION_EXPORT( server, config server );


/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
static uint16_t jt808_pack_byte( uint8_t * buf, uint8_t * fcs, uint8_t data )
{
	uint8_t * p = buf;
	*fcs ^= data;
	if( ( data == 0x7d ) || ( data == 0x7e ) )
	{
		*p++	= 0x7d;
		*p		= ( data - 0x7c );
		return 2;
	} else
	{
		*p = data;
		return 1;
	}
}

/*
   static uint16_t jt808_pack_word(uint8_t *buf,uint8_t *fcs,uint16_t data)
   {
   uint16_t count=0;
   count+=jt808_pack_byte(buf+count,fcs,(data>>8));
   count+=jt808_pack_byte(buf+count,fcs,(data&0xff));
   return count;
   }

   static uint16_t jt808_pack_dword(uint8_t *buf,uint8_t *fcs,uint32_t data)
   {
   uint16_t count=0;
   count+=jt808_pack_byte(buf+count,fcs,(data>>24));
   count+=jt808_pack_byte(buf+count,fcs,(data>>16));
   count+=jt808_pack_byte(buf+count,fcs,(data>>8));
   count+=jt808_pack_byte(buf+count,fcs,(data&0xff));
   return count;
   }
 */

/*传递进长度，便于计算*/
static uint16_t jt808_pack_int( uint8_t * buf, uint8_t * fcs, uint32_t data, uint8_t width )
{
	uint16_t count = 0;
	switch( width )
	{
		case 1:
			count += jt808_pack_byte( buf + count, fcs, ( data & 0xff ) );
			break;
		case 2:
			count	+= jt808_pack_byte( buf + count, fcs, ( data >> 8 ) );
			count	+= jt808_pack_byte( buf + count, fcs, ( data & 0xff ) );
			break;
		case 4:
			count	+= jt808_pack_byte( buf + count, fcs, ( data >> 24 ) );
			count	+= jt808_pack_byte( buf + count, fcs, ( data >> 16 ) );
			count	+= jt808_pack_byte( buf + count, fcs, ( data >> 8 ) );
			count	+= jt808_pack_byte( buf + count, fcs, ( data & 0xff ) );
			break;
	}
	return count;
}

/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
static uint16_t jt808_pack_string( uint8_t * buf, uint8_t * fcs, char * str )
{
	uint16_t count	= 0;
	char * p		= str;
	while( *p )
	{
		count += jt808_pack_byte( buf + count, fcs, *p++ );
	}
	return count;
}

/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
static uint16_t jt808_pack_array( uint8_t * buf, uint8_t * fcs, uint8_t * src, uint16_t len )
{
	uint16_t count = 0;
	int i;
	char * p = src;
	for( i = 0; i < len; i++ )
	{
		count += jt808_pack_byte( buf + count, fcs, *p++ );
	}
	return count;
}

/***********************************************************
* Function:
* Description:
* Input:
* Input:
* Output:
* Return:
* Others:
***********************************************************/
static rt_err_t jt808_tx( void )
{
	uint8_t * pdata;
	JT808_TX_MSG_NODEDATA * pnodedata;
	uint8_t buf [256];
	uint8_t * p;
	uint16_t len;
	uint8_t fcs = 0;
	int i;

	pnodedata = rt_malloc( sizeof( JT808_TX_MSG_NODEDATA ) );
	if( pnodedata == NULL )
	{
		return -1;
	}
	pnodedata->type				= TERMINAL_CMD;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             /*发送即删除，其他不用配置*/
	pnodedata->state			= IDLE;
	pnodedata->retry			= 0;
	pnodedata->cb_tx_timeout	= jt808_tx_timeout;
	pnodedata->cb_tx_response	= jt808_tx_response;

	len = 1;
	len += jt808_pack_int( buf + len, &fcs, 0x0100, 2 );
	len += jt808_pack_int( buf + len, &fcs, 37 + strlen( jt808_param.id_0x0083 ), 2 );
	len += jt808_pack_array( buf + len, &fcs, term_param.mobile, 6 );
	len += jt808_pack_int( buf + len, &fcs, tx_seq, 2 );

	len			+= jt808_pack_int( buf + len, &fcs, jt808_param.id_0x0081, 2 );
	len			+= jt808_pack_int( buf + len, &fcs, jt808_param.id_0x0082, 2 );
	len			+= jt808_pack_array( buf + len, &fcs, term_param.producer_id, 5 );
	len			+= jt808_pack_array( buf + len, &fcs, term_param.model, 20 );
	len			+= jt808_pack_array( buf + len, &fcs, term_param.terminal_id, 7 );
	len			+= jt808_pack_int( buf + len, &fcs, jt808_param.id_0x0084, 1 );
	len			+= jt808_pack_string( buf + len, &fcs, jt808_param.id_0x0083 );
	len			+= jt808_pack_byte( buf + len, &fcs, fcs );
	buf [0]		= 0x7e;
	buf [len]	= 0x7e;
	pdata		= rt_malloc( len + 1 );
	if( pdata == NULL )
	{
		rt_free( pnodedata );
		return;
	}
	rt_kprintf( "\r\n--------------------\r\n" );
	for( i = 0; i < len + 1; i++ )
	{
		rt_kprintf( "%02x ", buf [i] );
	}
	rt_kprintf( "\r\n--------------------\r\n" );
	memcpy( pdata, buf, len + 1 );
	pnodedata->msg_len	= len + 1;
	pnodedata->pmsg		= pdata;
	pnodedata->head_sn	= tx_seq;
	pnodedata->head_id	= 0x0100;
	msglist_append( list_jt808_tx, pnodedata );
	tx_seq++;
}

FINSH_FUNCTION_EXPORT( jt808_tx, jt808_tx test );

/************************************** The End Of File **************************************/

