#ifndef _H_JT808_H_
#define _H_JT808_H_

#include <stm32f4xx.h>

#define   MsgQ_Timeout            3




/*�ֽ�˳��Ķ�������˳��*/
typedef struct
{
	uint32_t latitude; 		/*γ�� 1/10000�� */
	uint32_t longitude; 	/*���� 1/10000�� */
	uint16_t altitude;		/*�߳� m*/
	uint16_t speed;			/*�ٶ� 1/10KMH*/
	uint8_t direction;		/*���� 0-178 �̶�Ϊ2��*/
	uint8_t datetime[6];	/*YY-MM-DD hh-mm-ss BCD����*/
}T_GPSINFO;


//------- �ı���Ϣ --------
typedef struct _TEXT_INFO
{
  uint8_t  TEXT_FLAG;          //  �ı���־
  uint8_t  TEXT_SD_FLAG;       // ���ͱ�־λ  
  uint8_t  TEXT_Content[100];  // �ı�����
}TEXT_INFO;


//----- ��Ϣ ----
typedef struct _MSG_TEXT
{
  uint8_t   TEXT_mOld;     //  ���µ�һ����Ϣ  дΪ1���������µ�һ����Ϣ
  uint8_t   TEXT_TYPE;     //  ��Ϣ����   1-8  �еڼ���
  uint8_t   TEXT_LEN;      //  ��Ϣ����    
  uint8_t   TEXT_STR[150]; //  ��Ϣ����
}MSG_TEXT;

//-----  ���� ------
typedef struct _CENTER_ASK
{
  uint8_t  ASK_SdFlag; //  ��־λ           ���� TTS  1  ��   TTS ����  2
  uint16_t ASK_floatID; // ������ˮ��
  uint8_t  ASK_infolen;// ��Ϣ����  
  uint8_t  ASK_answerID;    // �ظ�ID
  uint8_t  ASK_info[30];//  ��Ϣ����
  uint8_t  ASK_answer[30];  // ��ѡ��  
}CENTRE_ASK;

//------ �¼�  -------
typedef struct _EVENT               //  name: event
{
  uint8_t Event_ID;   //  �¼�ID
  uint8_t Event_Len;  //  �¼�����
  uint8_t Event_Effective; //  �¼��Ƿ���Ч��   1 ΪҪ��ʾ  0     
  uint8_t Event_Str[20];  //  �¼�����
}EVENT; 


//----- ��Ϣ ----
typedef struct _MSG_BROADCAST    // name: msg_broadcast
{
  uint8_t   INFO_TYPE;     //  ��Ϣ����
  uint16_t  INFO_LEN;      //  ��Ϣ����
  uint8_t   INFO_PlyCancel; // �㲥/ȡ����־      0 ȡ��  1  �㲥
  uint8_t   INFO_SDFlag;    //  ���ͱ�־λ
  uint8_t   INFO_Effective; //  ��ʾ�Ƿ���Ч   1 ��ʾ��Ч    0  ��ʾ��Ч     
  uint8_t   INFO_STR[30];  //  ��Ϣ����
}MSG_BRODCAST;

//------ �绰�� -----
typedef struct _PHONE_BOOK            // name: phonebook
{
  uint8_t CALL_TYPE ;    // ��������  1 ���� 2 ���� 3 ����/����
  uint8_t NumLen;        // ���볤��   
  uint8_t UserLen;       // ��ϵ�˳���
  uint8_t Effective_Flag;// ��Ч��־λ   ��Ч 0 ����Ч  1
  uint8_t NumberStr[20]; // �绰����
  uint8_t UserStr[10];   // ��ϵ������  GBK ����
}PHONE_BOOK;


