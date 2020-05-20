#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include "stego.h"

/** Discription of module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("NEMKOV NIKITA");
MODULE_VERSION("0.1");

struct counters *get_client_by_ip(int client_ip) {

	struct counters *ret_ptr = NULL;
	struct counters *iter_ptr = NULL;

	if(list_empty(&clients))
		return ret_ptr;

	list_for_each_entry(iter_ptr, &clients, list) {
		if(iter_ptr->ip == client_ip) {
			ret_ptr = iter_ptr;
			break;
		}
	}

	return ret_ptr;

}

struct counters *get_client_by_num(int num) {
	struct counters *iter_ptr = NULL;
	struct counters *ret_ptr = NULL;

	if(num == 0)
		return ret_ptr;

	list_for_each_entry(iter_ptr, &clients, list) {
		if(num == 1)
			break;
		else
			num--;
	}

	if(num == 1 && iter_ptr->ip)
		ret_ptr = iter_ptr;

	return ret_ptr;
}


int list_len(void) {
	int ret_len = 0;
	struct counters *iter_ptr = NULL;

	list_for_each_entry(iter_ptr, &clients, list) {
		if(iter_ptr->ip)
			ret_len++;
	}

	return ret_len;
}


netdev_tx_t stg_xmit(struct sk_buff *skb, struct net_device *dev) {

	struct iphdr *ip_header = NULL;
	struct tcphdr *tcp_header = NULL;
	struct counters *cnt = NULL;
	__u16 packet_port = 0;

	ip_header = ip_hdr(skb);
	tcp_header = tcp_hdr(skb);

	if(ip_header->protocol == PROTOCOL) {
		packet_port = ((__u16)tcp_header->source << CHAR_LEN) | ((__u16)tcp_header->source >> CHAR_LEN);

		if(packet_port == PORT) {

			if((cnt = get_client_by_ip(ip_header->daddr)) == NULL) {

				if(list_len() == NCLIENTS) {
					printk(KERN_INFO "\n[STG INFO] === Maximum number of clients reached! ===\n");
					return origin_net_ops->ndo_start_xmit(skb, netdev);
				}

				cnt = kmalloc(sizeof(struct counters), GFP_ATOMIC);

				if(!cnt) {
					printk(KERN_ERR "\n[STG ERR] === Cannot to allocate memory! ===\n");
					return origin_net_ops->ndo_start_xmit(skb, netdev);
				}
				
				cnt->ip = ip_header->daddr;
				cnt->repeat_count = 0;
				cnt->group_count = 0;
				cnt->global_offset = 0;
				cnt->local_offset = 0;
				list_add_tail(&(cnt->list), &clients);
				printk(\
						KERN_INFO \
						"\n[STG INFO] === Adding new client: %d.%d.%d.%d ===\n",\
						cnt->ip & IP_MASK,\
						(cnt->ip >> CHAR_LEN) & IP_MASK,\
						(cnt->ip >> CHAR_LEN * 2) & IP_MASK,\
						(cnt->ip >> CHAR_LEN * 3) & IP_MASK\
					  );

				return origin_net_ops->ndo_start_xmit(skb, netdev);
			}

			if(chardev->init_flag) {

				printk(\
						KERN_INFO \
						"\n[STG MSG] ===  Sending BYTE: %d, BIT: %d, GROUP NUM: %d, REPEAT NUM: %d, VALUE: %d to CLIENT: %d.%d.%d.%d  ===\n",\
						cnt->global_offset,\
						cnt->local_offset,\
						cnt->group_count,\
						cnt->repeat_count,\
						((chardev->buf[cnt->global_offset] << cnt->local_offset) & DATA_BIT_MASK) >> (CHAR_LEN - 1),\
						cnt->ip & IP_MASK,\
						(cnt->ip >> CHAR_LEN) & IP_MASK,\
						(cnt->ip >> CHAR_LEN * 2) & IP_MASK,\
						(cnt->ip >> CHAR_LEN * 3) & IP_MASK\
						 );

				if ((chardev->buf[cnt->global_offset]<<cnt->local_offset) & DATA_BIT_MASK) {
					mdelay(DELAY);
				}

				cnt->group_count = (cnt->group_count + 1) % GNUM;

				if(!cnt->group_count) {

					cnt->local_offset = (cnt->local_offset + 1) % CHAR_LEN;

					if(!cnt->local_offset) {

						cnt->global_offset = (cnt->global_offset + 1) % chardev->buf_len;

						if(!cnt->global_offset) {

							cnt->repeat_count = (cnt->repeat_count + 1) % REPEAT;

							if(!cnt->repeat_count) {
								list_del(&(cnt->list));
								kfree(cnt);
								if(list_empty(&clients)) {
									kfree(chardev->buf);
									chardev->buf = NULL;
									chardev->init_flag = 0;
								}
							}
						}
					}
				}
			}
			else {
				printk(KERN_INFO "\n[STG MSG] ===  No information to transfer  ===\n");
			}
		}
		else {
			printk(\
					KERN_INFO \
					"\n[STG INFO] ===  Skipping packet with PROTOCOL: %d and PORT: %d to CLIENT: %d.%d.%d.%d  ===\n",\
					ip_header->protocol,\
					packet_port,\
					ip_header->daddr & IP_MASK,\
					(ip_header->daddr>>CHAR_LEN) & IP_MASK,\
					(ip_header->daddr>>CHAR_LEN*2) & IP_MASK,\
					(ip_header->daddr>>CHAR_LEN*3) & IP_MASK\
				  );
		}
	}
	else {
		printk(KERN_INFO "\n[STG INFO] ===  Skipping packet with PROTOCOL: %d  ===\n", ip_header->protocol);
	}

	return origin_net_ops->ndo_start_xmit(skb, netdev);
}

static ssize_t stg_read(struct file *file, char *buf, size_t count, loff_t *ppos) {
	
	int len;
	char stat[MAX_LEN_STATE];
	struct counters *cnt = NULL;

	if((*ppos == NCLIENTS + 1) || (((cnt = get_client_by_num((__u64) *ppos)) == NULL) && (*ppos != 0)))
		return 0;

	if(chardev->init_flag) {
		if(list_empty(&clients)) {
			sprintf(\
					 stat,\
					 "%s\nNo clients connected yet.\n",\
					 proc_msg\
				   );

			*ppos = NCLIENTS + 1;
		}
		else {
			if(*ppos == 0)
				sprintf(stat, "%s", proc_msg);
			else
				sprintf(\
						 stat,\
					 	"\nCLIENT: %d.%d.%d.%d\nGROUP NUM: %d\nBYTE: %d\nBIT: %d\nREPEAT COUNTER: %d\n",\
					 	cnt->ip & IP_MASK,\
					 	(cnt->ip>>CHAR_LEN) & IP_MASK,\
					 	(cnt->ip>>CHAR_LEN*2) & IP_MASK,\
					 	(cnt->ip>>CHAR_LEN*3) & IP_MASK,\
					 	cnt->group_count,\
					 	cnt->global_offset,\
					 	cnt->local_offset,\
					 	cnt->repeat_count\
				   	   );
			(*ppos)++;
		}
	}
	else {
		*ppos = NCLIENTS + 1;
	}

	len = strlen(chardev->init_flag?stat:wellcome_msg);

	if(count < len)
		return -EINVAL;

	if(copy_to_user(buf, chardev->init_flag?stat:wellcome_msg, len))
		return -EINVAL;

	printk(KERN_INFO "\n[STG READ] ===  Was read: %d bytes  ===\n", len);

	return len;
}

static ssize_t stg_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {

	int res, len = count;

	if(chardev->init_flag)
		return -EBUSY;

	chardev->buf = kmalloc(len * sizeof(char), GFP_ATOMIC);
	if(!chardev->buf)
		return -ENOSPC;

	chardev->buf_len = len;
	res = copy_from_user(chardev->buf, (void*)buf, len);
	
	chardev->init_flag = 1;

	return len;
}

static int __init stg_init(void) {
	
	int ret;

	printk(\
			KERN_INFO \
		    "\n[STG INIT] ===  Hello, stego module was launch with DEVNAME=%s, INTERFACE=%s, PROTOCOL=%d, PORT=%d, GROUP NUM=%d, DELAY=%d, REPEAT=%d and CLIENTS NUM=%d  ===\n", \
		    DEVNAME, \
		    INTERFACE, \
		    PROTOCOL, \
		    PORT, \
		    GNUM, \
		    DELAY, \
		    REPEAT, \
		    NCLIENTS
		  );

	netdev = first_net_device(&init_net);
	while(netdev) {
		if(strcmp(netdev->name, INTERFACE))	
			netdev = next_net_device(netdev);
		else
			break;
	}

	if(netdev == NULL) {
		printk(KERN_ERR "\n[STG ERR] ===  Interface %s not found!  ===\n", INTERFACE);
		return -EINVAL;
	}

	printk(KERN_INFO "\n[STG INIT] ===  Net device successfully found. Name: %s  ===\n", netdev->name);

	origin_net_ops = netdev->netdev_ops;
	stg_net_ops = *(origin_net_ops);
	stg_net_ops.ndo_start_xmit = stg_xmit;
	netdev->netdev_ops = &stg_net_ops;

	ret = misc_register(&stg_dev);
	if(ret) {
		printk(KERN_ERR "\n[STG ERR] ===  Unable to register %s device!  ===\n", DEVNAME);
		return ret;
	}

	printk(KERN_INFO "\n[STG INIT] ===  Char device %s has been initialized  ===\n", DEVNAME);

	return 0;
}

static void __exit stg_exit(void) {
		
	struct counters *iter_ptr = NULL;
	struct counters *del_obj = NULL;
	int list_size = list_len();

	iter_ptr = list_first_entry(&clients, typeof(*iter_ptr), list);

	while(list_size) {
		if(del_obj && del_obj->ip)
			kfree(del_obj);

		del_obj = iter_ptr;
		iter_ptr = list_next_entry(iter_ptr, list);
		if(del_obj->ip)
			list_del(&(del_obj->list));
			list_size--;
	}

	netdev->netdev_ops = origin_net_ops;
	printk(KERN_INFO "\n[STG EXIT] ===  The original struct netdev_ops has been restored  ===\n");
	misc_deregister(&stg_dev);
	printk(KERN_INFO "\n[STG EXIT] ===  Char device %s has been deregistered  ===\n", DEVNAME);
	printk(KERN_INFO "\n[STG EXIT] ===  Stego module was unloaded. Goodbye  ===\n");
}

module_init(stg_init);
module_exit(stg_exit);
