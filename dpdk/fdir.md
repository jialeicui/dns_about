####首先要确认网卡是否支持flow director
[82599 datasheet](http://www.intel.com/content/dam/www/public/us/en/documents/datasheets/82599-10-gbe-controller-datasheet.pdf)  

7.1.2.7 的介绍  
82599支持perfect-match的表项个数为8K

####使用dpdk自带的testpmd测试该功能
由于dpdk的example中没有fdir相关的例子,所以没有代码参考.  
查找[testpmd手册](http://www.dpdk.org/doc/intel/dpdk-testpmd-app-1.7.0.pdf)  
该程序支持dir相关的命令行配置  

* 启动:参数中要是能fdir, 例如`sudo ./testpmd -c 0x0f -n 2 -m 512 -- -i --portmask=0x3 --nb-cores=2 --pkt-filter-mode=signature --rxq=2 --txq=2`
* 配置参数: `help flowdir`可以显示相关的命令行, 参考帮助, 配置如下

 ```
add_signature_filter 0 ip src 2.1.1.2 22 dst 2.1.1.3 53 flexbytes 0 vlan 0 queue 1
set_masks_filter 0 only_ip_flow 0 src_mask 0xffffffff 0 dst_mask 0 0 flexbytes 0 vlan_id 0 vlan_prio 0
 ```
打入源ip为2.1.1.2的流量, 在testpmd中`show  port stats  all`, 全都是fdirmiss, 没有一个match, 多次更改配置, 无果.尝试其他的测试方法.

####使用原生驱动测试
搜索资料, [一篇文档](https://www.kernel.org/doc/Documentation/networking/ixgbe.txt) 中有如下描述

```
You can verify that the driver is using Flow Director by looking at the counter
in ethtool: fdir_miss and fdir_match.

Other ethtool Commands:
To enable Flow Director
	ethtool -K ethX ntuple on
To add a filter
	Use -U switch. e.g., ethtool -U ethX flow-type tcp4 src-ip 0x178000a
        action 1
To see the list of filters currently present:
	ethtool -u ethX
```

尝试

1. ethtool -K eth2 ntuple on
2. ethtool -u eth2  
36 RX rings available  
Total 0 rules  
3. ethtool -U eth2 flow-type ip4 src-ip 2.1.1.2 action 1  
Added rule with ID 2045
4. ethtool -u eth2  
36 RX rings available  
Total 1 rules

```
Filter: 2045  
	Rule Type: Raw IPv4  
	Src IP addr: 2.1.1.2 mask: 0.0.0.0
	Dest IP addr: 0.0.0.0 mask: 255.255.255.255
	TOS: 0x0 mask: 0xff
	Protocol: 0 mask: 0xff
	L4 bytes: 0x0 mask: 0xffffffff
	VLAN EtherType: 0x0 mask: 0xffff
	VLAN: 0x0 mask: 0xffff
	User-defined: 0x0 mask: 0xffffffffffffffff
	Action: Direct to queue 1
```
从2.1.1.2 ping本机, 查看, 已经生效:

```
ethtool -S eth2 | grep fdir
     fdir_match: 5
     fdir_miss: 0
```
####查看原生驱动与DPDK的区别
既然原生驱动生效, dpdk不生效, 那么比较一下两者的不同.  
查看一下原生驱动下发动作[ixgbe_set_rx_ntuple](http://lxr.free-electrons.com/source/drivers/net/ixgbe/ixgbe_ethtool.c?v=3.0#L2338)  
以及[dpdk中的处理](http://www.dpdk.org/browse/dpdk/tree/lib/librte_pmd_ixgbe/ixgbe_fdir.c?h=1.5.0)  
发现以下不同

原生驱动|testpmd中的下发  
-------|--------  
先下发mask|后下发
下发的rule和mask进行了与操作,再下发硬件|没有考虑
mask中的1代表忽略|mask中的1代表不忽略
ntuple使能是下发的perfect-match规则|下发的signature规则

对比完不同之后,写代码实现,并进行测试, 成功

```
port_conf.fdir_conf.mode = RTE_FDIR_MODE_PERFECT;
port_conf.fdir_conf.pballoc = RTE_FDIR_PBALLOC_64K;
port_conf.fdir_conf.status = RTE_FDIR_REPORT_STATUS;
port_conf.fdir_conf.flexbytes_offset = 0x6;
port_conf.fdir_conf.drop_queue = 127;
    
void init_fdir()
{
    int rv;

    struct rte_fdir_masks fdir_masks;
    memset(&fdir_masks, 0, sizeof(struct rte_fdir_masks));
    fdir_masks.only_ip_flow = 1;
    fdir_masks.src_ipv4_mask = -1;
    rv = rte_eth_dev_fdir_set_masks(0, &fdir_masks);
    if (rv != 0)
    {
        printf("set mask fail\n");
    }

    struct rte_fdir_filter fdir_filter;
    memset(&fdir_filter, 0, sizeof(struct rte_fdir_filter));
    fdir_filter.ip_src.ipv4_addr = ntohl(inet_addr("2.1.1.2"));
    rv = rte_eth_dev_fdir_add_perfect_filter(0, &fdir_filter, 0, 7, 0);
    if (rv != 0)
    {
        printf("set rule fail\n");
    }
}
```





