####DPDK线上调试记录

#####前期工作
* 上线机器
* 流量镜像
* 交换机下发规则保证dpdk的应答drop(以免影响线上服务)
* 服务器下发规则`route add default dev vEth0_0`
*

#####运行
配置DPDK以及各种前期需要做的配置,参考前面的记录

#####问题
1. Cache接收不到流量  
由于线上流量都是镜像过来的,所以目的MAC不是本服务器网卡的MAC  
为了解决以上问题,需要网卡混杂模式启动  

    ```
rte_eth_promiscuous_enable(port);
```
2. Cache没有应答  
gdb跟踪发现能够收到报文,但是将请求透传到虚拟网卡的时候drop掉了.所以虚拟网卡同样遇到MAC地址的问题,并且IP也有问题.  
解决方法:
	* 修改虚拟网卡IP为线上服务IP  
	`ifconfig vEth0_0 202.106.184.166 netmask 255.255.255.0`
	* 修改虚拟网卡MAC,发现支持.  
	`vim lib/librte_eal/linuxapp/kni/kni_net.c`  
	添加修改MAC地址的支持(1.5.1的DPDK不支持,查看maillist,到1.7.1才会支持上)  
	
      ```
      static int kni_net_set_mac(struct net_device *netdev, void *p)
      {
         struct sockaddr *addr = p;
         if (!is_valid_ether_addr((unsigned char *)(addr->sa_data)))
             return -EADDRNOTAVAIL;
         memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
         return 0;
      }
      
      static const struct net_device_ops kni_net_netdev_ops = {
      ...
          .ndo_set_mac_address = kni_net_set_mac,
      };
      
      ```
	重新编译DPDK,重新LOAD KNI module
