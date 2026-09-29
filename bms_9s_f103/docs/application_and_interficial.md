## 功能分析和分层设计

### 裸机实现

#### 功能0 初始化bq

目标：控制iic发送初始化bq；设置SYS_CTRL寄存器位；以及检查当前通讯状态是否正常；引脚唤醒bq；以及后续的bq状态获取；设置保护触发的阈值；设置cc_config寄存器 0x19

需求：iic

识别概念：//LOAD_PRESENT当前是否有负载 //ADC使能OV和温度 //TEMP_SEL外部内部温度电阻选择 //DELAY_DIS延迟设置 //CC_EN库伦计数器连续读取使能 //CC_ONESHOT库伦计数器单次采样使能 //CHG_ON DSG_ON当前充放电

数据类型：

```c
typedef struct{
    bool adc_en,
    bool temp_sel,
    bool delay_dis,
    bool cc_en,
    bool cc_oneshot，
    uint8_t ov_shold,
    ...tbc
} bq_init_t;
bq_init_t bq_init{
    .adc_en =...tbc
};
//当前bq状态机，设备状态机还未设置
//使用枚举需要确保状态机是互斥的，那么没有实际意义
typedef enum{
    bms_new 0x00,
    bms_inited 0x01,	//成功创建并且iic通信没有异常
    bms_erri2c 0x02,
    //bms_errcan 0x04,	//can是bms的
    bms_errbq 0x08,		//具体错误需要查看后续protect的
    bms_load 0x10,
    bms_unload 0x20,
    bms_chg 0x40,
    bms_dsg 0x80
}bms_sta_t;
bms_sta_t g_bms_sta;

uint8_t sa_bms_sta;
#define BMS_STA_ERRI2C_POS `1
#define BMS_STA_ERRCAN_POS	2
#define BMS_STA_LOAD		3
#define BMS_STA_CHG			4
#define BMS_STA_DSG			5

uint8_t g_fault;
#define BMS_BQ_OV			1
#define BMS_BQ_UV			2
#define BMS_BQ_SCD			3
#define BMS_BQ_OCD			4
#define BMS_BQ_INR			5

typedef enum{
    FAILTH=0,
    SUCCEES
}result_t;

//一下是subapp流程，不是数据类型和API定义该有的部分
void bsp_bq_init(&bq_init_t);	//创建状态机，传递指针是减小消耗
void bsp_bq_wake(void); //引脚唤醒
result_t bsp_bq_testi2c(void);	//检查iic通信并且对返回值操作
void bsp_bq_getstatus(void);	//首次通过iic获取当前设备状态并存储进事件组
void subapp_setfault(const uint8_t *raw,uint8_t * g_fault);		//raw[]和g_fault是全局变量，不需要get
//根据返回值设置状态机
```

#### 功能1 硬件定时器触发采样

目标：硬件定时器触发标志位改变，taskSample检测到标志位修改，通过iic采样，采样包括数据和当前bq的状态，采样放进共享数组raw[]，并且设置标志位通知其他任务数组数据有效；SYS_CTRL1的LOAD是表示当前是否有设备负载在BMS系统上，但是这个检测是只有当CHG_ON==0的时候才生效，是因为当chg引脚是充电状态无法判断

需求：tim2中断 iic

数据类型：

```c
//直接从bq读取的寄存器状态
//uint8_t sa_raw[24];//8bit SYS_STA 8bit*2*9 VCHI VCLO 8*2 BATHI BATLO整体电压 8*2 CCHI CCLO 8 SYS_CTRL1 
//uint8_t g_sampled=0;
typedef struct{
    uint8_t sys_sta_r;
    uint16_t vc_r[9];
    uint16_t bat_r;
    uint16_t cc_r;
    uint16_t ts_r;
    uint8_t sys_trcl1_r;
    bool sampled;
}raw_t;
raw_t raw;
void app_sample(void);		//post:g_sampled=1;	progress:raw[]和g_fault判断
```

#### 功能2 软件保护

目标：在bq有毫秒级硬件保护的基础上，读取raw[]，添加软件保护通知上位机；保护恢复：通过proc[]读取电压电流值判断是否恢复，连续三次读取都恢复则上报正常，并且重启fet；sys_sta 的标志位只能host清理

需求：can iic

原理：sys_sta 的状态只能host 清理；电流保护查表，电压保护反推当时的adc值；保护自身有硬件delay

数据类型：

```c
//需要读取raw[0]，判断当前是否有故障状态；返回对应状态标志位
typedef enum{
    fault_uv 0x08,
    fault_ov 0x04,
    fault_scd 0x02,
    fault_ocd 0x01,
    fault_dev 0x20,
    fault_no_or_resumed 0x00
} fault_sta_t;
fault_sta_t ex_fault_sta;
void bsp_protect_judge(uint8_t raw[0],const uint8_t flag_sampled,fault_sta_t*ex_fault_sta);//读取的低四位，同时判断是否已采样；需要注意可能不止一个标志位故障，需要判断优先级，dev cd u 
//实现对应保护
void bsp_protect_process(fault_sta_t);//根据对应标志位执行对应控制mos充放电
typedef uint8_t fault_cnt_t;
static fault_cnt_t prvb_fault_cnt=0;//限制在app_protect文件中
//先if判断是否处于故障
void bsp_protect_resume(const uint8_t* poc);//根据计算值是否在阈值内判断是否已经恢复，如果已经恢复写对应位1
void app_protect(const uint8_t flag_sampled,fault_sta_t*,uint8_t raw[0]);

///////////////////////////////////////////以上作为subapp实现流程，以及数据类型和API定义的反例//////////
uint16_t g_proc[12];		//存放手计算之后处理的数据
//计算之后的数据是16*9单节电压 16*1总电压 16*1电流 16*1温度 
uint8_t s_resume_cnt=0;
void app_protect(void);	//pre:g_fault!=0;	post:g_fault=0;	bsp_iic_write()写入1清理标志
```

#### 功能3 计算

目标：先读取gain 和 offset 用于计算电压大小，而电流是直接库伦计数器*系数；如果当前处于故障状态要考虑恢复故障；从raw中读取数据处理之后放进新全局数组proc[]，设标志位有效

需求：iic

数据类型：

```c
uint16_t s_gain_map[32];
uint16_t s_gain;
uint8_t s_offset;
//uint16_t s_temp;		//ADC原始值通过温度电阻关系得到实际温度
//uint16_t s_cc_raw;		//即cc寄存器的原始值
//uint16_t s_cc;			//cc*LSB/Rsense，是存储在proc[]里面的

//uint16_t g_proc[12];
//bool s_computed=0;
typedef struct{
    uint16_t vc[9];
    uint16_t bat;
    uint16_t cc;
    uint16_t ts;
    bool computed;
}

void app_getGainAndOffset(void);	//
void app_compute(void);	//pre:g_samppled==1;	post:s_computed=1;	g_proc[];
```

#### 功能4 均衡

目标：从proc[]中读取，如果最大压差超阈值则进行被动均衡，放电处理

需求：iic

数据类型：

```c
uint16_t s_vc_even;
bsp_iic_write_cellbal();
```

#### 功能5 SOC计算

目标：从proc[]中读取，根据已知表格查表实现，二分法查找确定，写回proc[]

需求：iic

数据类型：

```c
```



#### 功能6 通讯RX

目标：将SOC等数据通过CAN发送

需求：can

原理：守门员任务控制一个队列，直接控制底层外设

#### 功能7 通讯TX

目标：查看邮箱，中断触发

需求：can