typedef struct _MULTIMEDIA
{
  u32  Media_ID;           //   ��ý������ID
  u8   Media_Type;         //   0:   ͼ��    1 : ��Ƶ    2:  ��Ƶ 
  u8   Media_CodeType;     //   �����ʽ  0 : JPEG  1:TIF  2:MP3  3:WAV  4: WMV
  u8   Event_Code;         //   �¼�����  0: ƽ̨�·�ָ��  1: ��ʱ����  2 : ���ٱ������� 3: ��ײ�෭�������� ��������
  u8   Media_Channel;      //   ͨ��ID
  //----------------------
  u8   SD_Eventstate;          // �����¼���Ϣ�ϴ�״̬    0 ��ʾ����   1  ��ʾ���ڷ���״̬        
  u8   SD_media_Flag;     // ����û���¼���Ϣ��־λ
  u8   SD_Data_Flag;      // �������ݱ�־λ
  u8   SD_timer;          // ���Ͷ�ʱ��
  u8   MaxSd_counter;   // ����ʹ���  
  u8   Media_transmittingFlag;  // ��ý�崫������״̬  1: ��ý�崫��ǰ����1����λ��Ϣ    2 :��ý�����ݴ�����  0:  δ���ж�ý�����ݴ���
  u16  Media_totalPacketNum;    // ��ý���ܰ��� 
  u16  Media_currentPacketNum;  // ��ý�嵱ǰ���� 
  //----------------------
  u8   RSD_State;     //  �ش�״̬   0 : �ش�û������   1 :  �ش���ʼ    2  : ��ʾ˳���굫�ǻ�û�յ����ĵ��ش�����
  u8   RSD_Timer;     //  ��״̬�µļ�����   
  u8   RSD_Reader;    //  �ش���������ǰ��ֵ 
  u8   RSD_total;     //  �ش�ѡ����Ŀ  
  
   
  u8	Media_ReSdList[10]; //  ��ý���ش���Ϣ�б� 
}MULTIMEDIA;   




void gps_rx(uint8_t *pinfo,uint16_t length);
void gprs_rx(uint8_t *pinfo,uint16_t length);


//--------------ÿ���ӵ�ƽ���ٶ� 
typedef  struct AvrgMintSpeed
{
  uint8_t datetime[6]; //current
  uint8_t datetime_Bak[6];//Bak
  uint8_t avgrspd[60];
  uint8_t saveFlag;  
}Avrg_MintSpeed;


extern uint32_t jt808_alarm;
extern uint32_t jt808_status;


extern TEXT_INFO TextInfo;
//-------�ı���Ϣ-------
extern MSG_TEXT       TEXT_Obj;
extern MSG_TEXT       TEXT_Obj_8[8],TEXT_Obj_8bak[8];

//------ ����  --------
extern CENTRE_ASK     ASK_Centre;  // ��������

//------- �¼� ----
extern EVENT          EventObj;    // �¼�   
extern EVENT          EventObj_8[8]; // �¼�  

//------  ��Ϣ�㲥  ---
extern MSG_BRODCAST   MSG_BroadCast_Obj;    // ��Ϣ�㲥         
extern MSG_BRODCAST   MSG_Obj_8[8];  // ��Ϣ�㲥    

//------  �绰��  -----
extern PHONE_BOOK    PhoneBook,Rx_PhoneBOOK;   //  �绰��
extern PHONE_BOOK    PhoneBook_8[8];


extern MULTIMEDIA   MediaObj;      // ��ý����Ϣ 

extern uint8_t  CarLoadState_Flag;//ѡ�г���״̬�ı�־   1:�ճ�   2:���   3:�س�
extern uint8_t		Warn_Status[4];

extern u16      ISP_total_packnum;  // ISP  �ܰ���
extern u16      ISP_current_packnum;// ISP  ��ǰ����

extern u8          APN_String[30];

extern u8  Camera_Number;
extern u8	Duomeiti_sdFlag; 

extern Avrg_MintSpeed  Avrgspd_Mint; 
extern u8          avgspd_Mint_Wr;       // ��дÿ����ƽ���ٶȼ�¼�±�




/*for new use*/

typedef struct
{
	int id;
	short attr;
	int latitute;	/*�Զ�λ��λ��γ��ֵ����10��6�η�����ȷ�������֮һ��*/
	int longitute;
	int radius;/*��λΪ��m��·��Ϊ�ùյ㵽��һ�յ�*/
	char start[6];
	char end[6];
	short speed;
	char interval; /*����ʱ��,��*/
}GPS_AREA_CIRCLE;




typedef struct
{
	uint32_t ver;	/*�汾��Ϣ�ĸ��ֽ�yy_mm_dd_build,�Ƚϴ�С*/
/*������Ϣ*/

	
/*�����й�*/
	char	apn[32];
	char	user[32];
	char	psw[32];



	




}JT808_PARAM;


extern JT808_PARAM jt808_param;















#endif

