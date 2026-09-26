## 功能分析和分层设计

### 裸机实现

#### 功能0 初始化bq

目标：控制iic发送初始化bq；设置SYS_CTRL寄存器位

需求：iic

数据类型：

```c
void bsp_
```

#### 功能1 硬件定时器触发采样

目标：硬件定时器触发标志位改变，taskSample检测到标志位修改，通过iic采样，采样包括数据和当前bq的状态，采样放进共享数组raw[]，并且设置标志位通知其他任务数组数据有效；SYS_CTRL1的LOAD是表示当前是否有设备负载在BMS系统上，但是这个检测是只有当CHG_ON==0的时候才生效，是因为当chg引脚是充电状态无法判断

需求：tim2中断 iic

数据类型：

```c
//直接从bq读取的寄存器状态
uint8_t ex_raw[24];//8bit SYS_STA 8bit*2*9 VCHI VCLO 8*2 BATHI BATLO整体电压 8*2 CCHI CCLO 8 SYS_CTRL1
void bsp_sample(uint8_t *);
uint8_t ex_flag_sampled;
void app_sample(uint8_t*raw,uint8_t flag);
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
```

#### 功能3 计算

目标：先读取gain 和 offset 用于计算电压大小，而电流是直接库伦计数器*系数；如果当前处于故障状态要考虑恢复故障；从raw中读取数据处理之后放进新全局数组proc[]，设标志位有效

需求：iic

数据类型：

```c
uint8_t 
```



#### 功能4 均衡

目标：从proc[]中读取，如果最大压差超阈值则进行被动均衡，放电处理

需求：iic

#### 功能5 SOC计算

目标：从proc[]中读取，根据已知表格查表实现，二分法查找确定，写回proc[]

需求：iic

#### 功能6 通讯RX

目标：将SOC等数据通过CAN发送

需求：can

原理：守门员任务控制一个队列，直接控制底层外设

#### 功能7 通讯TX

目标：查看邮箱，中断触发

需求：can