按照正常步骤部署DPDK, 编译cache,
cache启动过程中出现以下信息

```
APP: Initialising port 0 ...
EAL: Error - exiting with code: 1
  Cause: Could not configure port0 (-22)
```

打开dpdk lib的PMD INIT DEBUG开关,
文件位置: dpdk/config/defconfig_x86_64-default-linuxapp-gcc  
重新编译dpdk lib, cache
启动, 显示详细信息
>
eth_ixgbe_dev_init(): Hardware Initialization Failure: -30

查看查找`-30`错误码, 意义为`IXGBE_ERR_SFP_SETUP_NOT_COMPLETE`

经查找, 能够返回该值的只有`ixgbe_reset_pipeline_82599`函数.  
gdb调试, 加改函数断点, 没有符号表

`export EXTRA_CFLAGS="-O0 -g"` 重新编译dpdk lib

重新编译cache, bp ixgbe_reset_pipeline_82599,  
单步跟踪, 无错误发生,  
怀疑和下面步骤中的延时有关

```
for (i = 0; i < 10; i++) {
    msec_delay(4);
    anlp1_reg = IXGBE_READ_REG(hw, IXGBE_ANLP1);
    if (anlp1_reg & IXGBE_ANLP1_AN_STATE_MASK)
        break;
}
```

将每次查询寄存器的间隔延长是40msec, 重新编译dpdk lib, cache
启动成功

```
EAL: PCI device 0000:03:00.0 on NUMA socket 0
EAL:   probe driver: 8086:1528 rte_ixgbe_pmd
EAL:   0000:03:00.0 not managed by UIO driver, skipping
EAL: PCI device 0000:03:00.1 on NUMA socket 0
EAL:   probe driver: 8086:1528 rte_ixgbe_pmd
EAL:   0000:03:00.1 not managed by UIO driver, skipping
EAL: PCI device 0000:05:00.0 on NUMA socket 0
EAL:   probe driver: 8086:10fb rte_ixgbe_pmd
EAL:   PCI memory mapped at 0x7f73f7d8f000
EAL:   PCI memory mapped at 0x7f73f7e55000
EAL: PCI device 0000:05:00.1 on NUMA socket 0
EAL:   probe driver: 8086:10fb rte_ixgbe_pmd
EAL:   0000:05:00.1 not managed by UIO driver, skipping
APP: Initialising port 0 ...
KNI: pci: 05:00:00 	 8086:10fb
load new region 343891
load vserver_new 313
```
可见,启动失败主要是由于读取网卡寄存器时等待时间过短造成的.
查看`msec_delay`函数,是在EAL中实现, 

```
#define msec_delay(x) DELAY(1000*(x))
#define DELAY(x) rte_delay_us(x)
void
rte_delay_us(unsigned us)
{
	const uint64_t start = rte_get_timer_cycles();
	const uint64_t ticks = (uint64_t)us * rte_get_timer_hz() / 1E6;
	while ((rte_get_timer_cycles() - start) < ticks)
		rte_pause();
}
static inline uint64_t
rte_get_timer_cycles(void)
{
	switch(eal_timer_source) {
	case EAL_TIMER_TSC:
		return rte_rdtsc();
	case EAL_TIMER_HPET:
#ifdef RTE_LIBEAL_USE_HPET
		return rte_get_hpet_cycles();
#endif
	default: rte_panic("Invalid timer source specified\n");
	}
}

```

可以看到此函数和系统的时钟强相关.  
关于系统时钟 [介绍](http://www.ibm.com/developerworks/cn/linux/l-cn-timerm/)  
另, 也有可能是:服务器的两个CPU对应不同的PCI-E通道,如果跨CPU访问的话,可能造成访问速度下降, 导致不能在那么短的时间内读取寄存器.    
具体原因等有时间再进一步分析
