#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/jiffies.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

struct vnet_priv {
    struct net_device_stats stats;
};

static struct net_device *vnet_dev;

//
// virtual net interface
// 

static int vnet_open(struct net_device *dev)
{
    memcpy(dev->dev_addr, "\x12\x34\x56\x78\x9A\xBC", ETH_ALEN);
    netif_start_queue(dev);
    printk(KERN_INFO "vnet_open\n");
    return 0;
}

static int vnet_release(struct net_device *dev)
{
    netif_stop_queue(dev);
    printk(KERN_INFO "vnet_release\n");
    return 0;
}

static int vnet_tx(struct sk_buff *skb, struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);
    
    skb_tx_timestamp(skb);

    skb->protocol = eth_type_trans(skb, dev);
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    dev_kfree_skb(skb); // free skb
    return NETDEV_TX_OK;
}

static void vnet_tx_timeout(struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);

    priv->stats.tx_errors++;
    netif_wake_queue(dev);
    printk(KERN_DEBUG "Transmit timeout at %ld\n", jiffies);
}

static struct net_device_stats *vnet_stats(struct net_device *dev)
{
    struct vnet_priv *priv = netdev_priv(dev);
    return &priv->stats;
}

static int vnet_config(struct net_device *dev, struct ifmap *map)
{
    if (dev->flags & IFF_UP) /* can't act on a running interface */
        return -EBUSY;

    /* Don't allow changing the I/O address */
    if (map->base_addr != dev->base_addr) {
        printk(KERN_WARNING "netmirror: Can't change I/O address\n");
        return -EOPNOTSUPP;
    }
    /* ignore other fields */
    return 0;
}

static int vnet_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
    printk(KERN_DEBUG "ioctl %d\n", cmd);
    return 0;
}

static const struct net_device_ops vnet_netdev_ops = {
    .ndo_open            = vnet_open,
    .ndo_stop            = vnet_release,
    .ndo_start_xmit      = vnet_tx,
    .ndo_tx_timeout      = vnet_tx_timeout,
    .ndo_get_stats       = vnet_stats,
    .ndo_set_config      = vnet_config,
    .ndo_do_ioctl        = vnet_ioctl
};

static u32 always_on(struct net_device *dev)
{
    return 1;
}

static const struct ethtool_ops vnet_ethtool_ops = {
    .get_link = always_on
};

static void vnet_setup(struct net_device *dev)
{
    struct vnet_priv *priv;

    ether_setup(dev);
    dev->watchdog_timeo = 5; // jiffies
    dev->ethtool_ops = &vnet_ethtool_ops;
    dev->netdev_ops = &vnet_netdev_ops;

    dev->flags |= IFF_NOARP;
    dev->features |= NETIF_F_HW_CSUM;

    priv = netdev_priv(dev);
    memset(priv, 0, sizeof(struct vnet_priv));
}

//
// Mirror
//

static int mirror_fn(struct sk_buff *skb, struct net_device *dev,
        struct packet_type *pt, struct net_device *orig_dev)
{
    struct sk_buff *nskb;

    if (skb_shared(skb) && (dev != vnet_dev)) {
        // can't use skb_clone, because panic on namespace
        nskb = skb_copy(skb, GFP_ATOMIC);
        if (!nskb)
            goto out;
        nskb->dev = vnet_dev;
        nskb->len += nskb->mac_len;
        nskb->data -= nskb->mac_len;
        dev_queue_xmit(nskb);
    }
out:
    kfree_skb(skb);
    return NET_RX_SUCCESS;
}

static struct packet_type mirror_proto = {
    .type = __constant_htons(ETH_P_ALL),
    .func = mirror_fn,
};

//
// init & exit
//

static int netmirror_init(void)
{
    int err;

    // vnet
    vnet_dev = alloc_netdev(sizeof(struct vnet_priv), "netmirror", 
        NET_NAME_UNKNOWN, vnet_setup);
    if (!vnet_dev) {
        printk(KERN_ERR "alloc netmirror memory error\n");
        return -ENOMEM;
    }

    err = register_netdev(vnet_dev);
    if (err) {
        printk(KERN_ERR "registering device error %d\n", err);
        free_netdev(vnet_dev);
        return err;
    }
    // mirror
    dev_add_pack(&mirror_proto);
    printk(KERN_INFO "netmirror started!\n");
    return 0;
}

static void netmirror_exit(void)
{
    dev_remove_pack(&mirror_proto);
    unregister_netdev(vnet_dev);
    free_netdev(vnet_dev);
    printk(KERN_INFO "netmirror exited!\n");
}

module_init(netmirror_init);
module_exit(netmirror_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("elemeta, elemeta@foxmail.com");
MODULE_VERSION("0.1.0");

