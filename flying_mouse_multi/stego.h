#define DEVNAME "stg"
#define CHAR_LEN 0x08
#define MAX_LEN_STATE 0x400
#define DEFAULT_INTERFACE "ens33"
#define DEFAULT_GROUP_NUMBER 0x03
#define DEFAULT_PORT 0x50
#define DEFAULT_PROTOCOL 0x06
#define DEFAULT_REPEAT_NUMBER 0x01
#define DEFAULT_DELAY 0x100
#define DATA_BIT_MASK 0x80
#define IP_MASK 0xFF
#define DEFAULT_CLIENTS 0x40


/** Definition of module parameters */
static char *INTERFACE = DEFAULT_INTERFACE;
module_param(INTERFACE, charp, S_IRUGO);

static int PORT = DEFAULT_PORT;
module_param(PORT, int, S_IRUGO);

static int PROTOCOL = DEFAULT_PROTOCOL;
module_param(PROTOCOL, int, S_IRUGO);

static int GNUM = DEFAULT_GROUP_NUMBER;
module_param(GNUM, int, S_IRUGO);

static int REPEAT = DEFAULT_REPEAT_NUMBER;
module_param(REPEAT, int, S_IRUGO);

static int DELAY = DEFAULT_DELAY;
module_param(DELAY, int, S_IRUGO);

static int NCLIENTS = DEFAULT_CLIENTS;
module_param(NCLIENTS, int, S_IRUGO);

/** Prototypes of used functions */
struct counters *get_client_by_ip(int);
struct counters *get_client_by_num(int);
int list_len(void);
netdev_tx_t stg_xmit(struct sk_buff *, struct net_device *);
static ssize_t stg_read(struct file *, char *, size_t, loff_t *);
static ssize_t stg_write(struct file *, const char *, size_t, loff_t *);
static int __init stg_init(void);
static void __exit stg_exit(void);


/** Definition of global structures and variables */
LIST_HEAD(clients);

struct counters {
	int ip;
	int repeat_count;
	int group_count;
	int global_offset;
	char local_offset;
	struct list_head list;
};

static struct private {
	char *buf;
	int buf_len;
	char init_flag;
} stgblock = {
	.buf = NULL,
	.buf_len = 0,
	.init_flag = 0,
};

static struct file_operations stg_fops = {
	.owner = THIS_MODULE,
	.read = stg_read,
	.write = stg_write,
};

static struct miscdevice stg_dev = {
	MISC_DYNAMIC_MINOR,
	DEVNAME,
	&stg_fops,
};

struct net_device *netdev;
struct net_device_ops stg_net_ops;
const struct net_device_ops *origin_net_ops = NULL;
static struct private *chardev = &stgblock;
const char *wellcome_msg = "I'm ready.\n";
const char *proc_msg = "Sending data...\n";
